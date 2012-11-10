/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <glib.h>

#include <bluetooth/uuid.h>

#include "att.h"
#include "btio.h"
#include "gattrib.h"
#include "gatt.h"
#include "gatttool.h"

static GIOChannel *iochannel = NULL;
static GAttrib *attrib = NULL;
static GMainLoop *event_loop;
static GString *prompt;

#define INPUT_SIZE	100
static int didx = 0;
static gchar *inp = NULL;
static gchar *opt_src = NULL;
static gchar *opt_dst = NULL;
static gchar *opt_sec_level = NULL;
static int opt_psm = 0;
static int opt_mtu = 0;

struct characteristic_data {
	uint16_t orig_start;
	uint16_t start;
	uint16_t end;
	bt_uuid_t uuid;
};

static void cmd_help(int argcp, char **argvp);

enum state {
	STATE_DISCONNECTED,
	STATE_CONNECTING,
	STATE_CONNECTED
} conn_state;

static char *get_prompt(void)
{
	if (conn_state == STATE_CONNECTING) {
		g_string_assign(prompt, "Connecting... ");
		return prompt->str;
	}

	if (conn_state == STATE_CONNECTED)
		g_string_assign(prompt, "[CON]");
	else
		g_string_assign(prompt, "[   ]");

	if (opt_dst)
		g_string_append_printf(prompt, "[%17s]", opt_dst);
	else
		g_string_append_printf(prompt, "[%17s]", "");

	if (opt_psm)
		g_string_append(prompt, "[BR]");
	else
		g_string_append(prompt, "[LE]");

	g_string_append(prompt, "> ");

	return prompt->str;
}


static void set_state(enum state st)
{
	conn_state = st;
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
	uint8_t opdu[ATT_MAX_MTU];
	uint16_t handle, i, olen;

	handle = att_get_u16(&pdu[1]);

	printf("\r");
	switch (pdu[0]) {
	case ATT_OP_HANDLE_NOTIFY:
		printf("Notification handle = 0x%04x value: ", handle);
		break;
	case ATT_OP_HANDLE_IND:
		printf("Indication   handle = 0x%04x value: ", handle);
		break;
	default:
		printf("Invalid opcode\n");
		goto done;
	}

	for (i = 3; i < len; i++)
		printf("%02x ", pdu[i]);

	if (pdu[0] == ATT_OP_HANDLE_NOTIFY)
		goto done;

	olen = enc_confirmation(opdu, sizeof(opdu));

	if (olen > 0)
		g_attrib_send(attrib, 0, opdu[0], opdu, olen, NULL, NULL, NULL);

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
	bdaddr_t sba, dba;

	if (err) {
		printf("connect error: %s\n", err->message);
		set_state(STATE_DISCONNECTED);
		return;
	}

	if (!io) {
		printf("connect error io NULL\n");
		set_state(STATE_DISCONNECTED);
		return;
	}

	iochannel = io;
	attrib = g_attrib_new(iochannel);
	g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, events_handler,
							attrib, NULL);
	g_attrib_register(attrib, ATT_OP_HANDLE_IND, events_handler,
							attrib, NULL);

	/* LE connections share Client and Server paths */
	if (!opt_psm) {
		str2ba(opt_dst, &dba);
		str2ba(opt_src, &sba);
		attrib_server_attach(attrib, &sba, &dba, ATT_DEFAULT_LE_MTU);
	}

	set_state(STATE_CONNECTED);
}

static void primary_all_cb(GSList *services, guint8 status, gpointer user_data)
{
	GSList *l;

	if (status) {
		printf("Discover all primary services failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	printf("\r");
	for (l = services; l; l = l->next) {
		struct att_primary *prim = l->data;
		printf("attr handle: 0x%04x, end grp handle: 0x%04x "
			"uuid: %s\n", prim->start, prim->end, prim->uuid);
	}

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void primary_by_uuid_cb(GSList *ranges, guint8 status,
							gpointer user_data)
{
	GSList *l;

	if (status) {
		printf("Discover primary services by UUID failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	printf("\n");
	for (l = ranges; l; l = l->next) {
		struct att_range *range = l->data;
		g_print("Starting handle: 0x%04x Ending handle: 0x%04x\n",
						range->start, range->end);
	}

done:
	printf("\n%s", get_prompt());
	fflush(stdout);
}

static void char_cb(GSList *characteristics, guint8 status, gpointer user_data)
{
	GSList *l;

	if (status) {
		printf("Discover all characteristics failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	printf("\n");
	for (l = characteristics; l; l = l->next) {
		struct att_char *chars = l->data;

		printf("handle: 0x%04x, char properties: 0x%02x, char value "
				"handle: 0x%04x, uuid: %s\n", chars->handle,
				chars->properties, chars->value_handle,
				chars->uuid);
	}

done:
	printf("\n%s", get_prompt());
	fflush(stdout);
}

static void char_desc_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	struct att_data_list *list;
	guint8 format;
	int i;

	if (status != 0) {
		printf("Discover all characteristic descriptors failed: "
						"%s\n", att_ecode2str(status));
		goto done;
	}

	list = dec_find_info_resp(pdu, plen, &format);
	if (list == NULL)
		goto done;

	printf("\n");
	for (i = 0; i < list->num; i++) {
		char uuidstr[MAX_LEN_UUID_STR];
		uint16_t handle;
		uint8_t *value;
		bt_uuid_t uuid;

		value = list->data[i];
		handle = att_get_u16(value);

		if (format == 0x01)
			uuid = att_get_uuid16(&value[2]);
		else
			uuid = att_get_uuid128(&value[2]);

		bt_uuid_to_string(&uuid, uuidstr, MAX_LEN_UUID_STR);
		printf("handle: 0x%04x, uuid: %s\n", handle, uuidstr);
	}

	att_data_list_free(list);

done:
	printf("\n%s", get_prompt());
	fflush(stdout);
}

static void char_read_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	uint8_t value[ATT_MAX_MTU];
	int i, vlen;

	if (status != 0) {
		printf("Characteristic value/descriptor read failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	if (!dec_read_resp(pdu, plen, value, &vlen)) {
		printf("Protocol error\n");
		goto done;
	}

	printf("\nCharacteristic value/descriptor: ");
	for (i = 0; i < vlen; i++)
		printf("%02x ", value[i]);

done:
	printf("\n%s", get_prompt());
	fflush(stdout);
}

static void char_read_by_uuid_cb(guint8 status, const guint8 *pdu,
					guint16 plen, gpointer user_data)
{
	struct characteristic_data *char_data = user_data;
	struct att_data_list *list;
	int i;

	if (status == ATT_ECODE_ATTR_NOT_FOUND &&
				char_data->start != char_data->orig_start)
		goto done;

	if (status != 0) {
		printf("Read characteristics by UUID failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	list = dec_read_by_type_resp(pdu, plen);
	if (list == NULL)
		goto done;

	for (i = 0; i < list->num; i++) {
		uint8_t *value = list->data[i];
		int j;

		char_data->start = att_get_u16(value) + 1;

		printf("\nhandle: 0x%04x \t value: ", att_get_u16(value));
		value += 2;
		for (j = 0; j < list->len - 2; j++, value++)
			printf("%02x ", *value);
		printf("\n");
	}

	att_data_list_free(list);

done:
	printf("\n%s", get_prompt());
	fflush(stdout);

	g_free(char_data);
}

static void cmd_exit(int argcp, char **argvp)
{
	g_main_loop_quit(event_loop);
}

static void cmd_connect(int argcp, char **argvp)
{
	if (conn_state != STATE_DISCONNECTED)
		goto done;

	if (argcp > 1) {
		g_free(opt_dst);
		opt_dst = g_strdup(argvp[1]);
	}

	if (opt_dst == NULL) {
		printf("Remote Bluetooth address required\n");
		goto done;
	}

	set_state(STATE_CONNECTING);
	iochannel = gatt_connect(opt_src, opt_dst, opt_sec_level, opt_psm,
						opt_mtu, connect_cb);
	if (iochannel == NULL)
		set_state(STATE_DISCONNECTED);
	else
		return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void cmd_disconnect(int argcp, char **argvp)
{
	if (conn_state == STATE_DISCONNECTED)
		goto done;

	g_attrib_unref(attrib);
	attrib = NULL;
	opt_mtu = 0;

	g_io_channel_shutdown(iochannel, FALSE, NULL);
	g_io_channel_unref(iochannel);
	iochannel = NULL;

done:
	set_state(STATE_DISCONNECTED);
}

static void cmd_primary(int argcp, char **argvp)
{
	bt_uuid_t uuid;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		goto done;
	}

	if (argcp == 1) {
		gatt_discover_primary(attrib, NULL, primary_all_cb, NULL);
		goto done;
	}

	if (bt_string_to_uuid(&uuid, argvp[1]) < 0) {
		printf("Invalid UUID\n");
		goto done;
	}

	gatt_discover_primary(attrib, &uuid, primary_by_uuid_cb, NULL);
	return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static int strtohandle(const char *src)
{
	char *e;
	int dst;

	errno = 0;
	dst = strtoll(src, &e, 16);
	if (errno != 0 || *e != '\0')
		return -EINVAL;

	return dst;
}

static void cmd_char(int argcp, char **argvp)
{
	int start = 0x0001;
	int end = 0xffff;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		goto done;
	}

	if (argcp > 1) {
		start = strtohandle(argvp[1]);
		if (start < 0) {
			printf("Invalid start handle: %s\n", argvp[1]);
			goto done;
		}
	}

	if (argcp > 2) {
		end = strtohandle(argvp[2]);
		if (end < 0) {
			printf("Invalid end handle: %s\n", argvp[2]);
			goto done;
		}
	}

	if (argcp > 3) {
		bt_uuid_t uuid;

		if (bt_string_to_uuid(&uuid, argvp[3]) < 0) {
			printf("Invalid UUID\n");
			goto done;
		}

		gatt_discover_char(attrib, start, end, &uuid, char_cb, NULL);
		return;
	}

	gatt_discover_char(attrib, start, end, NULL, char_cb, NULL);
	return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void cmd_char_desc(int argcp, char **argvp)
{
	int start = 0x0001;
	int end = 0xffff;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		goto done;
	}

	if (argcp > 1) {
		start = strtohandle(argvp[1]);
		if (start < 0) {
			printf("Invalid start handle: %s\n", argvp[1]);
			goto done;
		}
	}

	if (argcp > 2) {
		end = strtohandle(argvp[2]);
		if (end < 0) {
			printf("Invalid end handle: %s\n", argvp[2]);
			goto done;
		}
	}

	gatt_find_info(attrib, start, end, char_desc_cb, NULL);
	return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void cmd_read_hnd(int argcp, char **argvp)
{
	int handle;
	int offset = 0;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		goto done;
	}

	if (argcp < 2) {
		printf("Missing argument: handle\n");
		goto done;
	}

	handle = strtohandle(argvp[1]);
	if (handle < 0) {
		printf("Invalid handle: %s\n", argvp[1]);
		goto done;
	}

	if (argcp > 2) {
		char *e;

		errno = 0;
		offset = strtol(argvp[2], &e, 0);
		if (errno != 0 || *e != '\0') {
			printf("Invalid offset: %s\n", argvp[2]);
			goto done;
		}
	}

	gatt_read_char(attrib, handle, offset, char_read_cb, attrib);
	return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void cmd_read_uuid(int argcp, char **argvp)
{
	struct characteristic_data *char_data;
	int start = 0x0001;
	int end = 0xffff;
	bt_uuid_t uuid;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		goto done;
	}

	if (argcp < 2) {
		printf("Missing argument: UUID\n");
		goto done;
	}

	if (bt_string_to_uuid(&uuid, argvp[1]) < 0) {
		printf("Invalid UUID\n");
		goto done;
	}

	if (argcp > 2) {
		start = strtohandle(argvp[2]);
		if (start < 0) {
			printf("Invalid start handle: %s\n", argvp[1]);
			goto done;
		}
	}

	if (argcp > 3) {
		end = strtohandle(argvp[3]);
		if (end < 0) {
			printf("Invalid end handle: %s\n", argvp[2]);
			goto done;
		}
	}

	char_data = g_new(struct characteristic_data, 1);
	char_data->orig_start = start;
	char_data->start = start;
	char_data->end = end;
	char_data->uuid = uuid;

	gatt_read_char_by_uuid(attrib, start, end, &char_data->uuid,
					char_read_by_uuid_cb, char_data);

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void char_write_req_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	if (status != 0) {
		printf("Characteristic Write Request failed: "
						"%s\n", att_ecode2str(status));
		goto done;
	}

	if (!dec_write_resp(pdu, plen)) {
		printf("Protocol error\n");
		goto done;
	}

	printf("Characteristic value was written successfully\n");

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void cmd_char_write(int argcp, char **argvp)
{
	uint8_t *value;
	size_t plen;
	int handle;

	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: disconnected\n");
		goto done;
	}

	if (argcp < 3) {
		printf("Usage: %s <handle> <new value>\n", argvp[0]);
		goto done;
	}

	handle = strtoll(argvp[1], NULL, 16);
	if (errno != 0 || handle <= 0) {
		printf("A valid handle is required\n");
		goto done;
	}

	plen = gatt_attr_data_from_string(argvp[2], &value);
	if (plen == 0) {
		g_printerr("Invalid value\n");
		goto done;
	}

	if (g_strcmp0("char-write-req", argvp[0]) == 0)
		gatt_write_char(attrib, handle, value, plen,
					char_write_req_cb, NULL);
	else
		gatt_write_char(attrib, handle, value, plen, NULL, NULL);

	g_free(value);
	return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void cmd_sec_level(int argcp, char **argvp)
{
	GError *gerr = NULL;
	BtIOSecLevel sec_level;

	if (argcp < 2) {
		printf("sec-level: %s\n", opt_sec_level);
		goto done;
	}

	if (strcasecmp(argvp[1], "medium") == 0)
		sec_level = BT_IO_SEC_MEDIUM;
	else if (strcasecmp(argvp[1], "high") == 0)
		sec_level = BT_IO_SEC_HIGH;
	else if (strcasecmp(argvp[1], "low") == 0)
		sec_level = BT_IO_SEC_LOW;
	else {
		printf("Allowed values: low | medium | high\n");
		goto done;
	}

	g_free(opt_sec_level);
	opt_sec_level = g_strdup(argvp[1]);

	if (conn_state != STATE_CONNECTED)
		goto done;

	if (opt_psm) {
		printf("It must be reconnected to this change take effect\n");
		goto done;
	}

	bt_io_set(iochannel, BT_IO_L2CAP, &gerr,
			BT_IO_OPT_SEC_LEVEL, sec_level,
			BT_IO_OPT_INVALID);

	if (gerr) {
		printf("Error: %s\n", gerr->message);
		g_error_free(gerr);
	}
	return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void exchange_mtu_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	uint16_t mtu;

	if (status != 0) {
		printf("Exchange MTU Request failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	if (!dec_mtu_resp(pdu, plen, &mtu)) {
		printf("Protocol error\n");
		goto done;
	}

	mtu = MIN(mtu, opt_mtu);
	/* Set new value for MTU in client */
	if (g_attrib_set_mtu(attrib, mtu))
		printf("MTU was exchanged successfully: %d\n", mtu);
	else
		printf("Error exchanging MTU\n");

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static void cmd_mtu(int argcp, char **argvp)
{
	if (conn_state != STATE_CONNECTED) {
		printf("Command failed: not connected.\n");
		goto done;
	}

	if (opt_psm) {
		printf("Command failed: operation is only available for LE"
							" transport.\n");
		goto done;
	}

	if (argcp < 2) {
		printf("Usage: mtu <value>\n");
		goto done;
	}

	if (opt_mtu) {
		printf("Command failed: MTU exchange can only occur once per"
							" connection.\n");
		goto done;
	}

	errno = 0;
	opt_mtu = strtoll(argvp[1], NULL, 0);
	if (errno != 0 || opt_mtu < ATT_DEFAULT_LE_MTU) {
		printf("Invalid value. Minimum MTU size is %d\n",
							ATT_DEFAULT_LE_MTU);
		goto done;
	}

	gatt_exchange_mtu(attrib, opt_mtu, exchange_mtu_cb, NULL);
	return;

done:
	printf("\r%s", get_prompt());
	fflush(stdout);
}

static struct {
	const char *cmd;
	void (*func)(int argcp, char **argvp);
	const char *params;
	const char *desc;
} commands[] = {
	{ "help",		cmd_help,	"",
		"Show this help"},
	{ "exit",		cmd_exit,	"",
		"Exit interactive mode" },
	{ "connect",		cmd_connect,	"[address]",
		"Connect to a remote device" },
	{ "disconnect",		cmd_disconnect,	"",
		"Disconnect from a remote device" },
	{ "primary",		cmd_primary,	"[UUID]",
		"Primary Service Discovery" },
	{ "characteristics",	cmd_char,	"[start hnd [end hnd [UUID]]]",
		"Characteristics Discovery" },
	{ "char-desc",		cmd_char_desc,	"[start hnd] [end hnd]",
		"Characteristics Descriptor Discovery" },
	{ "char-read-hnd",	cmd_read_hnd,	"<handle> [offset]",
		"Characteristics Value/Descriptor Read by handle" },
	{ "char-read-uuid",	cmd_read_uuid,	"<UUID> [start hnd] [end hnd]",
		"Characteristics Value/Descriptor Read by UUID" },
	{ "char-write-req",	cmd_char_write,	"<handle> <new value>",
		"Characteristic Value Write (Write Request)" },
	{ "char-write-cmd",	cmd_char_write,	"<handle> <new value>",
		"Characteristic Value Write (No response)" },
	{ "sec-level",		cmd_sec_level,	"[low | medium | high]",
		"Set security level. Default: low" },
	{ "mtu",		cmd_mtu,	"<value>",
		"Exchange MTU for GATT/ATT" },
	{ NULL, NULL, NULL, NULL}
};

static void cmd_help(int argcp, char **argvp)
{
	int i;

	for (i = 0; commands[i].cmd; i++)
		printf("%-15s %-30s %s\n", commands[i].cmd,
				commands[i].params, commands[i].desc);

	printf("\n%s", get_prompt());
	fflush(stdout);
}

static void parse_line(char *line_read)
{
	gchar *argvp[10];
	int argcp;
	int i;

	if (line_read == NULL) {
		printf("\n");
		cmd_exit(0, NULL);
		return;
	}

	line_read = g_strstrip(line_read);

	if (*line_read == '\0')
		return;

	for (i = 0; i < 10; i++) {
		if (*line_read) {
			argvp[i] = line_read++;
			while (*line_read && *line_read != ' ')
				line_read++;
			while (*line_read && *line_read == ' ')
				*line_read++ = '\0';
		} else
			break;
	}

	argcp = i;

	for (i = 0; commands[i].cmd; i++)
		if (strcasecmp(commands[i].cmd, argvp[0]) == 0)
			break;

	if (commands[i].cmd)
		commands[i].func(argcp, argvp);
	else
		printf("%s: command not found\n", argvp[0]);
}

static gboolean prompt_read(GIOChannel *chan, GIOCondition cond,
							gpointer user_data)
{
	gsize bytes_read;
	GIOStatus status;
	GError *err = NULL;
	int i;


	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		g_io_channel_unref(chan);
		return FALSE;
	}

	while (cond & G_IO_IN) {
		bytes_read = 0;
		status = g_io_channel_read_chars(chan, &inp[didx], INPUT_SIZE - didx, &bytes_read, &err);
		inp[didx + bytes_read] = '\0';
		if (bytes_read && (status == G_IO_STATUS_NORMAL)) {
			for (i = 0; (i < INPUT_SIZE) && (i < (didx + bytes_read)); i++) {
				if (inp[i] == '\r' || inp[i] == '\n')
					inp[i] = '\0';
				if (!inp[i]) {
					didx = i;
					break;
				}
			}
			if (inp[i] == '\0') {
				if (didx) {
					didx = 0;
					parse_line(inp);
				}
				//printf("\r%s", get_prompt());
				//fflush(stdout);
				return TRUE;
			} else if (didx < INPUT_SIZE) {
				inp[didx] = '\0';
				if (didx == INPUT_SIZE)
					didx--;
			} else {
				/* Should never get here */
				didx--;
			}
		} else if (err) {
			printf("Error: %s\n", err->message);
			break;
		} else {
			printf("IO Done\n");
			break;
		}
	}
	printf("Cond: %d\n", g_io_channel_get_buffer_condition(chan));


	return TRUE;
}

int interactive(const gchar *src, const gchar *dst, int psm)
{
	GIOChannel *pchan;
	gint events;

	opt_sec_level = g_strdup("low");

	inp = g_malloc0(INPUT_SIZE + 1);
	opt_src = g_strdup(src);
	opt_dst = g_strdup(dst);
	opt_psm = psm;

	prompt = g_string_new(NULL);

	event_loop = g_main_loop_new(NULL, FALSE);

	pchan = g_io_channel_unix_new(fileno(stdin));
	g_io_channel_set_close_on_unref(pchan, TRUE);
	g_io_channel_set_encoding(pchan, NULL, NULL);
	g_io_channel_set_buffered(pchan, FALSE);
	events = G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL;
	g_io_add_watch(pchan, events, prompt_read, NULL);

	printf("\r%s", get_prompt());
	fflush(stdout);
	g_main_loop_run(event_loop);

	cmd_disconnect(0, NULL);
	g_io_channel_unref(pchan);
	g_main_loop_unref(event_loop);
	g_string_free(prompt, TRUE);

	g_free(opt_src);
	g_free(opt_dst);
	g_free(opt_sec_level);
	g_free(inp);

	return 0;
}
