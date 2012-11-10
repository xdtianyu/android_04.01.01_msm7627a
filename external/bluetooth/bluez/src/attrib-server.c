/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <sys/stat.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <gdbus.h>

#include "adapter.h"
#include "device.h"
#include "log.h"
#include "dbus-common.h"
#include "error.h"
#include "glib-helper.h"
#include "btio.h"
#include "sdpd.h"
#include "hcid.h"
#include "att.h"
#include "gattrib.h"
#include "storage.h"
#include "manager.h"

#include "attrib-server.h"

#define GATT_SERVER_INTERFACE	"org.bluez.GattServer"
#define REQUEST_TIMEOUT (5 * 1000)		/* 5 seconds */

#define CARRIER_NO_RESTRICTION	0
#define CARRIER_LE_ONLY		1
#define CARRIER_BR_ONLY		2

const char *gatt_sdp_prefix = "gatt_sdp_";
const char *gatt_adv_prefix = "gatt_adv_";
const char *serial_num_str = "SerialNum";

static GSList *database = NULL;

struct gatt_sdp_handles {
	struct gatt_sdp_handles	*next;
	uint32_t		handle;
};

struct gatt_adv_handles {
	struct gatt_adv_handles	*next;
	uuid_t			uuid;
};

struct gatt_server {
	struct gatt_server	*next;
	struct gatt_server	*prev;
	struct gatt_sdp_handles	*sdp;
	struct gatt_adv_handles	*adv;
	uint16_t		count;
	uint16_t		base;
	uint8_t			carrier;
	char			*path;
	char			name[0];
} *gatt_server_list = NULL, *gatt_server_last = NULL;

struct operation {
	uint8_t	opcode;
	struct gatt_server	*server;
	union {
		struct {
			uint16_t	start;
			uint16_t	end;
			struct att_data_list *adl;
		} find_info;
		struct {
			uint16_t	start;
			uint16_t	end;
			uint16_t	type;
			uint8_t		vlen;
			uint8_t		value[16];
			struct att_data_list *adl;
		} find_by_type;
		struct {
			uint16_t	start;
			uint16_t	end;
			bt_uuid_t	uuid;
			struct att_data_list *adl;
		} read_by_type;
		struct {
			uint16_t	handle;
			uint16_t	offset;
		} read_blob;
		struct {
			uint16_t	count;
			uint16_t	finished;
			uint16_t	*array;
		} read_mult;
		struct {
			uint16_t	start;
			uint16_t	end;
			bt_uuid_t	uuid;
			struct att_data_list *adl;
		} read_by_group;
		struct {
			uint16_t	handle;
			int		vlen;
			uint8_t		*value;
		} write;
	} u;
};

struct gatt_channel {
	bdaddr_t src;
	bdaddr_t dst;
	GSList *notify;
	GSList *indicate;
	GAttrib *attrib;
	void *device;
	guint mtu;
	gboolean le;
	guint id;
	uint32_t serial;
	uint32_t session;
	DBusMessage *msg;
	DBusMessage *ind_msg;
	DBusPendingCall *call;
	struct operation op;
	uint16_t olen;
	uint8_t opdu[0];
};

struct group_elem {
	uint16_t handle;
	uint16_t end;
	uint8_t *data;
	uint16_t len;
};

static DBusConnection *connection = NULL;
static GIOChannel *l2cap_io = NULL;
static GIOChannel *le_io = NULL;
static GSList *clients = NULL;
static uint32_t gatt_sdp_handle = 0;
static uint32_t gap_sdp_handle = 0;
static uint32_t serial_num = 0;

/* GAP attribute handles */
static uint16_t name_handle = 0x0000;
static uint16_t appearance_handle = 0x0000;

/* GATT attribute handles */
static uint16_t svc_chg_handle = 0x0000;

static bt_uuid_t prim_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_PRIM_SVC_UUID
};
static bt_uuid_t snd_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_SND_SVC_UUID
};

static bt_uuid_t inc_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_INCLUDE_UUID
};

static bt_uuid_t char_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_CHARAC_UUID
};

static bt_uuid_t clicfg_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_CLIENT_CHARAC_CFG_UUID
};

static bt_uuid_t aggr_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_CHARAC_AGREG_FMT_UUID
};

static guint server_resp(GAttrib *attrib, guint id, guint8 opcode,
			const guint8 *pdu, guint16 len, GAttribResultFunc func,
			gpointer user_data, GDestroyNotify notify)
{
	guint ret = g_attrib_send(attrib, id, opcode, pdu, len, func,
							user_data, notify);

	g_attrib_unref(attrib);

	return ret;
}

static sdp_record_t *server_record_new(uuid_t *uuid, uint16_t start, uint16_t end)
{
	sdp_list_t *svclass_id, *apseq, *proto[2], *root, *aproto;
	uuid_t root_uuid, proto_uuid, l2cap;
	sdp_record_t *record;
	sdp_data_t *psm, *sh, *eh;
	uint16_t lp = ATT_PSM;

	if (uuid == NULL)
		return NULL;

	if (start > end)
		return NULL;

	record = sdp_record_alloc();
	if (record == NULL)
		return NULL;

	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root = sdp_list_append(NULL, &root_uuid);
	sdp_set_browse_groups(record, root);
	sdp_list_free(root, NULL);

	svclass_id = sdp_list_append(NULL, uuid);
	sdp_set_service_classes(record, svclass_id);
	sdp_list_free(svclass_id, NULL);

	sdp_uuid16_create(&l2cap, L2CAP_UUID);
	proto[0] = sdp_list_append(NULL, &l2cap);
	psm = sdp_data_alloc(SDP_UINT16, &lp);
	proto[0] = sdp_list_append(proto[0], psm);
	apseq = sdp_list_append(NULL, proto[0]);

	sdp_uuid16_create(&proto_uuid, ATT_UUID);
	proto[1] = sdp_list_append(NULL, &proto_uuid);
	sh = sdp_data_alloc(SDP_UINT16, &start);
	proto[1] = sdp_list_append(proto[1], sh);
	eh = sdp_data_alloc(SDP_UINT16, &end);
	proto[1] = sdp_list_append(proto[1], eh);
	apseq = sdp_list_append(apseq, proto[1]);

	aproto = sdp_list_append(NULL, apseq);
	sdp_set_access_protos(record, aproto);

	sdp_data_free(psm);
	sdp_data_free(sh);
	sdp_data_free(eh);
	sdp_list_free(proto[0], NULL);
	sdp_list_free(proto[1], NULL);
	sdp_list_free(apseq, NULL);
	sdp_list_free(aproto, NULL);

	return record;
}

static int handle_cmp(gconstpointer a, gconstpointer b)
{
	const struct attribute *attrib = a;
	uint16_t handle = GPOINTER_TO_UINT(b);

	return attrib->handle - handle;
}

static int attribute_cmp(gconstpointer a1, gconstpointer a2)
{
	const struct attribute *attrib1 = a1;
	const struct attribute *attrib2 = a2;

	return attrib1->handle - attrib2->handle;
}

static uint8_t att_check_reqs(struct gatt_channel *channel, uint8_t opcode,
								int reqs)
{
	BtIOSecLevel sec_level = g_attrib_sec_level(channel->attrib);

	if (reqs == ATT_AUTHENTICATION && sec_level < BT_IO_SEC_HIGH)
		return ATT_ECODE_AUTHENTICATION;
	else if (reqs == ATT_AUTHORIZATION && sec_level < BT_IO_SEC_MEDIUM)
		return ATT_ECODE_AUTHORIZATION;

	switch (opcode) {
	case ATT_OP_READ_BY_GROUP_REQ:
	case ATT_OP_READ_BY_TYPE_REQ:
	case ATT_OP_READ_REQ:
	case ATT_OP_READ_BLOB_REQ:
	case ATT_OP_READ_MULTI_REQ:
		if (reqs == ATT_NOT_PERMITTED)
			return ATT_ECODE_READ_NOT_PERM;
		break;
	case ATT_OP_PREP_WRITE_REQ:
	case ATT_OP_WRITE_REQ:
	case ATT_OP_WRITE_CMD:
		if (reqs == ATT_NOT_PERMITTED)
			return ATT_ECODE_WRITE_NOT_PERM;
		break;
	}

	return 0;
}

static void make_cli_cfg_name(char *dst, struct gatt_channel *channel)
{
	char srcstr[18];
	char dststr[18 + 7];

	ba2str(&channel->src, srcstr);
	memcpy(dststr, "clicfg_", 7);
	ba2str(&channel->dst, &dststr[7]);
	create_name(dst, PATH_MAX, STORAGEDIR, srcstr, dststr);
}

static void cache_cli_cfg(struct gatt_channel *channel, uint16_t handle,
								uint8_t *val)
{
	char filename[PATH_MAX + 1];
	char key[5];
	char value[9];
	int i, j;
	uint16_t cfg_val = att_get_u16(val);

	make_cli_cfg_name(filename, channel);

	snprintf(key, sizeof(key), "%4.4X", handle);

	if (!cfg_val) {
		textfile_del(filename, key);
		return;
	}

	create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (cfg_val && handle == svc_chg_handle + 1)
		snprintf(value, sizeof(value), "%8.8X", serial_num);
	else
		for (j = i = 0; i < 2; i++, j += 2)
			snprintf(&value[j], 3, "%2.2X", val[i]);

	textfile_put(filename, key, value);
}

static uint16_t read_cli_cfg(struct gatt_channel *channel, uint16_t handle,
								uint8_t *dst)
{
	char filename[PATH_MAX + 1];
	char key[5];
	char tmp[3];
	char *val;

	make_cli_cfg_name(filename, channel);

	snprintf(key, sizeof(key), "%4.4X", handle);
	val = textfile_get(filename, key);

	/* Default to all off */
	att_put_u16(0x0000, dst);

	/* Special handling of SCI config, where any non-NULL returns 0x0002 */
	if (val && handle == svc_chg_handle + 1)
		att_put_u16(0x0002, dst);
	else if (val) {
		int i, vlen = MIN(2, strlen(val) / 2);

		tmp[2] = 0;
		for (i = 0; i < vlen; i++) {
			tmp[0] = val[i * 2];
			tmp[1] = val[i * 2 + 1];
			dst[i] = (uint8_t) strtol(tmp, NULL, 16);
		}
	}

	g_free(val);

	return att_get_u16(dst);
}

static void update_client_serial(struct gatt_channel *channel)
{
	uint8_t buf[2];
	uint16_t handle = svc_chg_handle + 1;

	channel->serial = serial_num;

	read_cli_cfg(channel, handle, buf);
	cache_cli_cfg(channel, handle, buf);
}

static uint8_t client_get_configurations(struct attribute *attr,
							gpointer user_data)
{
	struct gatt_channel *channel = user_data;

	read_cli_cfg(channel, attr->handle, attr->data);

	return 0;
}

static uint8_t client_set_configurations(struct attribute *attr,
							gpointer user_data)
{
	struct gatt_channel *channel = user_data;
	struct attribute *last_chr_val = NULL;
	uint16_t handle = 0;
	uint16_t cfg_val;
	uint8_t props;
	GSList *l;

	cfg_val = att_get_u16(attr->data);

	/* Search correct for builtin Characteristic Value Handle */
	for (l = database, props = 0; l != NULL; l = l->next) {
		struct attribute *a = l->data;

		if (a->handle >= attr->handle)
			break;

		if (bt_uuid_cmp(&a->uuid, &char_uuid) == 0) {
			props = att_get_u8(&a->data[0]);
			handle = att_get_u16(&a->data[1]);
			continue;
		}

		if (handle && a->handle == handle)
			last_chr_val = a;
	}

	if (last_chr_val == NULL)
		return 0;

	if (cfg_val & 0xFFFC)
		return ATT_ECODE_INVALID_PDU;

	if (!g_attrib_is_encrypted(channel->attrib))
		return ATT_ECODE_AUTHORIZATION;

	if ((cfg_val & 0x0001) && !(props & ATT_CHAR_PROPER_NOTIFY))
		return ATT_ECODE_INVALID_PDU;

	if ((cfg_val & 0x0002) && !(props & ATT_CHAR_PROPER_INDICATE))
		return ATT_ECODE_INVALID_PDU;

	cache_cli_cfg(channel, attr->handle, attr->data);

	if (last_chr_val->handle == svc_chg_handle)
		return 0;

	if (cfg_val & 0x0001)
		channel->notify = g_slist_append(channel->notify, last_chr_val);
	else
		channel->notify = g_slist_remove(channel->notify, last_chr_val);

	if (cfg_val & 0x0002)
		channel->indicate = g_slist_append(channel->indicate,
								last_chr_val);
	else
		channel->indicate = g_slist_remove(channel->indicate,
								last_chr_val);

	return 0;
}

static struct attribute *client_cfg_attribute(struct gatt_channel *channel,
						struct attribute *orig_attr)
{
	static uint8_t static_attribute[sizeof(struct attribute) + 2];
	struct attribute *a = (void *) &static_attribute;

	if (bt_uuid_cmp(&orig_attr->uuid, &clicfg_uuid) != 0)
		return NULL;

	/* permanent memory for passing client Config data */
	a->uuid = clicfg_uuid;
	a->read_reqs = ATT_NONE;
	a->write_reqs = ATT_AUTHORIZATION;
	a->read_cb = client_get_configurations;
	a->write_cb = client_set_configurations;
	a->len = 2;

	a->cb_user_data = channel;
	a->handle = orig_attr->handle;

	return a;
}

static int massage_payload(bt_uuid_t *uuid, uint16_t base, uint16_t limit,
				struct gatt_channel *channel,
				uint16_t handle, const uint8_t *payload,
				uint8_t plen, uint8_t *dst, uint8_t dlen)
{
	bt_uuid_t rx_uuid, tx_uuid;

	/* Massage payload */
	if (!bt_uuid_cmp(uuid, &prim_uuid) || !bt_uuid_cmp(uuid, &snd_uuid)) {
		if (plen < sizeof(struct server_def_val16) || !payload)
			return -1;

		/* No massage needed */
		if (plen == dlen)
			return 0;

		if (plen == sizeof(struct server_def_val128))
			bt_uuid128_create(&rx_uuid, att_get_u128(payload));
		else
			bt_uuid16_create(&rx_uuid, att_get_u16(payload));

		if (!dlen || dlen == sizeof(struct server_def_val16)) {
			bt_uuid_to_uuid16(&rx_uuid, &tx_uuid);
			if (tx_uuid.type == BT_UUID16) {
				att_put_u16(tx_uuid.value.u16, dst);
				return sizeof(struct server_def_val16);
			}
			if (dlen)
				return -1;
		}
		bt_uuid_to_uuid128(&rx_uuid, &tx_uuid);
		att_put_u128(tx_uuid.value.u128, dst);
		return sizeof(struct server_def_val128);

	} else if (!bt_uuid_cmp(uuid, &inc_uuid)) {
		uint16_t start;
		uint16_t end;

		if (plen < sizeof(struct include_def_val16) || !payload)
			return -1;

		start = att_get_u16(payload) + base;
		end = att_get_u16(&payload[2]) + base;
		if (limit && (limit < start && limit < end))
			return -1;

		if (plen == sizeof(struct include_def_val128))
			bt_uuid128_create(&rx_uuid, att_get_u128(&payload[4]));
		else
			bt_uuid16_create(&rx_uuid, att_get_u16(&payload[4]));

		if (!dlen || dlen == sizeof(struct include_def_val16)) {
			bt_uuid_to_uuid16(&rx_uuid, &tx_uuid);
			if (tx_uuid.type == BT_UUID16) {
				att_put_u16(start, dst);
				att_put_u16(end, &dst[2]);
				att_put_u16(tx_uuid.value.u16, &dst[4]);
				return sizeof(struct include_def_val16);
			}
			if (dlen)
				return -1;
		}

		bt_uuid_to_uuid128(&rx_uuid, &tx_uuid);
		att_put_u16(start, dst);
		att_put_u16(end, &dst[2]);
		att_put_u128(tx_uuid.value.u128, &dst[4]);
		return sizeof(struct include_def_val128);

	} else if (!bt_uuid_cmp(uuid, &char_uuid)) {
		uint16_t handle;

		if (plen < sizeof(struct char_def_val16) || !payload)
			return -1;

		handle = att_get_u16(&payload[1]) + base;
		if (limit && limit < handle)
			return -1;

		if (plen == sizeof(struct char_def_val128))
			bt_uuid128_create(&rx_uuid, att_get_u128(&payload[3]));
		else
			bt_uuid16_create(&rx_uuid, att_get_u16(&payload[3]));

		if (!dlen || dlen == sizeof(struct char_def_val16)) {
			bt_uuid_to_uuid16(&rx_uuid, &tx_uuid);
			if (tx_uuid.type == BT_UUID16) {
				att_put_u8(payload[0], dst);
				att_put_u16(handle, &dst[1]);
				att_put_u16(tx_uuid.value.u16, &dst[3]);
				return sizeof(struct char_def_val16);
			}
			if (dlen)
				return -1;
		}

		bt_uuid_to_uuid128(&rx_uuid, &tx_uuid);
		att_put_u8(payload[0], dst);
		att_put_u16(handle, &dst[1]);
		att_put_u128(tx_uuid.value.u128, &dst[3]);
		return sizeof(struct char_def_val128);

	} else if (!bt_uuid_cmp(uuid, &clicfg_uuid)) {

		read_cli_cfg(channel, handle, dst);
		return 2;

	} else if (!bt_uuid_cmp(uuid, &aggr_uuid)) {
		int i;

		if (!dlen)
			dlen = ATT_DEFAULT_LE_MTU;

		if (plen > dlen || !payload)
			return -1;

		for (i = 0; plen && i < plen - 1; i += 2) {
			uint16_t handle = att_get_u16(&payload[i]) + base;

			if (limit && limit < handle)
				return -1;

			att_put_u16(handle, &dst[i]);
		}

		return i;
	}

	/* Not massaged or copied */
	return 0;
}

static const char *sec_level_to_auth(struct gatt_channel *channel)
{
	switch (g_attrib_sec_level(channel->attrib)) {
	case BT_IO_SEC_HIGH:
		return "Authenticated";
	case BT_IO_SEC_MEDIUM:
		return "Authorized";
	default:
		return "None";
	}
}

const char *att_err_map[] = {
	"",				/* 0x00 */
	ATT_INVALID_HANDLE,		/* 0x01 */
	ATT_READ_NOT_PERM,		/* 0x02 */
	ATT_WRITE_NOT_PERM,		/* 0x03 */
	ATT_INVALID_PDU,		/* 0x04 */
	ATT_INSUFF_AUTHENTICATION,	/* 0x05 */
	ATT_REQ_NOT_SUPP,		/* 0x06 */
	ATT_INVALID_OFFSET,		/* 0x07 */
	ATT_INSUFF_AUTHORIZATION,	/* 0x08 */
	ATT_PREP_QUEUE_FULL,		/* 0x09 */
	ATT_ATTR_NOT_FOUND,		/* 0x0A */
	ATT_ATTR_NOT_LONG,		/* 0x0B */
	ATT_INSUFF_ENCR_KEY_SIZE,	/* 0x0C */
	ATT_INVAL_ATTR_VALUE_LEN,	/* 0x0D */
	ATT_UNLIKELY,			/* 0x0E */
	ATT_INSUFF_ENCRYPTION,		/* 0x0F */
	ATT_UNSUPP_GRP_TYPE,		/* 0x10 */
	ATT_INSUFF_RESOURCES,		/* 0x11 */
};

static const char *map_att_error(uint8_t status)
{
	if (status < sizeof(att_err_map)/sizeof(att_err_map[0]))
		return att_err_map[status];

	return att_err_map[ATT_ECODE_UNLIKELY];
}

static uint8_t map_dbus_error(DBusError *err, uint16_t *handle)
{
	const char *app_err = "ATT_0x";
	size_t len;
	int i, j, array_len;

	array_len = sizeof(att_err_map)/sizeof(att_err_map[0]);

	/* Standard ATT Error codes */
	for (i = ATT_ECODE_INVALID_HANDLE; i < array_len; i++) {
		len = strlen(att_err_map[i]);

		if (strncmp(err->message, att_err_map[i], len) == 0) {
			if (sscanf(&err->message[len], ".%x", &j) != 1)
				j = 0xffff;

			*handle = (uint16_t) j;
			return (uint8_t) i;
		}
	}

	/* Extended and Application Error codes */
	len = strlen(app_err);
	if (strncmp(err->message, app_err, len) == 0) {
		if (sscanf(&err->message[len], "%x.%x", &i, &j) != 2) {
			*handle = 0xffff;
			return ATT_ECODE_UNLIKELY;
		}

		*handle = (uint16_t) j;
		if (((uint8_t) i) & 0xFF)
			return (uint8_t) i;
	}

	/* Unrecognized Error code */
	*handle = 0xffff;
	return ATT_ECODE_UNLIKELY;
}

static gboolean is_channel_valid(gpointer data)
{
	GSList *l;

	for (l = clients; l; l = l->next) {
		if (l->data == data) {
			DBG("Channel found :%p", data);
			return TRUE;
		}
	}

	return FALSE;
}

static void dbus_read_by_group(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid);
static void read_by_group_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct att_data_list *adl = channel->op.u.read_by_group.adl;
	struct gatt_server *server = channel->op.server;
	DBusMessage *message;
	DBusError err;
	uint16_t length, handle, end;
	char *uuid_str;
	bt_uuid_t uuid, result_uuid;
	uint8_t *value;
	uint8_t att_err = ATT_ECODE_ATTR_NOT_FOUND;
	gboolean terminated = FALSE;

	DBG("");

	/* Init handle to end of this server in case of error */
	handle = server->base + server->count;

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		DBG("Server replied with an error: %s, %s",
				err.name, err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
					DBUS_TYPE_UINT16, &handle,
					DBUS_TYPE_UINT16, &end,
					DBUS_TYPE_STRING, &uuid_str,
					DBUS_TYPE_INVALID)) {
		DBG("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	handle += server->base;
	end += server->base;
	bt_string_to_uuid(&uuid, uuid_str);

	if (!adl || adl->len == 6)
		bt_uuid_to_uuid16(&uuid, &result_uuid);
	else
		bt_uuid_to_uuid128(&uuid, &result_uuid);

	if (!adl) {
		uint16_t res_size = 6;

		if (result_uuid.type != BT_UUID16) {
			bt_uuid_to_uuid128(&uuid, &result_uuid);
			res_size = 20;
		}
		adl = att_data_list_alloc((channel->mtu - 2)/res_size,
								res_size);
	}

	/* If result_uuid is unspecified here, we are done */
	if (result_uuid.type == BT_UUID_UNSPEC) {
		terminated = TRUE;
		DBG(" Bail-5");
		goto cleanup_dbus;
	}

	channel->op.u.read_by_group.adl = adl;
	value = (void *) adl->data[adl->cnt++];
	/* Attribute Handles */
	att_put_u16(handle, value);
	att_put_u16(end, &value[2]);

	/* Attribute Value */
	if (adl->len == 6)
		att_put_u16(result_uuid.value.u16, &value[4]);
	else
		att_put_u128(result_uuid.value.u128, &value[4]);

	handle = end + 1;

	/* End of attribute range reached */
	if (!handle || handle > channel->op.u.read_by_group.end)
		terminated = TRUE;

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	if (adl && adl->num == adl->cnt) {
		terminated = TRUE;
		goto done;
	}

	/* See if any other servers could provide results */
	while (server && (handle >= (server->base + server->count)))
		server = server->next;

	if (!server)
		terminated = TRUE;

done:
	DBG(" Compose Response");
	if (terminated && adl) {
		/* send results */
		length = enc_read_by_grp_resp(adl, channel->opdu, channel->mtu);
		att_data_list_free(adl);
		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	if (terminated) {
		/* Or send NOT FOUND */
		length = enc_error_resp(ATT_OP_READ_BY_GROUP_REQ,
					channel->op.u.read_by_group.start,
					att_err, channel->opdu, channel->mtu);

		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	dbus_read_by_group(channel, handle, channel->op.u.read_by_group.end,
					&channel->op.u.read_by_group.uuid);
}

static void dbus_read_by_group(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid)
{
	struct gatt_server *server = gatt_server_list;
	struct att_data_list *adl = channel->op.u.read_by_group.adl;
	uint16_t norm_start = 0, norm_end, length;
	char uuid_buf[MAX_LEN_UUID_STR];
	char *uuid_str = uuid_buf;
	bt_uuid_t uuid128;
	uint8_t status = ATT_ECODE_ATTR_NOT_FOUND;
	int err;

	DBG(" start:0x%04x end:0x%04x", start, end);

	while (server && (server->base + server->count) <= start)
		server = server->next;

	if (!server)
		goto failed;

	if (start > server->base)
		norm_start = start - server->base;

	if (end < (server->base + server->count))
		norm_end = end - server->base;
	else
		norm_end = server->count - 1;

	DBG(" Construct Server Call %s, %s", server->name, server->path);

	channel->msg = dbus_message_new_method_call(server->name, server->path,
					GATT_SERVER_INTERFACE, "ReadByGroup");
	if (channel->msg == NULL)
		goto failed;

	bt_uuid_to_uuid128(uuid, &uuid128);
	err = bt_uuid_to_string(&uuid128, uuid_buf, MAX_LEN_UUID_STR);

	if (err < 0)
		goto failed;

	dbus_message_append_args(channel->msg, DBUS_TYPE_UINT16, &norm_start,
						DBUS_TYPE_UINT16, &norm_end,
						DBUS_TYPE_STRING, &uuid_str,
						DBUS_TYPE_INVALID);

	DBG(" Calling Server %s, %s", server->name, server->path);

	if (!dbus_connection_send_with_reply(connection, channel->msg,
					&channel->call, REQUEST_TIMEOUT)) {
		DBG(" Failed try to: %s + %s -- Cleanup and recurse", server->name, server->path);
		goto failed_dbus;
	}

	channel->op.server = server;
	dbus_pending_call_set_notify(channel->call, read_by_group_reply,
								channel, NULL);
	DBG(" Server Pending %s, %s", server->name, server->path);

	return;

failed_dbus:
	/* cleanup dbus */
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}
	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	DBG(" Server Failed %s, %s", server->name, server->path);

	/* Attempt on next server, if one exists */
	if (server != gatt_server_last && server->next) {
		server = server->next;
		start = server->base;

		DBG(" Try Next %s, %s 0x%04x,0x%04x", server->name, server->path, start, end);

		dbus_read_by_group(channel, start, end, uuid);
		return;
	}
	DBG(" Server List End");

failed:
	DBG(" Compose Response");
	if (!adl)
		length = enc_error_resp(channel->op.opcode,
					channel->op.u.read_by_group.start,
					status, channel->opdu, channel->mtu);
	else {
		length = enc_read_by_grp_resp(adl, channel->opdu, channel->mtu);
		att_data_list_free(channel->op.u.read_by_group.adl);
	}

	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0], channel->opdu,
					length, NULL, NULL, NULL);
}

static int read_by_group(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid,
						uint8_t *pdu, int len)
{
	struct att_data_list *adl = NULL;
	struct attribute *a;
	struct group_elem *cur, *old = NULL;
	GSList *l, *groups;
	uint16_t length = 0, last_handle, last_size = 0;
	uint8_t status;
	gboolean terminated = FALSE;
	int i;

	{
		char uuid_buf[MAX_LEN_UUID_STR];
		uuid_buf[0] = 0;
		bt_uuid_to_string(uuid, uuid_buf, MAX_LEN_UUID_STR);
		DBG("start:0x%04x end:0x%04x %s", start, end, uuid_buf);
	}

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ, start,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	/*
	 * Only <<Primary Service>> and <<Secondary Service>> grouping
	 * types may be used in the Read By Group Type Request.
	 */

	if (bt_uuid_cmp(uuid, &prim_uuid) != 0 &&
		bt_uuid_cmp(uuid, &snd_uuid) != 0)
		return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ, 0x0000,
					ATT_ECODE_UNSUPP_GRP_TYPE, pdu, len);

	/* Any Primary service discovery updates the clients serial number */
	if (channel->serial < serial_num && !bt_uuid_cmp(uuid, &prim_uuid))
		update_client_serial(channel);

	if (gatt_server_list && gatt_server_list->base <= start)
		goto empty_list;

	last_handle = end;
	for (l = database, groups = NULL, cur = NULL; l; l = l->next) {
		a = l->data;

		DBG("a->handle:0x%04x", a->handle);

		if (a->handle < start)
			continue;

		if (a->handle >= end) {
			terminated = TRUE;
			break;
		}

		/* The old group ends when a new one starts */
		if (old && (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
				bt_uuid_cmp(&a->uuid, &snd_uuid) == 0)) {
			old->end = last_handle;
			old = NULL;
		}

		if (bt_uuid_cmp(&a->uuid, uuid) != 0) {
			char uuid_buf[MAX_LEN_UUID_STR];
			uuid_buf[0] = 0;
			bt_uuid_to_string(&a->uuid, uuid_buf, MAX_LEN_UUID_STR);
			/* Still inside a service, update its last handle */
			DBG("not found h:0x%04x %s", a->handle, uuid_buf);
			if (old)
				last_handle = a->handle;
			continue;
		} else {
			DBG("found h:0x%04x", a->handle);
		}

		if (last_size && (last_size != a->len)) {
			terminated = TRUE;
			break;
		}

		status = att_check_reqs(channel, ATT_OP_READ_BY_GROUP_REQ,
								a->read_reqs);

		if (status == 0x00 && a->read_cb)
			status = a->read_cb(a, a->cb_user_data);

		if (status) {
			DBG("status:0x%02x", status);
			g_slist_foreach(groups, (GFunc) g_free, NULL);
			g_slist_free(groups);
			return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ,
						a->handle, status, pdu, len);
		}

		cur = g_new0(struct group_elem, 1);
		cur->handle = a->handle;
		cur->data = a->data;
		cur->len = a->len;

		/* Attribute Grouping Type found */
		groups = g_slist_append(groups, cur);

		last_size = a->len;
		old = cur;
		last_handle = cur->handle;
	}

	if (groups == NULL) {
		DBG(" Built-in: ATT_ECODE_ATTR_NOT_FOUND");
		if (terminated || gatt_server_list == NULL ||
						gatt_server_list->base > end)
			return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ, start,
					ATT_ECODE_ATTR_NOT_FOUND, pdu, len);
		else
			goto empty_list;
	}

	if (l == NULL)
		last_handle = a->handle;

	cur->end = last_handle;

	last_size += 4;
	length = (len - 2) / last_size;

	adl = att_data_list_alloc(length, last_size);

	for (i = 0, l = groups; l && i < adl->num; l = l->next, i++) {
		uint8_t *value;

		cur = l->data;

		adl->cnt++;
		value = (void *) adl->data[i];

		att_put_u16(cur->handle, value);
		att_put_u16(cur->end, &value[2]);
		/* Attribute Value */
		memcpy(&value[4], cur->data, cur->len);
	}

	g_slist_foreach(groups, (GFunc) g_free, NULL);
	g_slist_free(groups);

	/* If no more room, or we are not at end of builtin list, send now */
	if (l != NULL || adl->cnt == adl->num || terminated ||
			gatt_server_list == NULL ||
			gatt_server_list->base > end) {

		length = enc_read_by_grp_resp(adl, pdu, len);
		att_data_list_free(adl);
		return length;
	}

empty_list:
	channel->op.opcode = ATT_OP_READ_BY_GROUP_REQ;
	channel->op.u.read_by_group.adl = adl;
	channel->op.u.read_by_group.start = start;
	channel->op.u.read_by_group.end = end;
	channel->op.u.read_by_group.uuid = *uuid;
	dbus_read_by_group(channel, start, end, uuid);
	return -1;
}

static void dbus_read_by_type(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid);

static void read_by_chr_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct att_data_list *adl = channel->op.u.read_by_type.adl;
	struct gatt_server *server = channel->op.server;
	bt_uuid_t uuid, result_uuid;
	DBusMessage *message;
	DBusError err;
	uint16_t length, handle, val;
	uint8_t att_err = ATT_ECODE_ATTR_NOT_FOUND;
	uint8_t *value, res_size, prop;
	const char *uuid_str;
	gboolean terminated = FALSE;

	DBG("");

	/* Init handle to end of this server in case of error */
	handle = server->base + server->count;

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		error("Server replied with an error: %s, %s",
				err.name, err.message);

		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_BYTE, &prop,
				DBUS_TYPE_UINT16, &val,
				DBUS_TYPE_STRING, &uuid_str,
				DBUS_TYPE_INVALID)) {
		error("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	if (handle >= val || val >= server->count) {
		DBG(" range error");
		goto cleanup_dbus;
	}

	handle += server->base;
	val += server->base;

	bt_string_to_uuid(&uuid, uuid_str);

	if (!adl || adl->len == 7) {
		bt_uuid_to_uuid16(&uuid, &result_uuid);
		if (result_uuid.type == BT_UUID16) {
			res_size = 7;
		} else {
			bt_uuid_to_uuid128(&uuid, &result_uuid);
			res_size = 21;
		}
	} else {
		bt_uuid_to_uuid128(&uuid, &result_uuid);
		res_size = 21;
	}


	if (!adl) {
		adl = att_data_list_alloc((channel->mtu - 2)/res_size, res_size);
	} else if (res_size != adl->len) {
		terminated = TRUE;
		goto cleanup_dbus;
	}

	channel->op.u.read_by_type.adl = adl;
	value = (void *) adl->data[adl->cnt++];

	/* Characteristic Definition Handle */
	att_put_u16(handle, &value[0]);

	/* Characteristic Property (Value) */
	att_put_u8(prop, &value[2]);

	/* Characteristic Value Handle (Value) */
	att_put_u16(val, &value[3]);

	/* Characteristic UUID (Value) */
	if (result_uuid.type == BT_UUID16)
		att_put_u16(result_uuid.value.u16, &value[5]);
	else
		att_put_u128(result_uuid.value.u128, &value[5]);

	handle += 1;

	/* End of attribute range reached */
	if (!handle || handle > channel->op.u.read_by_type.end)
		terminated = TRUE;

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	if (adl && adl->num == adl->cnt) {
		terminated = TRUE;
		goto done;
	}

	/* See if any other servers could provide results */
	while (server && (handle >= (server->base + server->count)))
		server = server->next;

	if (!server)
		terminated = TRUE;

done:
	if (terminated && adl) {
		/* send results */
		length = enc_read_by_type_resp(adl, channel->opdu,
							channel->mtu);
		att_data_list_free(adl);
		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	if (terminated) {
		/* Or send NOT FOUND */
		length = enc_error_resp(ATT_OP_READ_BY_TYPE_REQ,
					channel->op.u.read_by_type.start,
					att_err, channel->opdu, channel->mtu);

		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	dbus_read_by_type(channel, handle, channel->op.u.read_by_type.end,
					&channel->op.u.read_by_type.uuid);
}

static void read_by_inc_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct att_data_list *adl = channel->op.u.read_by_type.adl;
	struct gatt_server *server = channel->op.server;
	bt_uuid_t uuid, uuid16;
	DBusMessage *message;
	DBusError err;
	uint16_t length, handle, start, end;
	uint8_t *value, res_size;
	uint8_t att_err = ATT_ECODE_ATTR_NOT_FOUND;
	const char *uuid_str;
	gboolean terminated = FALSE;

	DBG("");

	/* Init handle to end of this server in case of error */
	handle = server->base + server->count;

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		error("Server replied with an error: %s, %s",
				err.name, err.message);

		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_UINT16, &start,
				DBUS_TYPE_UINT16, &end,
				DBUS_TYPE_STRING, &uuid_str,
				DBUS_TYPE_INVALID)) {
		error("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	if (start > end || handle >= server->count || end >= server->count) {
		DBG(" range error");
		goto cleanup_dbus;
	}

	handle += server->base;
	start += server->base;
	end += server->base;

	bt_string_to_uuid(&uuid, uuid_str);
	bt_uuid_to_uuid16(&uuid, &uuid16);

	if ((!adl || adl->len == 8) && uuid16.type == BT_UUID16)
		res_size = 8;
	else
		res_size = 6;


	if (!adl) {
		adl = att_data_list_alloc((channel->mtu - 2)/res_size, res_size);
	} else if (res_size != adl->len) {
		terminated = TRUE;
		goto cleanup_dbus;
	}

	channel->op.u.read_by_type.adl = adl;
	value = (void *) adl->data[adl->cnt++];

	/* Attribute Handle */
	att_put_u16(handle, value);

	/* Included Service Range (Value) */
	att_put_u16(start, &value[2]);
	att_put_u16(end, &value[4]);

	/* Included Service UUID (Value, if UUID16 only) */
	if (uuid16.type == BT_UUID16)
		att_put_u16(uuid16.value.u16, &value[6]);

	handle += 1;

	/* End of attribute range reached */
	if (!handle || handle > channel->op.u.read_by_type.end)
		terminated = TRUE;

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	if (adl && adl->num == adl->cnt) {
		terminated = TRUE;
		goto done;
	}

	/* See if any other servers could provide results */
	while (server && (handle >= (server->base + server->count)))
		server = server->next;

	if (!server)
		terminated = TRUE;

done:
	if (terminated && adl) {
		/* send results */
		length = enc_read_by_type_resp(adl, channel->opdu,
							channel->mtu);
		att_data_list_free(adl);
		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	if (terminated) {
		/* Or send NOT FOUND */
		length = enc_error_resp(ATT_OP_READ_BY_TYPE_REQ,
					channel->op.u.read_by_type.start,
					att_err, channel->opdu, channel->mtu);

		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	dbus_read_by_type(channel, handle, channel->op.u.read_by_type.end,
					&channel->op.u.read_by_type.uuid);
}

static void read_by_type_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct att_data_list *adl = channel->op.u.read_by_type.adl;
	struct gatt_server *server = channel->op.server;
	DBusMessage *message;
	DBusError err;
	uint16_t length, handle, err_handle = 0;
	uint8_t *value, dst[ATT_DEFAULT_LE_MTU];
	uint8_t att_err = ATT_ECODE_ATTR_NOT_FOUND;
	const uint8_t *payload;
	dbus_int32_t cnt;
	gboolean terminated = FALSE;
	int mas_ret, res_size;

	DBG("");

	/* Init handle to end of this server in case of error */
	handle = server->base + server->count;

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		error("Server replied with an error: %s, %s",
				err.name, err.message);

		att_err = map_dbus_error(&err, &err_handle);
		dbus_error_free(&err);

		/* Ensure server didn't return handle outside it's range */
		if (err_handle >= server->count)
			err_handle = server->count - 1;

		err_handle += server->base;

		/* Ensure error handle within requested range */
		if (err_handle < channel->op.u.read_by_type.start)
			err_handle = channel->op.u.read_by_type.start;

		if (err_handle > channel->op.u.read_by_type.end)
			err_handle = channel->op.u.read_by_type.end;

		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &payload, &cnt,
				DBUS_TYPE_INVALID)) {
		error("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	handle += server->base;

	if (adl)
		res_size = adl->len - 2;
	else
		res_size = 0;

	/* Massage payload */
	mas_ret = massage_payload(&channel->op.u.read_by_type.uuid,
			server->base, server->base + server->count,
			channel, handle, payload, cnt, dst, res_size);

	/* If error during massage, skip */
	if (mas_ret < 0) {
		handle = server->base + server->count;
		goto cleanup_dbus;
	}

	/* If data needed massage, use massaged data */
	if (mas_ret) {
		payload = dst;
		cnt = (dbus_int32_t) mas_ret;
	}

	res_size = MIN(cnt + 2, (int)(channel->mtu - 2));

	if (!adl) {
		adl = att_data_list_alloc((channel->mtu - 2)/res_size, res_size);
	} else if (res_size != adl->len) {
		terminated = TRUE;
		goto cleanup_dbus;
	}

	channel->op.u.read_by_type.adl = adl;
	value = (void *) adl->data[adl->cnt++];

	/* Attribute Handle */
	att_put_u16(handle, value);

	/* Attribute Value */
	memcpy(&value[2], payload, res_size - 2);

	handle += 1;

	/* End of attribute range reached */
	if (!handle || handle > channel->op.u.read_by_type.end)
		terminated = TRUE;

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	if (adl && adl->num == adl->cnt) {
		terminated = TRUE;
		goto done;
	}

	if (att_err != ATT_ECODE_ATTR_NOT_FOUND) {
		terminated = TRUE;
		goto done;
	}

	/* See if any other servers could provide results */
	while (server && (handle >= (server->base + server->count)))
		server = server->next;

	if (!server)
		terminated = TRUE;

done:
	if (terminated && adl) {
		/* send results */
		length = enc_read_by_type_resp(adl, channel->opdu,
							channel->mtu);
		att_data_list_free(adl);
		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	if (terminated) {
		/* Or send Error */
		length = enc_error_resp(ATT_OP_READ_BY_TYPE_REQ,
					err_handle, att_err,
					channel->opdu, channel->mtu);

		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	dbus_read_by_type(channel, handle, channel->op.u.read_by_type.end,
					&channel->op.u.read_by_type.uuid);
}

static void dbus_read_by_type(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid)
{
	struct gatt_server *server = gatt_server_list;
	struct att_data_list *adl = channel->op.u.read_by_type.adl;
	uint16_t norm_start = 0, norm_end, length;
	uint8_t status = ATT_ECODE_ATTR_NOT_FOUND;
	uint16_t type = 0;
	int err;

	/* To avoid needless recursion */
restart_read_by_type:

	DBG("start:0x%04x end:0x%04x", start, end);

	while (server && (server->base + server->count) <= start)
		server = server->next;

	if (!server)
		goto failed;

	/* Apply Carrier restriction if appropriate */
	if ((server->carrier == CARRIER_BR_ONLY && channel->le) ||
			(server->carrier == CARRIER_LE_ONLY && !channel->le)) {

		server = server->next;
		goto restart_read_by_type;
	}

	if (start > server->base)
		norm_start = start - server->base;

	if (end < (server->base + server->count))
		norm_end = end - server->base;
	else
		norm_end = server->count - 1;

	/* Characteristic and Included UUIDs get special treatment */
	if (!bt_uuid_cmp(&char_uuid, uuid)) {
		type = GATT_CHARAC_UUID;
		channel->msg = dbus_message_new_method_call(server->name,
				server->path, GATT_SERVER_INTERFACE,
							"ReadByChar");
	} else if (!bt_uuid_cmp(&inc_uuid, uuid)) {
		type = GATT_INCLUDE_UUID;
		channel->msg = dbus_message_new_method_call(server->name,
				server->path, GATT_SERVER_INTERFACE,
							"ReadByInc");
	} else {
		channel->msg = dbus_message_new_method_call(server->name,
				server->path, GATT_SERVER_INTERFACE,
							"ReadByType");
	}

	if (channel->msg == NULL)
		goto failed;

	if (type) {
		dbus_message_append_args(channel->msg,
				DBUS_TYPE_UINT16, &norm_start,
				DBUS_TYPE_UINT16, &norm_end,
				DBUS_TYPE_INVALID);
	} else {
		char uuid_buf[MAX_LEN_UUID_STR];
		char *uuid_str = uuid_buf;
		const char *dev = device_get_path(channel->device);
		const char *auth = sec_level_to_auth(channel);
		bt_uuid_t uuid128;

		bt_uuid_to_uuid128(uuid, &uuid128);
		err = bt_uuid_to_string(&uuid128, uuid_buf, MAX_LEN_UUID_STR);

		if (err < 0)
			goto failed;


		dbus_message_append_args(channel->msg,
				DBUS_TYPE_UINT16, &norm_start,
				DBUS_TYPE_UINT16, &norm_end,
				DBUS_TYPE_STRING, &uuid_str,
				DBUS_TYPE_STRING, &auth,
				DBUS_TYPE_INVALID);
	}

	if (!dbus_connection_send_with_reply(connection, channel->msg,
					&channel->call, REQUEST_TIMEOUT))
		goto failed_dbus;

	channel->op.server = server;

	switch (type) {
	case GATT_CHARAC_UUID:
		dbus_pending_call_set_notify(channel->call, read_by_chr_reply,
								channel, NULL);
		break;
	case GATT_INCLUDE_UUID:
		dbus_pending_call_set_notify(channel->call, read_by_inc_reply,
								channel, NULL);
		break;
	default:
		dbus_pending_call_set_notify(channel->call, read_by_type_reply,
								channel, NULL);
	}
	return;

failed_dbus:
	/* cleanup dbus */
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}
	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	/* Attempt on next server, if one exists */
	if (server != gatt_server_last && server->next) {
		server = server->next;
		goto restart_read_by_type;
	}

	DBG(" Server List End");

failed:
	if (!adl)
		length = enc_error_resp(channel->op.opcode,
					channel->op.u.read_by_type.start,
					status, channel->opdu, channel->mtu);
	else {
		length = enc_read_by_grp_resp(adl, channel->opdu, channel->mtu);
		att_data_list_free(channel->op.u.read_by_type.adl);
	}

	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0], channel->opdu,
					length, NULL, NULL, NULL);
}

static int read_by_type(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid,
						uint8_t *pdu, int len)
{
	struct att_data_list *adl = NULL;
	GSList *l, *types;
	struct attribute *a;
	uint16_t num, length, res_len;
	uint8_t status;
	gboolean terminated = FALSE;
	int i;

	DBG("start:0x%04x end:0x%04x", start, end);

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_READ_BY_TYPE_REQ, start,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	if (gatt_server_list && gatt_server_list->base <= start)
		goto empty_list;

	for (l = database, length = 0, types = NULL; l; l = l->next) {
		struct attribute *client_attr;

		a = l->data;

		if (a->handle < start)
			continue;

		if (a->handle > end) {
			terminated = TRUE;
			break;
		}

		if (bt_uuid_cmp(&a->uuid, uuid) != 0)
			continue;

		client_attr = client_cfg_attribute(channel, a);
		if (client_attr)
			a = client_attr;

		status = att_check_reqs(channel, ATT_OP_READ_BY_TYPE_REQ,
								a->read_reqs);

		if (status == 0x00 && a->read_cb)
			status = a->read_cb(a, a->cb_user_data);

		if (status) {
			g_slist_free(types);
			return enc_error_resp(ATT_OP_READ_BY_TYPE_REQ,
						a->handle, status, pdu, len);
		}

		/* All elements must have the same length */
		if (length == 0)
			length = a->len;
		else if (a->len != length) {
			terminated = TRUE;
			break;
		}

		/* Work-around for having only one client_cfg attribute storage
		 * location. Return builtin cli cfg attributes one at a time.
		 */
		if (a == client_attr && types)
			break;

		types = g_slist_append(types, a);
	}

	if (types == NULL) {
		if (terminated || gatt_server_list == NULL ||
						gatt_server_list->base > end)
			return enc_error_resp(ATT_OP_READ_BY_TYPE_REQ, start,
					ATT_ECODE_ATTR_NOT_FOUND, pdu, len);
		else
			goto empty_list;
	}

	/* Handle length plus attribute value length */
	length += 2;
	res_len = length;
	num = (len - 2) / length;

	adl = att_data_list_alloc(num, length);

	for (i = 0, l = types; l && i < adl->num; i++, l = l->next) {
		uint8_t *value;

		a = l->data;

		adl->cnt++;
		value = (void *) adl->data[i];

		att_put_u16(a->handle, value);

		/* Attribute Value */
		memcpy(&value[2], a->data, a->len);
	}

	g_slist_free(types);

	/* If no more room, or we are not at end of builtin list, send now */
	if (adl->cnt == adl->num || terminated ||
			gatt_server_list == NULL ||
			gatt_server_list->base > end) {

		length = enc_read_by_type_resp(adl, pdu, len);
		att_data_list_free(adl);
		return length;
	}

empty_list:
	channel->op.opcode = ATT_OP_READ_BY_TYPE_REQ;
	channel->op.u.read_by_type.adl = adl;
	channel->op.u.read_by_type.start = start;
	channel->op.u.read_by_type.end = end;
	channel->op.u.read_by_type.uuid = *uuid;
	dbus_read_by_type(channel, start, end, uuid);
	return -1;
}

static void dbus_find_info(struct gatt_channel *channel, uint16_t start,
								uint16_t end);

static void find_info_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct att_data_list *adl = channel->op.u.find_info.adl;
	struct gatt_server *server = channel->op.server;
	DBusMessage *message;
	DBusError err;
	uint16_t length, handle;
	uint8_t *value, res_size;
	uint8_t att_err = ATT_ECODE_ATTR_NOT_FOUND;
	const char *uuid_str;
	bt_uuid_t uuid, result_uuid;
	gboolean terminated = FALSE;

	DBG("");

	/* Init handle to end of this server in case of error */
	handle = server->base + server->count;

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		error("Server replied with an error: %s, %s",
				err.name, err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_STRING, &uuid_str,
				DBUS_TYPE_INVALID)) {
		error("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	handle += server->base;

	bt_string_to_uuid(&uuid, uuid_str);

	if (!adl || adl->len == 4) {
		bt_uuid_to_uuid16(&uuid, &result_uuid);
		if (result_uuid.type == BT_UUID16) {
			res_size = 4;
		} else {
			bt_uuid_to_uuid128(&uuid, &result_uuid);
			res_size = 18;
		}
	} else {
		bt_uuid_to_uuid128(&uuid, &result_uuid);
		res_size = 18;
	}


	if (!adl) {
		adl = att_data_list_alloc((channel->mtu - 2)/res_size, res_size);
	} else if (res_size != adl->len) {
		terminated = TRUE;
		goto cleanup_dbus;
	}

	channel->op.u.find_info.adl = adl;
	value = (void *) adl->data[adl->cnt++];

	/* Attribute Handle */
	att_put_u16(handle, value);

	/* Attribute Value */
	if (result_uuid.type == BT_UUID16)
		att_put_u16(result_uuid.value.u16, &value[2]);
	else
		att_put_u128(result_uuid.value.u128, &value[2]);

	handle += 1;

	/* End of attribute range reached */
	if (!handle || handle > channel->op.u.find_info.end)
		terminated = TRUE;

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	if (adl && adl->num == adl->cnt) {
		terminated = TRUE;
		goto done;
	}

	/* See if any other servers could provide results */
	while (server && (handle >= (server->base + server->count)))
		server = server->next;

	if (!server)
		terminated = TRUE;

done:
	if (terminated && adl) {
		uint8_t format = (4 == adl->len) ? 1 : 2;

		/* send results */
		length = enc_find_info_resp(format, adl, channel->opdu,
							channel->mtu);
		att_data_list_free(adl);
		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	if (terminated) {
		/* Or send NOT FOUND */
		length = enc_error_resp(ATT_OP_FIND_INFO_REQ,
					channel->op.u.find_info.start,
					att_err, channel->opdu, channel->mtu);

		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	dbus_find_info(channel, handle, channel->op.u.find_info.end);
}

static void dbus_find_info(struct gatt_channel *channel, uint16_t start,
								uint16_t end)
{
	struct gatt_server *server = gatt_server_list;
	struct att_data_list *adl = channel->op.u.find_info.adl;
	uint16_t norm_start = 0, norm_end, length;
	uint8_t status = ATT_ECODE_ATTR_NOT_FOUND;
	int err;

	DBG("start:0x%04x end:0x%04x", start, end);

	while (server && (server->base + server->count) <= start)
		server = server->next;

	if (!server)
		goto failed;

	if (start > server->base)
		norm_start = start - server->base;

	if (end < (server->base + server->count))
		norm_end = end - server->base;
	else
		norm_end = server->count - 1;

	channel->msg = dbus_message_new_method_call(server->name, server->path,
					GATT_SERVER_INTERFACE, "FindInfo");
	if (channel->msg == NULL)
		goto failed;

	dbus_message_append_args(channel->msg, DBUS_TYPE_UINT16, &norm_start,
						DBUS_TYPE_UINT16, &norm_end,
						DBUS_TYPE_INVALID);

	if (!dbus_connection_send_with_reply(connection, channel->msg,
					&channel->call, REQUEST_TIMEOUT))
		goto failed_dbus;

	channel->op.server = server;
	dbus_pending_call_set_notify(channel->call, find_info_reply,
								channel, NULL);
	return;

failed_dbus:
	/* cleanup dbus */
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}
	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	/* Attempt on next server, if one exists */
	if (server != gatt_server_last && server->next) {
		server = server->next;
		start = server->base;

		DBG(" Try Next %s, %s 0x%04x,0x%04x", server->name, server->path, start, end);

		dbus_find_info(channel, start, end);
		return;
	}
	DBG(" Server List End");

failed:
	if (!adl)
		length = enc_error_resp(channel->op.opcode,
					channel->op.u.find_info.start,
					status, channel->opdu, channel->mtu);
	else {
		uint8_t format = (4 == adl->len) ? 1 : 2;
		length = enc_find_info_resp(format, adl, channel->opdu, channel->mtu);
		att_data_list_free(channel->op.u.find_info.adl);
	}

	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0], channel->opdu,
					length, NULL, NULL, NULL);
}

static int find_info(struct gatt_channel *channel, uint16_t start,
					uint16_t end, uint8_t *pdu, int len)
{
	struct attribute *a;
	struct att_data_list *adl = NULL;
	GSList *l, *info;
	uint8_t format, last_type = BT_UUID_UNSPEC;
	uint16_t num, length, res_len;
	gboolean terminated = FALSE;
	int i;

	DBG("start:0x%04x end:0x%04x", start, end);

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_FIND_INFO_REQ, start,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	if (gatt_server_list && gatt_server_list->base <= start)
		goto empty_list;

	for (l = database, info = NULL, num = 0; l; l = l->next) {
		a = l->data;

		if (a->handle < start)
			continue;

		if (a->handle > end) {
			terminated = TRUE;
			break;
		}

		if (last_type == BT_UUID_UNSPEC)
			last_type = a->uuid.type;

		if (a->uuid.type != last_type) {
			terminated = TRUE;
			break;
		}

		info = g_slist_append(info, a);
		num++;

		last_type = a->uuid.type;
	}

	if (info == NULL) {
		if (terminated || gatt_server_list == NULL ||
						gatt_server_list->base > end)
			return enc_error_resp(ATT_OP_FIND_INFO_REQ, start,
					ATT_ECODE_ATTR_NOT_FOUND, pdu, len);
		else
			goto empty_list;
	}

	if (last_type == BT_UUID16) {
		length = 2;
		format = 0x01;
	} else if (last_type == BT_UUID128) {
		length = 16;
		format = 0x02;
	} else {
		g_slist_free(info);
		return 0;
	}

	length += 2;
	res_len = length;
	num = (len - 2) / length;

	adl = att_data_list_alloc(num, length);

	for (i = 0, l = info; l && i < adl->num; i++, l = l->next) {
		uint8_t *value;

		a = l->data;

		adl->cnt++;
		value = (void *) adl->data[i];

		att_put_u16(a->handle, value);

		/* Attribute Value */
		att_put_uuid(a->uuid, &value[2]);
	}

	g_slist_free(info);

	/* If no more room, or we are not at end of builtin list, send now */
	if (adl->cnt == adl->num || terminated ||
			gatt_server_list == NULL ||
			gatt_server_list->base > end) {

		length = enc_find_info_resp(format, adl, pdu, len);
		att_data_list_free(adl);
		return length;
	}


empty_list:
	channel->olen = (uint16_t) length;
	channel->op.opcode = ATT_OP_FIND_INFO_REQ;
	channel->op.u.find_info.adl = adl;
	channel->op.u.find_info.start = start;
	channel->op.u.find_info.end = end;
	dbus_find_info(channel, start, end);
	return -1;
}

static void dbus_find_by_type(struct gatt_channel *channel, uint16_t start,
					uint16_t end, uint16_t type,
					const uint8_t *value, uint8_t vlen);

static void find_by_type_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct att_data_list *adl = channel->op.u.find_by_type.adl;
	struct gatt_server *server = channel->op.server;
	DBusMessage *message;
	DBusError err;
	uint16_t length, start, end;
	uint8_t *value;
	uint8_t att_err = ATT_ECODE_ATTR_NOT_FOUND;
	gboolean terminated = FALSE;

	DBG("");

	/* Init handle to end of this server in case of error */
	start = server->base + server->count;

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		error("Server replied with an error: %s, %s",
				err.name, err.message);
		dbus_error_free(&err);
		start = server->base + server->count;
		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
				DBUS_TYPE_UINT16, &start,
				DBUS_TYPE_UINT16, &end,
				DBUS_TYPE_INVALID)) {
		error("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
		start = server->base + server->count;
		goto cleanup_dbus;
	}

	/* Range check returned result */
	if (start > end || end > server->count) {
		start = server->base + server->count;
		if (!start)
			terminated = TRUE;

		goto cleanup_dbus;
	}

	start += server->base;
	end += server->base;

	if (!adl) {
		adl = att_data_list_alloc((channel->mtu - 1)/4, 4);
	} else if (4 != adl->len) {
		terminated = TRUE;
		goto cleanup_dbus;
	}

	channel->op.u.find_by_type.adl = adl;
	value = (void *) adl->data[adl->cnt++];

	/* Attribute Handle range */
	att_put_u16(start, value);
	att_put_u16(end, &value[2]);

	start = end + 1;

	/* End of attribute range reached */
	if (!start || start > channel->op.u.find_by_type.end)
		terminated = TRUE;

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	if (adl && adl->num == adl->cnt) {
		terminated = TRUE;
		goto done;
	}

	/* See if any other servers could provide results */
	while (server && (start >= (server->base + server->count)))
		server = server->next;

	if (!server)
		terminated = TRUE;

done:
	if (terminated && adl) {
		/* send results */
		length = enc_find_by_type_resp(adl, channel->opdu,
							channel->mtu);
		att_data_list_free(adl);
		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	if (terminated) {
		/* Or send NOT FOUND */
		length = enc_error_resp(ATT_OP_FIND_BY_TYPE_REQ,
					channel->op.u.find_by_type.start,
					att_err, channel->opdu, channel->mtu);

		channel->op.opcode = 0;
		server_resp(channel->attrib, 0, channel->opdu[0],
				channel->opdu, length, NULL, NULL, NULL);
		return;
	}

	dbus_find_by_type(channel, start, channel->op.u.find_by_type.end,
					channel->op.u.find_by_type.type,
					channel->op.u.find_by_type.value,
					channel->op.u.find_by_type.vlen);
}

static void dbus_find_by_type(struct gatt_channel *channel, uint16_t start,
					uint16_t end, uint16_t type,
					const uint8_t *value, uint8_t vlen)
{
	struct gatt_server *server = gatt_server_list;
	struct att_data_list *adl = channel->op.u.find_by_type.adl;
	uint16_t norm_start = 0, norm_end, length;
	uint8_t status = ATT_ECODE_ATTR_NOT_FOUND;
	bt_uuid_t uuid, uuid128;
	char uuid_buf[MAX_LEN_UUID_STR];
	char *uuid_str = uuid_buf;
	int err;

	DBG("start:0x%04x end:0x%04x", start, end);

	while (server && (server->base + server->count) <= start)
		server = server->next;

	if (!server)
		goto failed;

	if (start > server->base)
		norm_start = start - server->base;

	if (end < (server->base + server->count))
		norm_end = end - server->base;
	else
		norm_end = server->count - 1;

	bt_uuid16_create(&uuid, type);
	if (!bt_uuid_cmp(&prim_uuid, &uuid)) {
		if (vlen == 16)
			bt_uuid128_create(&uuid, att_get_u128(value));
		else
			bt_uuid16_create(&uuid, att_get_u16(value));

		bt_uuid_to_uuid128(&uuid, &uuid128);
		err = bt_uuid_to_string(&uuid128, uuid_buf, MAX_LEN_UUID_STR);
		if (err < 0)
			goto failed;

		channel->msg = dbus_message_new_method_call(server->name,
				server->path, GATT_SERVER_INTERFACE,
				"FindByPrim");

		if (channel->msg == NULL)
			goto failed;

		dbus_message_append_args(channel->msg,
					DBUS_TYPE_UINT16, &norm_start,
					DBUS_TYPE_UINT16, &norm_end,
					DBUS_TYPE_STRING, &uuid_str,
					DBUS_TYPE_INVALID);

		goto send_dbus;
	}

	bt_uuid_to_uuid128(&uuid, &uuid128);
	err = bt_uuid_to_string(&uuid128, uuid_buf, MAX_LEN_UUID_STR);

	if (err < 0)
		goto failed;

	channel->msg = dbus_message_new_method_call(server->name, server->path,
					GATT_SERVER_INTERFACE, "FindByType");
	if (channel->msg == NULL)
		goto failed;

	dbus_message_append_args(channel->msg, DBUS_TYPE_UINT16, &norm_start,
				DBUS_TYPE_UINT16, &norm_end,
				DBUS_TYPE_STRING, &uuid_str,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &value, vlen,
				DBUS_TYPE_INVALID);

send_dbus:
	if (!dbus_connection_send_with_reply(connection, channel->msg,
					&channel->call, REQUEST_TIMEOUT))
		goto failed_dbus;

	channel->op.server = server;
	dbus_pending_call_set_notify(channel->call, find_by_type_reply,
								channel, NULL);
	return;

failed_dbus:
	/* cleanup dbus */
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}
	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	/* Attempt on next server, if one exists */
	if (server != gatt_server_last && server->next) {
		server = server->next;
		start = server->base;

		DBG(" Try Next %s, %s 0x%04x,0x%04x", server->name, server->path, start, end);

		dbus_find_by_type(channel, start, end, type, value, vlen);
		return;
	}
	DBG(" Server List End");

failed:
	if (!adl)
		length = enc_error_resp(channel->op.opcode,
					channel->op.u.find_by_type.start,
					status, channel->opdu, channel->mtu);
	else {
		length = enc_find_by_type_resp(adl, channel->opdu, channel->mtu);
		att_data_list_free(channel->op.u.find_by_type.adl);
	}

	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0], channel->opdu,
					length, NULL, NULL, NULL);
}

static int find_by_type(struct gatt_channel *channel, uint16_t start,
			uint16_t end, bt_uuid_t *uuid, const uint8_t *value,
					int vlen, uint8_t *opdu, int len)
{
	struct attribute *a;
	struct att_data_list *adl = NULL;
	struct att_range *range;
	GSList *l, *matches;
	bt_uuid_t srch_uuid, tmp_uuid;
	uint16_t length;
	int i;
	gboolean compare, terminated = FALSE;

	DBG("start:0x%04x end:0x%04x", start, end);

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_FIND_BY_TYPE_REQ, start,
					ATT_ECODE_INVALID_HANDLE, opdu, len);

	if (vlen != sizeof(struct server_def_val16) &&
			vlen != sizeof(struct server_def_val128))
		return enc_error_resp(ATT_OP_FIND_BY_TYPE_REQ, start,
					ATT_ECODE_INVALID_PDU, opdu, len);

	/* Any Primary service discovery updates the clients serial number */
	if (channel->serial < serial_num && !bt_uuid_cmp(uuid, &prim_uuid))
		update_client_serial(channel);

	if (gatt_server_list && gatt_server_list->base <= start)
		goto empty_list;

	if (vlen == sizeof(struct server_def_val128))
		bt_uuid128_create(&srch_uuid, att_get_u128(value));
	else
		bt_uuid16_create(&srch_uuid, att_get_u16(value));

	/* Searching first requested handle number */
	for (l = database, matches = NULL, range = NULL; l; l = l->next) {
		a = l->data;

		if (a->handle < start)
			continue;

		if (a->handle > end) {
			terminated = TRUE;
			break;
		}

		/* Convert attribute value to UUID for generic UUID compares */
		if (a->len == sizeof(struct server_def_val16) ||
				a->len == sizeof(struct server_def_val128)) {
			compare = TRUE;
			if (a->len == sizeof(struct server_def_val128))
				bt_uuid128_create(&tmp_uuid,
						att_get_u128(a->data));
			else
				bt_uuid16_create(&tmp_uuid,
						att_get_u16(a->data));
		} else {
			compare = FALSE;
		}

		/* Attribute value UUID matches? */
		if (compare && bt_uuid_cmp(&a->uuid, uuid) == 0 &&
				bt_uuid_cmp(&tmp_uuid, &srch_uuid) == 0) {

			range = g_new0(struct att_range, 1);
			range->start = a->handle;
			/* It is allowed to have end group handle the same as
			 * start handle, for groups with only one attribute. */
			range->end = a->handle;

			matches = g_slist_append(matches, range);
		} else if (range) {
			/* Update the last found handle or reset the pointer
			 * to track that a new group started: Primary or
			 * Secondary service. */
			if (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
					bt_uuid_cmp(&a->uuid, &snd_uuid) == 0)
				range = NULL;
			else
				range->end = a->handle;
		}
	}

	if (matches == NULL) {
		if (terminated || gatt_server_list == NULL ||
						gatt_server_list->base > end)
			return enc_error_resp(ATT_OP_FIND_BY_TYPE_REQ, start,
				ATT_ECODE_ATTR_NOT_FOUND, opdu, len);
		else
			goto empty_list;
	}

	adl = att_data_list_alloc((channel->mtu - 1)/4, 4);

	for (i = 0, l = matches; l && i < adl->num; l = l->next, i++) {
		uint8_t *value;

		range = l->data;

		adl->cnt++;
		value = (void *) adl->data[i];

		/* Attribute Handles */
		att_put_u16(range->start, value);
		att_put_u16(range->end, &value[2]);
	}

	g_slist_foreach(matches, (GFunc) g_free, NULL);
	g_slist_free(matches);

	/* If no more room, or we are not at end of db, send now */
	if (adl->cnt == adl->num || terminated ||
			gatt_server_list == NULL ||
			gatt_server_list->base > end) {

		length = enc_find_by_type_resp(adl, opdu, len);
		att_data_list_free(adl);
		return length;
	}

empty_list:
	channel->op.opcode = ATT_OP_FIND_BY_TYPE_REQ;
	channel->op.u.find_by_type.adl = adl;
	channel->op.u.find_by_type.start = start;
	channel->op.u.find_by_type.end = end;
	channel->op.u.find_by_type.type = uuid->value.u16;
	channel->op.u.find_by_type.vlen = (uint8_t) vlen;
	memcpy(channel->op.u.find_by_type.value, value, vlen);
	dbus_find_by_type(channel, start, end, uuid->value.u16, value,
								(uint8_t) vlen);
	return -1;
}

static struct attribute *find_primary_range(uint16_t start, uint16_t *end)
{
	struct attribute *attrib;
	guint h = start;
	GSList *l;

	if (end == NULL)
		return NULL;

	l = g_slist_find_custom(database, GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return NULL;

	attrib = l->data;

	if (bt_uuid_cmp(&attrib->uuid, &prim_uuid) != 0)
		return NULL;

	*end = start;

	for (l = l->next; l; l = l->next) {
		struct attribute *a = l->data;

		if (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
				bt_uuid_cmp(&a->uuid, &snd_uuid) == 0)
			break;

		*end = a->handle;
	}

	return attrib;
}

static void dbus_read(struct gatt_channel *channel, uint16_t handle);

static void read_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct gatt_server *server = channel->op.server;
	DBusMessage *message;
	DBusError err;
	uint16_t handle, length = 0;
	uint8_t dst[ATT_DEFAULT_LE_MTU];
	uint8_t att_err = 0;
	const uint8_t *value;
	const char *uuid_str;
	bt_uuid_t uuid;
	dbus_int32_t cnt;
	int mas_ret;

	DBG("");

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		att_err = map_dbus_error(&err, &handle);
		error("Server replied with an error: %s, %s (0x%x)",
			err.name, err.message, att_err);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
				DBUS_TYPE_STRING, &uuid_str,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &value, &cnt,
				DBUS_TYPE_INVALID)) {
		att_err = ATT_ECODE_UNLIKELY;
		error("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	bt_string_to_uuid(&uuid, uuid_str);

	/* Massage payload */
	mas_ret = massage_payload(&uuid, server->base,
			server->base + server->count, channel,
			channel->op.u.read_blob.handle,
			value, cnt, dst, 0);

	/* If error during massage, return failure */
	if (mas_ret < 0) {
		att_err = ATT_ECODE_UNLIKELY;
		goto cleanup_dbus;
	}

	/* If data needed massage, use massaged data */
	if (mas_ret) {
		value = dst;
		cnt = (dbus_int32_t) mas_ret;
	}

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

done:
	if (!att_err) {
		/* send results */
		if (channel->op.opcode == ATT_OP_READ_REQ)
			length = enc_read_resp(value, cnt, channel->opdu,
					channel->mtu);
		else if (channel->op.opcode == ATT_OP_READ_BLOB_REQ) {
			if (cnt <= channel->op.u.read_blob.offset)
				att_err = ATT_ECODE_INVALID_OFFSET;
			else
				length = enc_read_blob_resp(value, cnt,
					channel->op.u.read_blob.offset,
					channel->opdu, channel->mtu);
		} else {
			att_err = ATT_ECODE_UNLIKELY;
		}
	}

	/* Or send ATT ERR */
	if (!length)
		length = enc_error_resp(channel->op.opcode,
				channel->op.u.read_blob.handle,
				att_err, channel->opdu, channel->mtu);

	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0],
			channel->opdu, length, NULL, NULL, NULL);
}

static void dbus_read(struct gatt_channel *channel, uint16_t handle)
{
	struct gatt_server *server = gatt_server_list;
	uint8_t att_err = ATT_ECODE_ATTR_NOT_FOUND;
	const char *dev = device_get_path(channel->device);
	const char *auth = sec_level_to_auth(channel);
	uint16_t length;

	DBG("handle:0x%04x", handle);

	while (server && (server->base + server->count) <= handle)
		server = server->next;

	if (!server)
		goto failed;

	/* Apply Carrier restriction if appropriate */
	if (server->carrier == CARRIER_BR_ONLY && channel->le) {
		att_err = ATT_ECODE_INVALID_TRANSPORT;
		goto failed;
	}

	if (server->carrier == CARRIER_LE_ONLY && !channel->le) {
		att_err = ATT_ECODE_INVALID_TRANSPORT;
		goto failed;
	}

	if (handle >= server->base)
		handle -= server->base;

	channel->msg = dbus_message_new_method_call(server->name, server->path,
					GATT_SERVER_INTERFACE, "Read");
	if (channel->msg == NULL) {
		att_err = ATT_ECODE_UNLIKELY;
		goto failed;
	}

	dbus_message_append_args(channel->msg, DBUS_TYPE_UINT16, &handle,
						DBUS_TYPE_STRING, &auth,
						DBUS_TYPE_INVALID);

	if (!dbus_connection_send_with_reply(connection, channel->msg,
					&channel->call, REQUEST_TIMEOUT)) {
		att_err = ATT_ECODE_UNLIKELY;
		goto failed_dbus;
	}

	channel->op.server = server;
	dbus_pending_call_set_notify(channel->call, read_reply, channel, NULL);
	return;

failed_dbus:
	/* cleanup dbus */
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}
	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

failed:
	length = enc_error_resp(channel->op.opcode,
			channel->op.u.read_blob.handle,
			att_err, channel->opdu, channel->mtu);

	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0], channel->opdu,
					length, NULL, NULL, NULL);
}

static int read_value(struct gatt_channel *channel, uint16_t handle,
							uint8_t *pdu, int len)
{
	struct attribute *a, *client_attr;
	uint8_t status;
	GSList *l;
	guint h = handle;

	DBG("handle:0x%04x", handle);

	if (gatt_server_list && gatt_server_list->base <= handle) {
		channel->op.opcode = ATT_OP_READ_REQ;
		channel->op.u.read_blob.handle = handle;
		channel->op.u.read_blob.offset = 0;
		dbus_read(channel, handle);
		return -1;
	}

	l = g_slist_find_custom(database, GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return enc_error_resp(ATT_OP_READ_REQ, handle,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	a = l->data;

	client_attr = client_cfg_attribute(channel, a);
	if (client_attr)
		a = client_attr;

	status = att_check_reqs(channel, ATT_OP_READ_REQ, a->read_reqs);

	if (status == 0x00 && a->read_cb)
		status = a->read_cb(a, a->cb_user_data);

	if (status)
		return enc_error_resp(ATT_OP_READ_REQ, handle, status, pdu,
									len);

	return enc_read_resp(a->data, a->len, pdu, len);
}

static int read_blob(struct gatt_channel *channel, uint16_t handle,
					uint16_t offset, uint8_t *pdu, int len)
{
	struct attribute *a, *client_attr;
	uint8_t status;
	GSList *l;
	guint h = handle;

	DBG("handle:0x%04x offset:0x%04x", handle, offset);

	if (gatt_server_list && gatt_server_list->base <= handle) {
		channel->op.opcode = ATT_OP_READ_BLOB_REQ;
		channel->op.u.read_blob.handle = handle;
		channel->op.u.read_blob.offset = offset;
		dbus_read(channel, handle);
		return -1;
	}

	l = g_slist_find_custom(database, GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return enc_error_resp(ATT_OP_READ_BLOB_REQ, handle,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	a = l->data;

	client_attr = client_cfg_attribute(channel, a);
	if (client_attr)
		a = client_attr;

	status = att_check_reqs(channel, ATT_OP_READ_BLOB_REQ, a->read_reqs);

	if (!status && a->len <= offset)
		return enc_error_resp(ATT_OP_READ_BLOB_REQ, handle,
					ATT_ECODE_INVALID_OFFSET, pdu, len);

	if (status == 0x00 && a->read_cb)
		status = a->read_cb(a, a->cb_user_data);

	if (status)
		return enc_error_resp(ATT_OP_READ_BLOB_REQ, handle, status,
								pdu, len);

	return enc_read_blob_resp(a->data, a->len, offset, pdu, len);
}

static void write_reply(DBusPendingCall *call, void *user_data)
{
	struct gatt_channel *channel = user_data;
	DBusMessage *message;
	const char *uuid_str;
	bt_uuid_t uuid;
	DBusError err;
	uint16_t handle, length;
	uint8_t att_err = 0;

	DBG("");

	/* steal_reply will always return non-NULL since the callback
	 * is only called after a reply has been received */
	message = dbus_pending_call_steal_reply(call);

	if (!is_channel_valid(channel)) {
		dbus_message_unref(message);
		return;
	}

	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		att_err = map_dbus_error(&err, &handle);
		DBG("Server replied with an error: %s, %s (0x%x)",
			err.name, err.message, att_err);
		dbus_error_free(&err);
		goto cleanup_dbus;
	}

	dbus_error_init(&err);
	if (!dbus_message_get_args(message, &err,
				DBUS_TYPE_STRING, &uuid_str,
				DBUS_TYPE_INVALID)) {
		att_err = ATT_ECODE_UNLIKELY;
		error("Wrong reply signature: %s", err.message);
		dbus_error_free(&err);
	}

	bt_string_to_uuid(&uuid, uuid_str);

	if (!bt_uuid_cmp(&uuid, &clicfg_uuid) &&
			channel->op.u.write.vlen == 2)
		cache_cli_cfg(channel, channel->op.u.write.handle,
						channel->op.u.write.value);

cleanup_dbus:
	dbus_message_unref(message);
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

done:
	/* send result */
	if (!att_err)
		length = enc_write_resp(channel->opdu, channel->mtu);
	else
		length = enc_error_resp(ATT_OP_WRITE_REQ,
				channel->op.u.write.handle,
				att_err, channel->opdu, channel->mtu);

	g_free(channel->op.u.write.value);
	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0],
			channel->opdu, length, NULL, NULL, NULL);
}

static void dbus_write(struct gatt_channel *channel, uint16_t handle,
						const uint8_t *value, int vlen)
{
	struct gatt_server *server = gatt_server_list;
	uint8_t att_err = ATT_ECODE_INVALID_HANDLE;
	const char *dev = device_get_path(channel->device);
	const char *auth = sec_level_to_auth(channel);
	uint16_t length;

	DBG("handle:0x%04x", handle);

	while (server && (server->base + server->count) <= handle)
		server = server->next;

	if (!server)
		goto failed;

	if (handle >= server->base)
		handle -= server->base;

	channel->msg = dbus_message_new_method_call(server->name,
				server->path, GATT_SERVER_INTERFACE, "Write");

	if (channel->msg == NULL) {
		att_err = ATT_ECODE_UNLIKELY;
		goto failed;
	}

	dbus_message_append_args(channel->msg,
				DBUS_TYPE_UINT32, &channel->session,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &value, vlen,
				DBUS_TYPE_STRING, &auth,
				DBUS_TYPE_INVALID);

	if (!dbus_connection_send_with_reply(connection, channel->msg,
					&channel->call, REQUEST_TIMEOUT)) {
		att_err = ATT_ECODE_UNLIKELY;
		goto failed_dbus;
	}

	dbus_pending_call_set_notify(channel->call, write_reply, channel, NULL);
	return;

failed_dbus:
	/* cleanup dbus */
	if (channel->msg) {
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}
	if (channel->call) {
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

failed:
	g_free(channel->op.u.write.value);
	length = enc_error_resp(ATT_OP_WRITE_REQ,
			channel->op.u.write.handle,
			att_err, channel->opdu, channel->mtu);

	channel->op.opcode = 0;
	server_resp(channel->attrib, 0, channel->opdu[0],
			channel->opdu, length, NULL, NULL, NULL);
}

static void dbus_writecmd(struct gatt_channel *channel, uint16_t handle,
						const uint8_t *value, int vlen)
{
	struct gatt_server *server = gatt_server_list;
	uint8_t att_err = ATT_ECODE_INVALID_HANDLE;
	const char *auth = sec_level_to_auth(channel);
	uint16_t length;

	DBG("handle:0x%04x", handle);

	while (server && (server->base + server->count) <= handle)
		server = server->next;

	if (!server)
		return;

	if (handle >= server->base)
		handle -= server->base;

	channel->msg = dbus_message_new_method_call(server->name,
			server->path, GATT_SERVER_INTERFACE, "WriteCmd");

	if (channel->msg == NULL)
		return;

	dbus_message_append_args(channel->msg, DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &value, vlen,
				DBUS_TYPE_STRING, &auth,
				DBUS_TYPE_INVALID);

	dbus_connection_send(connection, channel->msg, NULL);
	dbus_message_unref(channel->msg);
	channel->msg = NULL;
}

static int write_value(struct gatt_channel *channel, gboolean resp,
				uint16_t handle, const uint8_t *value, int vlen,
							uint8_t *pdu, int len)
{
	struct attribute *a, *client_attr;
	uint8_t status;
	GSList *l;
	guint h = handle;

	DBG("handle:0x%04x", handle);

	if (gatt_server_list && gatt_server_list->base <= handle) {
		if (resp) {
			if (value && vlen)
				channel->op.u.write.value = g_malloc0(vlen);
			else
				channel->op.u.write.value = NULL;

			if (channel->op.u.write.value)
				memcpy(channel->op.u.write.value, value, vlen);

			channel->op.opcode = ATT_OP_WRITE_REQ;
			channel->op.u.write.handle = handle;
			channel->op.u.write.vlen = vlen;
			dbus_write(channel, handle, value, vlen);
		} else {
			dbus_writecmd(channel, handle, value, vlen);
		}
		return -1;
	}

	l = g_slist_find_custom(database, GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return enc_error_resp(ATT_OP_WRITE_REQ, handle,
				ATT_ECODE_INVALID_HANDLE, pdu, len);

	a = l->data;

	status = att_check_reqs(channel, ATT_OP_WRITE_REQ, a->write_reqs);
	if (status)
		return enc_error_resp(ATT_OP_WRITE_REQ, handle, status, pdu,
									len);

	client_attr = client_cfg_attribute(channel, a);
	if (client_attr) {
		a = client_attr;
		memcpy(a->data, value, 2);
	} else
		attrib_db_update(a->handle, NULL, value, vlen, &a);

	if (a->write_cb) {
		status = a->write_cb(a, a->cb_user_data);
		if (status)
			return enc_error_resp(ATT_OP_WRITE_REQ, handle, status,
								pdu, len);
	}

	DBG("Notifications: %d, indications: %d",
					g_slist_length(channel->notify),
					g_slist_length(channel->indicate));

	return enc_write_resp(pdu, len);
}

static int mtu_exchange(struct gatt_channel *channel, uint16_t mtu,
		uint8_t *pdu, int len)
{
	guint old_mtu = channel->mtu;

	DBG("mtu:0x%04x", mtu);

	if (mtu < ATT_DEFAULT_LE_MTU)
		channel->mtu = ATT_DEFAULT_LE_MTU;
	else
		channel->mtu = MIN(mtu, channel->mtu);

	bt_io_set(le_io, BT_IO_L2CAP, NULL,
			BT_IO_OPT_OMTU, channel->mtu,
			BT_IO_OPT_INVALID);

	return enc_mtu_resp(old_mtu, pdu, len);
}

static void zero_cli_cfg(char *key, char *value, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct gatt_server *server = gatt_server_list;
	DBusMessage *msg;
	uint16_t handle = (uint16_t) strtol(key, NULL, 16);
	uint8_t tmp_buf[] = {0,0};
	uint8_t *buf = tmp_buf;

	while (server && (server->base + server->count) <= handle)
		server = server->next;

	if (!server)
		return;

	handle -= server->base;

	msg = dbus_message_new_method_call(server->name,
			server->path, GATT_SERVER_INTERFACE, "UpdateClientConfig");

	if (msg == NULL)
		return;

	dbus_message_append_args(msg,
				DBUS_TYPE_UINT32, &channel->session,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &buf, 2,
				DBUS_TYPE_INVALID);

	dbus_connection_send(connection, msg, NULL);
	dbus_message_unref(msg);
}

static void ind_return(guint8 status, const guint8 *pdu, guint16 len,
								gpointer data);
static void channel_destroy(void *user_data)
{
	struct gatt_channel *channel = user_data;
	char filename[PATH_MAX + 1];

	DBG("channel: %p", user_data);


	make_cli_cfg_name(filename, channel);
	textfile_foreach(filename, zero_cli_cfg, channel);

	if (channel->ind_msg) {
		DBG(" return_failure channel->ind_msg");
		ind_return(ATT_ECODE_UNLIKELY, NULL, 0, channel);
	}

	clients = g_slist_remove(clients, channel);

	if (channel->msg) {
		DBG("channel_disconnect channel->msg");
		dbus_message_unref(channel->msg);
		channel->msg = NULL;
	}

	if (channel->call) {
		DBG("channel_disconnect channel->call");
		dbus_pending_call_unref(channel->call);
		channel->call = NULL;
	}

	g_slist_free(channel->notify);
	g_slist_free(channel->indicate);
	g_attrib_set_disconnect_server_function(channel->attrib, NULL, NULL);
	g_attrib_set_destroy_function(channel->attrib, NULL, NULL);

	g_free(channel);
}

static void channel_disconnect(void *user_data)
{
	struct gatt_channel *channel = user_data;
	GAttrib *attrib = channel->attrib;
	DBG("");

	channel_destroy(channel);
	g_attrib_unref(attrib);
}

static void channel_handler(const uint8_t *ipdu, uint16_t len,
							gpointer user_data)
{
	struct gatt_channel *channel = user_data;
	uint8_t value[ATT_MAX_MTU];
	uint16_t start, end, mtu, offset;
	int length;
	bt_uuid_t uuid;
	uint8_t status = 0;
	int vlen;

	DBG("op 0x%02x", ipdu[0]);

	if (channel->op.opcode) {
		if (ipdu[0] == ATT_OP_WRITE_CMD ||
				ipdu[0] == ATT_OP_HANDLE_CNF ||
				ipdu[0] == ATT_OP_SIGNED_WRITE_CMD)
			return;

		g_attrib_ref(channel->attrib);
		status = ATT_ECODE_INVALID_PDU;
		goto done;
	}

	g_attrib_ref(channel->attrib);

	switch (ipdu[0]) {
	case ATT_OP_READ_BY_GROUP_REQ:
		length = dec_read_by_grp_req(ipdu, len, &start, &end, &uuid);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_by_group(channel, start, end, &uuid,
						channel->opdu, channel->mtu);
		break;
	case ATT_OP_READ_BY_TYPE_REQ:
		length = dec_read_by_type_req(ipdu, len, &start, &end, &uuid);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_by_type(channel, start, end, &uuid, channel->opdu,
								channel->mtu);
		break;
	case ATT_OP_READ_REQ:
		length = dec_read_req(ipdu, len, &start);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_value(channel, start, channel->opdu,
								channel->mtu);
		break;
	case ATT_OP_READ_BLOB_REQ:
		length = dec_read_blob_req(ipdu, len, &start, &offset);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_blob(channel, start, offset, channel->opdu,
								channel->mtu);
		break;
	case ATT_OP_MTU_REQ:
		if (!channel->le) {
			status = ATT_ECODE_REQ_NOT_SUPP;
			goto done;
		}

		length = dec_mtu_req(ipdu, len, &mtu);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = mtu_exchange(channel, mtu, channel->opdu,
								channel->mtu);
		break;
	case ATT_OP_FIND_INFO_REQ:
		length = dec_find_info_req(ipdu, len, &start, &end);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = find_info(channel, start, end, channel->opdu, channel->mtu);
		break;
	case ATT_OP_WRITE_REQ:
		length = dec_write_req(ipdu, len, &start, value, &vlen);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = write_value(channel, TRUE, start, value, vlen,
						channel->opdu, channel->mtu);
		break;
	case ATT_OP_WRITE_CMD:
		length = dec_write_cmd(ipdu, len, &start, value, &vlen);
		if (length > 0)
			write_value(channel, FALSE, start, value, vlen,
						channel->opdu, channel->mtu);
		g_attrib_unref(channel->attrib);
		return;
	case ATT_OP_FIND_BY_TYPE_REQ:
		length = dec_find_by_type_req(ipdu, len, &start, &end,
							&uuid, value, &vlen);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = find_by_type(channel, start, end, &uuid, value,
					vlen, channel->opdu, channel->mtu);
		break;
	case ATT_OP_HANDLE_CNF:
	case ATT_OP_SIGNED_WRITE_CMD:
		g_attrib_unref(channel->attrib);
		return;
	case ATT_OP_READ_MULTI_REQ:
	case ATT_OP_PREP_WRITE_REQ:
	case ATT_OP_EXEC_WRITE_REQ:
	default:
		DBG("Unsupported request 0x%02x", ipdu[0]);
		status = ATT_ECODE_REQ_NOT_SUPP;
		goto done;
	}

	/* If handled Asyncronously, return is negative */
	if (length < 0)
		return;

	if (length == 0)
		status = ATT_ECODE_IO;

done:
	if (status)
		length = enc_error_resp(ipdu[0], 0x0000, status, channel->opdu,
								channel->mtu);

	server_resp(channel->attrib, 0, channel->opdu[0], channel->opdu,
						length, NULL, NULL, NULL);
}

static void sci_return(guint8 status, const guint8 *pdu, guint16 len,
								gpointer data)
{
	struct gatt_channel *channel = NULL;
	DBusMessage *reply;
	GSList *l;

	for (l = clients; l; l = l->next) {
		if (l->data == data) {
			channel = data;
			break;
		}
	}

	if (!channel)
		return;

	g_attrib_unref(channel->attrib);

	/* Reset serial number after successful SCI delivery */
	if (!status)
		update_client_serial(channel);
}

static void update_cli_cfg(char *key, char *value, void *user_data)
{
	struct gatt_channel *channel = user_data;
	struct gatt_server *server = gatt_server_list;
	DBusMessage *msg;
	uint16_t handle = (uint16_t) strtol(key, NULL, 16);
	uint8_t *buf;
	int i, len;

	if (handle == svc_chg_handle + 1) {
		uint8_t tmp[7];

		channel->serial = (uint32_t) strtol(value, NULL, 16);

		/* Remote client is up-to-date */
		if (channel->serial >= serial_num)
			return;

		/* Create Service Change Indication, and send */
		tmp[0] = ATT_OP_HANDLE_IND;
		att_put_u16(svc_chg_handle, &tmp[1]);
		att_put_u16(svc_chg_handle + 2, &tmp[3]);
		att_put_u16(0xffff, &tmp[5]);

		i = g_attrib_send(channel->attrib, 0, ATT_OP_HANDLE_IND, tmp,
				sizeof(tmp), sci_return, channel, NULL);

		if (i)
			g_attrib_ref(channel->attrib);

		return;
	}

	len = strlen(value) / 2;
	buf = g_malloc0(len);

	if (!buf)
		return;

	for (i = 0; i < len; i++) {
		char tmp[3];

		tmp[0] = value[i * 2];
		tmp[1] = value[i * 2 + 1];
		tmp[2] = 0;
		buf[i] = (uint8_t) strtol(tmp, NULL, 16);
	}

	while (server && (server->base + server->count) <= handle)
		server = server->next;

	if (!server)
		goto failed;

	handle -= server->base;

	msg = dbus_message_new_method_call(server->name,
			server->path, GATT_SERVER_INTERFACE, "UpdateClientConfig");

	if (msg == NULL)
		goto failed;

	dbus_message_append_args(msg,
				DBUS_TYPE_UINT32, &channel->session,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &buf, len,
				DBUS_TYPE_INVALID);

	dbus_connection_send(connection, msg, NULL);
	dbus_message_unref(msg);

failed:
	g_free(buf);
}

void attrib_server_attach(struct _GAttrib *attrib, bdaddr_t *src,
						bdaddr_t *dst, guint mtu)
{
	static uint32_t session = 0;
	char filename[PATH_MAX + 1];
	struct gatt_channel *channel;
	uint16_t cid, cli_cfg = 0;
	GError *gerr = NULL;
	char addrstr[18];
	uint8_t tmp[7];
	char *serial_val, *cfg_val;
	void *adapter, *device;
	int ret;

	DBG("");

	adapter = manager_find_adapter(src);
	if (!adapter)
		return;

	ba2str(dst, addrstr);
	device = adapter_find_device(adapter, addrstr);
	if (!device)
		return;

	channel = g_malloc0(sizeof(struct gatt_channel) + mtu);

	if (mtu == ATT_DEFAULT_LE_MTU)
		channel->le = TRUE;
	else
		channel->le = FALSE;

	channel->mtu = mtu;
	channel->attrib = attrib;
	channel->src = *src;
	channel->dst = *dst;
	channel->device = device;

	/* Make session magic number change each time we reboot, in such a way
	 * as to be slightly unpredicatable.
	 */
	session += 0x11111111 + (uint32_t)channel;

	channel->session = session;


	channel->id = g_attrib_register(attrib, GATTRIB_ALL_REQS,
				channel_handler, channel, NULL);

	g_attrib_set_destroy_function(attrib, channel_destroy, channel);
	g_attrib_set_disconnect_server_function(attrib, channel_disconnect,
			channel);

	clients = g_slist_append(clients, channel);

	/* Determine if Service Change Indication needed */

	/* If not paired, don't indicate */
	if (read_link_key(src, dst, NULL, NULL) &&
			read_le_key(src, dst, NULL, NULL, NULL,
				NULL, NULL, 0, NULL, NULL, 0))
		return;

	make_cli_cfg_name(filename, channel);
	textfile_foreach(filename, update_cli_cfg, channel);
}

static void connect_event(GIOChannel *io, GError *err, void *user_data)
{
	GAttrib *attrib;
	uint16_t omtu;
	bdaddr_t src, dst;

	DBG(" %p == %p ?", io, le_io);
	if (err) {
		error("%s", err->message);
		return;
	}

	attrib = g_attrib_new(io);
	if (attrib) {
		if (bt_io_get(io, BT_IO_L2CAP, NULL,
					BT_IO_OPT_OMTU, &omtu,
					BT_IO_OPT_SOURCE_BDADDR, &src,
					BT_IO_OPT_DEST_BDADDR, &dst,
					BT_IO_OPT_INVALID))

			attrib_server_attach(attrib, &src, &dst, omtu);
	}
}

static void confirm_event(GIOChannel *io, void *user_data)
{
	GError *gerr = NULL;

	DBG(" %p == %p ?", io, le_io);
	if (bt_io_accept(io, connect_event, user_data, NULL, &gerr) == FALSE) {
		error("bt_io_accept: %s", gerr->message);
		g_error_free(gerr);
		g_io_channel_unref(io);
	}

	return;
}

static void attrib_notify_clients(struct attribute *attr)
{
	guint handle = attr->handle;
	GSList *l;

	for (l = clients; l; l = l->next) {
		struct gatt_channel *channel = l->data;

		/* Notification */
		if (g_slist_find_custom(channel->notify,
					GUINT_TO_POINTER(handle), handle_cmp)) {
			uint8_t pdu[ATT_MAX_MTU];
			uint16_t len;

			len = enc_notification(attr, pdu, channel->mtu);
			if (len == 0)
				continue;

			server_resp(channel->attrib, 0, pdu[0], pdu, len,
							NULL, NULL, NULL);
		}

		/* Indication */
		if (g_slist_find_custom(channel->indicate,
					GUINT_TO_POINTER(handle), handle_cmp)) {
			uint8_t pdu[ATT_MAX_MTU];
			uint16_t len;

			len = enc_indication(attr, pdu, channel->mtu);
			if (len == 0)
				return;

			server_resp(channel->attrib, 0, pdu[0], pdu, len,
							NULL, NULL, NULL);
		}
	}
}

static gboolean register_core_services(void)
{
	uint8_t atval[256];
	bt_uuid_t uuid;
	uint16_t appearance = 0x0000;

	/* GAP service: primary service definition */
	att_put_u16(GENERIC_ACCESS_PROFILE_ID, &atval[0]);
	attrib_db_add(0x0001, &prim_uuid, ATT_NONE, ATT_NOT_PERMITTED, atval, 2);

	/* GAP service: device name characteristic */
	name_handle = 0x0006;
	atval[0] = ATT_CHAR_PROPER_READ;
	att_put_u16(name_handle, &atval[1]);
	att_put_u16(GATT_CHARAC_DEVICE_NAME, &atval[3]);
	attrib_db_add(0x0004, &char_uuid, ATT_NONE, ATT_NOT_PERMITTED, atval, 5);

	/* GAP service: device name attribute */
	bt_uuid16_create(&uuid, GATT_CHARAC_DEVICE_NAME);
	attrib_db_add(name_handle, &uuid, ATT_NONE, ATT_NOT_PERMITTED,
								NULL, 0);

	/* GAP service: device appearance characteristic */
	appearance_handle = 0x0008;
	atval[0] = ATT_CHAR_PROPER_READ;
	att_put_u16(appearance_handle, &atval[1]);
	att_put_u16(GATT_CHARAC_APPEARANCE, &atval[3]);
	attrib_db_add(0x0007, &char_uuid, ATT_NONE, ATT_NOT_PERMITTED, atval, 5);

	/* GAP service: device appearance attribute */
	bt_uuid16_create(&uuid, GATT_CHARAC_APPEARANCE);
	att_put_u16(appearance, &atval[0]);
	attrib_db_add(appearance_handle, &uuid, ATT_NONE, ATT_NOT_PERMITTED,
								atval, 2);
	gap_sdp_handle = attrib_create_sdp(0x0001, "Generic Access Profile");
	if (gap_sdp_handle == 0) {
		error("Failed to register GAP service record");
		goto failed;
	}

	/* GATT service: primary service definition */
	att_put_u16(GENERIC_ATTRIB_PROFILE_ID, &atval[0]);
	attrib_db_add(0x0010, &prim_uuid, ATT_NONE, ATT_NOT_PERMITTED, atval, 2);

	/* GATT service: Service Changed Characteristic */
	svc_chg_handle = 0x0012;
	atval[0] = ATT_CHAR_PROPER_INDICATE;
	att_put_u16(svc_chg_handle, &atval[1]);
	att_put_u16(GATT_CHARAC_SERVICE_CHANGED, &atval[3]);
	attrib_db_add(0x0011, &char_uuid, ATT_NONE, ATT_NOT_PERMITTED, atval, 5);

	/* GATT service: Service Changed Attribute */
	bt_uuid16_create(&uuid, GATT_CHARAC_SERVICE_CHANGED);
	attrib_db_add(svc_chg_handle, &uuid, ATT_NOT_PERMITTED,
						ATT_NOT_PERMITTED, NULL, 0);

	/* GATT service: Service Changed Attribute Client Config Desc */
	attrib_db_add(svc_chg_handle + 1, &clicfg_uuid, ATT_NONE,
						ATT_NONE, NULL, 0);

	gatt_sdp_handle = attrib_create_sdp(0x0010,
						"Generic Attribute Profile");
	if (gatt_sdp_handle == 0) {
		error("Failed to register GATT service record");
		goto failed;
	}

	return TRUE;

failed:
	if (gap_sdp_handle)
		remove_record_from_server(gap_sdp_handle);

	return FALSE;
}

static uint32_t create_gatt_sdp(uuid_t *svc, uint16_t handle, uint16_t end,
							const char *name)

{
	sdp_record_t *record;
	uuid_t gap_uuid;

	record = server_record_new(svc, handle, end);
	if (record == NULL)
		return 0;

	if (name)
		sdp_set_info_attr(record, name, "BlueZ", NULL);

	sdp_uuid16_create(&gap_uuid, GENERIC_ACCESS_PROFILE_ID);
	if (sdp_uuid_cmp(svc, &gap_uuid) == 0) {
		sdp_set_url_attr(record, "http://www.bluez.org/",
				"http://www.bluez.org/",
				"http://www.bluez.org/");
	}

	if (add_record_to_server(BDADDR_ANY, record) < 0)
		sdp_record_free(record);
	else
		return record->handle;

	return 0;
}

static struct gatt_server *find_gatt_server(const char *path)
{
	struct gatt_server *server = gatt_server_last;

	while (server) {
		if (strcmp(server->path, path) == 0)
			return server;

		server = server->prev;
	}

	return NULL;
}

static char *create_gatt_name(const char *prefix, const char *base)
{
	char *result, *tmp;
	int prefix_len = strlen(prefix);

	result = g_malloc0(prefix_len + strlen(base) + 1);
	if (!result)
		return NULL;

	memcpy(result, prefix, prefix_len);

	while (*base == '/')
		base++;

	tmp = &result[prefix_len];
	while (*base) {
		if (*base == '/')
			*tmp++ = ':';
		else
			*tmp++ = *base;
		base++;
	}

	return result;
}

static void add_gatt_sdp(struct gatt_server *server, const char *uuid_str,
			uint16_t start, uint16_t end, const char *svc_name)
{
	struct gatt_sdp_handles *sdp_handle;
	uuid_t uuid;

	sdp_handle = g_malloc0(sizeof(*sdp_handle));
	if (!sdp_handle)
		return;

	/* Optimize UUID to smallest size, and un-normalize start/end values */
	bt_string2uuid(&uuid, uuid_str);
	sdp_uuid128_to_uuid(&uuid);
	start += server->base;
	end += server->base;
	sdp_handle->handle = create_gatt_sdp(&uuid, start, end, svc_name);

	if (!sdp_handle->handle) {
		free (sdp_handle);
		return;
	}

	/* Link into SDP handle list */
	sdp_handle->next = server->sdp;
	server->sdp = sdp_handle;
}

static void create_sdp_entry(char *key, char *value, void *user_data)
{
	struct gatt_server *server = user_data;
	int val_len = strlen(value);
	char *svc_name = NULL;
	uint16_t start, end;
	uuid_t uuid;
	int eir;

	if (val_len < 11)
		return;

	start = (uint16_t) strtol(value, NULL, 16);
	value += 5;
	end = (uint16_t) strtol(value, NULL, 16);
	value += 5;
	eir = (int) strtol(value, NULL, 16);

	if (val_len > 12)
		svc_name = value + 2;

	add_gatt_sdp(server, key, start, end, svc_name);

	/* TODO: Add BR EIR to lower level if required */
}

static void rebuild_sdp_list(struct gatt_server *server)
{
	char filename[PATH_MAX + 1];
	char *sdp_name = create_gatt_name(gatt_sdp_prefix, server->path);

	if (!sdp_name)
		return;

	create_name(filename, PATH_MAX, STORAGEDIR, "any", sdp_name);
	free(sdp_name);

	textfile_foreach(filename, create_sdp_entry, server);
}

static void add_gatt_adv(struct gatt_server *server, const char *uuid_str)
{
	struct gatt_adv_handles *adv_handle;

	adv_handle = g_malloc0(sizeof(*adv_handle));
	if (!adv_handle)
		return;

	/* Optimize UUID to smallest size */
	bt_string2uuid(&adv_handle->uuid, uuid_str);
	sdp_uuid128_to_uuid(&adv_handle->uuid);

	/* Link into adv handle list */
	adv_handle->next = server->adv;
	server->adv = adv_handle;

	/* TODO: Add LE Adv to lower level */
}

static void create_adv_entry(char *key, char *value, void *user_data)
{
	add_gatt_adv(user_data, key);
}

static void rebuild_adv_list(struct gatt_server *server)
{
	char filename[PATH_MAX + 1];
	char *sdp_name = create_gatt_name(gatt_adv_prefix, server->path);

	if (!sdp_name)
		return;

	create_name(filename, PATH_MAX, STORAGEDIR, "any", sdp_name);
	free(sdp_name);

	textfile_foreach(filename, create_adv_entry, server);
}

static void create_server_entry(char *key, char *value, void *user_data)
{
	struct gatt_server *new_server;
	char *name, *path;
	int nlen, plen;

	if (strcmp(key, serial_num_str) == 0) {
		serial_num = (uint32_t) strtol(value, NULL, 16);
		DBG(" Server version: %d", serial_num);
		return;
	}

	if (strlen(value) <= 8)
		return;

	path = key;
	plen = strlen(path);
	name = &value[8];

	for (nlen = 0; name[nlen]; nlen++) {
		if (name[nlen] == ' ') {
			name[nlen] = 0;
			break;
		}
	}

	new_server = g_malloc0(sizeof(struct gatt_server) + plen + nlen + 2);

	if (!new_server)
		return;

	new_server->count = (uint16_t) strtol(value, NULL, 16);
	new_server->carrier = (uint8_t) strtol(&value[5], NULL, 16);
	new_server->prev = gatt_server_last;
	new_server->path = &new_server->name[nlen + 1];
	memcpy(new_server->name, name, nlen);
	memcpy(new_server->path, path, plen);

	/* Append to end of existing server list */
	if (gatt_server_last) {
		gatt_server_last->next = new_server;
		new_server->base = gatt_server_last->base;
		new_server->base += gatt_server_last->count;
		gatt_server_last = new_server;
	} else {
		gatt_server_list = gatt_server_last = new_server;
		new_server->base = attrib_db_find_end();
	}

	DBG(" Entry %s %s -- attrs: 0x%4.4x-0x%4.4x",
			new_server->name, new_server->path, new_server->base,
			new_server->base + new_server->count - 1);

	rebuild_sdp_list(new_server);
	rebuild_adv_list(new_server);
}

static void rebuild_server_list(char *filename)
{
	/* Server file not gaurenteed to be stored in any particular order, so
	 * rebuild server list. No clients are currently connected, and will
	 * be given a ServiceChange Indication when they next connect.
	 */
	while (gatt_server_list) {
		void *tmp;

		while (gatt_server_list->sdp) {
			tmp = gatt_server_list->sdp;
			/* TODO: Remove BR EIR entry from lower level */
			remove_record_from_server(gatt_server_list->sdp->handle);
			gatt_server_list->sdp = gatt_server_list->sdp->next;
			free(tmp);
		}

		while (gatt_server_list->adv) {
			tmp = gatt_server_list->adv;
			/* TODO: Remove LE Adv from lower level */
			gatt_server_list->adv = gatt_server_list->adv->next;
			free(tmp);
		}

		tmp = gatt_server_list;
		gatt_server_list = gatt_server_list->next;
		free(tmp);
	}
	gatt_server_last = NULL;
	textfile_foreach(filename, create_server_entry, NULL);
}

static DBusMessage *register_server(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	char filename[PATH_MAX + 1];
	char vstr[32];
	const char *path, *owner, *car;
	uint16_t cnt;
	uint8_t carrier = CARRIER_NO_RESTRICTION;
	uint32_t available = 0x10000, serial_num = 0;
	char *str;

	DBG("");

	if (clients)
		return btd_error_not_ready(msg);

	if (gatt_server_last)
		available -= gatt_server_last->base + gatt_server_last->count;
	else
		available -= attrib_db_find_end();

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_STRING, &owner,
				DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_UINT16, &cnt,
				DBUS_TYPE_STRING, &car,
				DBUS_TYPE_INVALID) ||
				!cnt || !path || !path[0] ||
				!owner || !owner[0])
		return btd_error_invalid_args(msg);

	if (available < cnt)
		return btd_error_failed(msg, "Insufficient Space");

	if (strlen(owner) > (sizeof(vstr) - 6))
		return btd_error_failed(msg, "Owner too long");

	DBG(" Registering 0x%4.4x attrs on %s %s", cnt, owner, path);

	create_name(filename, PATH_MAX, STORAGEDIR, "any", "server");
	create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	str = textfile_get(filename, serial_num_str);
	if (str) {
		serial_num = (uint32_t) strtol(str, NULL, 16);
		g_free(str);
	}

	str = textfile_caseget(filename, path);
	if (str) {
		available = (uint32_t) strtol(str, NULL, 16);
		g_free(str);
		if (available != cnt)
			return btd_error_failed(msg, "Already Registered");
	} else {
		serial_num++;
		/* Put updated Serial Number */
		snprintf(vstr, sizeof(vstr), "%8.8X", serial_num);
		textfile_put(filename, serial_num_str, vstr);
	}

	if (car) {
		if (!strcmp(car, "LE"))
			carrier = CARRIER_LE_ONLY;
		else if (!strcmp(car, "BR"))
			carrier = CARRIER_BR_ONLY;
		else
			carrier = CARRIER_NO_RESTRICTION;
	}

	/* Register new Server, using fixed size str to prevent db reordering */
	snprintf(vstr, sizeof(vstr), "%4.4X %2.2X %s %99s",
						cnt, carrier, owner, " ");
	vstr[sizeof(vstr) - 1] = 0;
	textfile_put(filename, path, vstr);

	rebuild_server_list(filename);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *deregister_server(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	char filename[PATH_MAX + 1];
	char vstr[9];
	const char *path, *name;
	char *tmp_name;
	char *str;
	int ret;

	DBG("");

	if (clients)
		return btd_error_not_ready(msg);

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
						DBUS_TYPE_INVALID) == FALSE)
		return btd_error_invalid_args(msg);

	create_name(filename, PATH_MAX, STORAGEDIR, "any", "server");
	str = textfile_caseget(filename, path);
	if (!str)
		return btd_error_does_not_exist(msg);

	create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	DBG(" Deleting GATT server: %s %s", path, str);
	g_free(str);

	str = textfile_get(filename, serial_num_str);
	if (str) {
		serial_num = (uint32_t) strtol(str, NULL, 16);
		g_free(str);
	}

	serial_num++;
	/* Put updated Serial Number */
	snprintf(vstr, sizeof(vstr), "%8.8X", serial_num);
	textfile_put(filename, serial_num_str, vstr);

	textfile_casedel(filename, path);

	rebuild_server_list(filename);

	/* Delete SDP and ADV records associated with this server */
	tmp_name = create_gatt_name(gatt_sdp_prefix, path);
	if (!tmp_name)
		goto done;

	create_name(filename, PATH_MAX, STORAGEDIR, "any", tmp_name);
	delete_file(filename);
	free(tmp_name);

	tmp_name = create_gatt_name(gatt_adv_prefix, path);
	if (!tmp_name)
		goto done;

	create_name(filename, PATH_MAX, STORAGEDIR, "any", tmp_name);
	delete_file(filename);
	free(tmp_name);

done:
	return dbus_message_new_method_return(msg);
}

static DBusMessage *add_primary_sdp(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct gatt_server *server;
	char filename[PATH_MAX + 1];
	char *vstr;
	const char *path;
	const char *svc_name;
	const char *uuid_str;
	char *str;
	char *sdp_name;
	uuid_t uuid;
	uint16_t start, end, server_count;
	dbus_bool_t eir;
	int ret;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
					DBUS_TYPE_STRING, &svc_name,
					DBUS_TYPE_STRING, &uuid_str,
					DBUS_TYPE_UINT16, &start,
					DBUS_TYPE_UINT16, &end,
					DBUS_TYPE_BOOLEAN, &eir,
					DBUS_TYPE_INVALID))
		return btd_error_invalid_args(msg);

	create_name(filename, PATH_MAX, STORAGEDIR, "any", "server");

	/* Don't allow SDP registration before server is registered */
	str = textfile_get(filename, path);
	if (!str)
		return btd_error_does_not_exist(msg);

	server_count = (uint16_t) strtol(str, NULL, 16);
	free(str);

	/* Range check start and end attributes */
	if (end >= server_count || start > end)
		return btd_error_invalid_args(msg);

	sdp_name = create_gatt_name(gatt_sdp_prefix, path);

	if (!sdp_name)
		return btd_error_failed(msg, "Insufficient Space");

	create_name(filename, PATH_MAX, STORAGEDIR, "any", sdp_name);
	create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	free(sdp_name);

	if (svc_name && strlen(svc_name)) {
		vstr = g_malloc0(13 + strlen(svc_name));
		if (vstr)
			snprintf(vstr, sizeof(vstr), "%4.4X %4.4X %c %s",
					start, end, eir ? '1' : '0', svc_name);
	} else {
		svc_name = NULL;
		vstr = g_malloc0(12);
		if (vstr)
			snprintf(vstr, sizeof(vstr), "%4.4X %4.4X %c",
					start, end, eir ? '1' : '0');
	}

	if (!vstr)
		return btd_error_failed(msg, "Insufficient Space");

	/* Store SDP request */
	textfile_put(filename, uuid_str, vstr);
	free(vstr);

	/* Only register SDP record now if server currently loaded */
	server = find_gatt_server(path);
	if (server)
		add_gatt_sdp(server, uuid_str, start, end, svc_name);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *add_primary_adv(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct gatt_server *server;
	char filename[PATH_MAX + 1];
	const char *path;
	const char *uuid_str;
	char *str;
	char *adv_name;
	uuid_t uuid;
	struct gatt_adv_handles *adv_handle;
	int ret;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
					DBUS_TYPE_STRING, &uuid_str,
					DBUS_TYPE_INVALID) == FALSE)
		return btd_error_invalid_args(msg);

	create_name(filename, PATH_MAX, STORAGEDIR, "any", "server");

	str = textfile_get(filename, path);
	if (!str)
		return btd_error_does_not_exist(msg);

	adv_name = create_gatt_name(gatt_adv_prefix, path);

	if (!adv_name)
		return btd_error_failed(msg, "Insufficient Space");

	create_name(filename, PATH_MAX, STORAGEDIR, "any", adv_name);
	create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	/* Store Adv request */
	textfile_put(filename, uuid_str, "T");

	/* Only register SDP record now if server currently loaded */
	server = find_gatt_server(path);
	if (server)
		add_gatt_adv(server, uuid_str);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *server_notify(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct gatt_channel *channel;
	struct gatt_server *server;
	uint8_t pdu[ATT_DEFAULT_LE_MTU];
	GSList *l;
	uint32_t session;
	const char *path;
	uint16_t handle;
	const uint8_t *payload;
	dbus_int32_t len;
	int ret;

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_UINT32, &session,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &payload, &len,
				DBUS_TYPE_INVALID) == FALSE)
		return btd_error_invalid_args(msg);

	for (l = clients; l; l = l->next) {
		channel = l->data;
		if (channel->session == session)
			break;
	}

	if (!l)
		return btd_error_not_connected(msg);

	server = find_gatt_server(path);
	if (!server)
		return btd_error_invalid_args(msg);

	handle += server->base;

	ret = enc_notify(handle, payload, len, pdu, sizeof(pdu));

	g_attrib_send(channel->attrib, 0, ATT_OP_HANDLE_NOTIFY, pdu,
			(guint16) ret, NULL, NULL, NULL);

	return dbus_message_new_method_return(msg);
}

static void ind_return(guint8 status, const guint8 *pdu, guint16 len,
								gpointer data)
{
	struct gatt_channel *channel = NULL;
	DBusMessage *reply;
	GSList *l;

	for (l = clients; l; l = l->next) {
		if (l->data == data) {
			channel = data;
			break;
		}
	}

	if (!channel)
		return;

	g_attrib_unref(channel->attrib);

	if (!channel->ind_msg)
		return;

	if (status)
		reply = btd_error_failed(channel->ind_msg,
						map_att_error(status));
	else
		reply = dbus_message_new_method_return(channel->ind_msg);

	g_dbus_send_message(connection, reply);
	dbus_message_unref(channel->ind_msg);
	channel->ind_msg = NULL;
}

static DBusMessage *server_indicate(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct gatt_channel *channel = NULL;
	struct gatt_server *server;
	uint8_t pdu[ATT_DEFAULT_LE_MTU];
	GSList *l;
	uint32_t session;
	const char *path;
	uint16_t handle;
	const uint8_t *payload;
	dbus_int32_t len;
	int ret;

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_UINT32, &session,
				DBUS_TYPE_UINT16, &handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &payload, &len,
				DBUS_TYPE_INVALID) == FALSE)
		return btd_error_invalid_args(msg);

	for (l = clients; l; l = l->next) {
		struct gatt_channel *chan = l->data;
		if (chan->session == session) {
			channel = chan;
			break;
		}
	}

	if (!channel)
		return btd_error_not_connected(msg);

	if (channel->ind_msg)
		return btd_error_busy(msg);

	server = find_gatt_server(path);
	if (!server)
		return btd_error_invalid_args(msg);

	handle += server->base;

	ret = enc_indicate(handle, payload, len, pdu, sizeof(pdu));

	ret = g_attrib_send(channel->attrib, 0, ATT_OP_HANDLE_IND, pdu,
			(guint16) ret, ind_return, channel, NULL);

	if (!ret)
		return btd_error_failed(msg, "Insufficient Resources");

	g_attrib_ref(channel->attrib);
	channel->ind_msg = dbus_message_ref(msg);

	return NULL;
}

static DBusMessage *get_reg_servers(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter array_iter;

	if (!dbus_message_has_signature(msg, DBUS_TYPE_INVALID_AS_STRING))
		return btd_error_invalid_args(msg);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
				DBUS_TYPE_OBJECT_PATH_AS_STRING, &array_iter);
	if (gatt_server_list) {
		struct gatt_server *l;

		for (l = gatt_server_list; l; l = l->next) {

		dbus_message_iter_append_basic(&array_iter,
				DBUS_TYPE_OBJECT_PATH, &l->path);
		}
	}
	dbus_message_iter_close_container(&iter, &array_iter);
	return reply;
}

static DBusMessage *get_server_prop(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	const char *path;
	const char *prop;
	int ret;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &path,
					DBUS_TYPE_STRING, &prop,
					DBUS_TYPE_INVALID) == FALSE)
		return btd_error_invalid_args(msg);

	return dbus_message_new_method_return(msg);
}

static GDBusMethodTable gatt_server_methods[] = {
	{ "RegisterServer",      "soqs",   "",      register_server,   0, 0 },
	{ "AddPrimarySdp",       "ossqqb", "",      add_primary_sdp,   0, 0 },
	{ "AddPrimaryAdvertise", "os",     "",      add_primary_adv,   0, 0 },
	{ "DeregisterServer",    "o",      "",      deregister_server, 0, 0 },
	{ "Notify",              "ouqay",  "",      server_notify,     0, 0 },
	{ "Indicate",            "ouqay",  "",      server_indicate,   
						G_DBUS_METHOD_FLAG_ASYNC, 0 },
	{ "GetRegisteredServers", "",      "a{o}",  get_reg_servers,   0, 0 },
	{ "GetProperty",         "os",     "v",     get_server_prop,   0, 0 },
	{ NULL, NULL, NULL, NULL, 0, 0 }
};

static GDBusSignalTable gatt_server_signals[] = {
	{ "PropertyChanged", "sv", 0 },
	{ NULL, NULL, 0 }
};

int attrib_server_init(void)
{
	GError *gerr = NULL;

	/* BR/EDR socket */
	l2cap_io = bt_io_listen(BT_IO_L2CAP, NULL, confirm_event,
					NULL, NULL, &gerr,
					BT_IO_OPT_SOURCE_BDADDR, BDADDR_ANY,
					BT_IO_OPT_PSM, ATT_PSM,
					BT_IO_OPT_SEC_LEVEL, BT_IO_SEC_LOW,
					BT_IO_OPT_INVALID);
	if (l2cap_io == NULL) {
		error("%s", gerr->message);
		g_error_free(gerr);
		return -1;
	}

	if (!register_core_services())
		goto failed;

	if (!main_opts.le)
		goto no_le;

	/* LE socket */
	le_io = bt_io_listen(BT_IO_L2CAP, NULL, confirm_event,
					&le_io, NULL, &gerr,
					BT_IO_OPT_SOURCE_BDADDR, BDADDR_ANY,
					BT_IO_OPT_CID, ATT_CID,
					BT_IO_OPT_SEC_LEVEL, BT_IO_SEC_LOW,
					BT_IO_OPT_INVALID);
	if (le_io == NULL) {
		error("%s", gerr->message);
		g_error_free(gerr);
		/* Doesn't have LE support, continue */
	}

no_le:
	connection = get_dbus_connection();

	if (g_dbus_register_interface(connection,
				btd_adapter_any_request_path(),
				GATT_SERVER_INTERFACE,
				gatt_server_methods, gatt_server_signals,
				NULL, NULL, NULL))
		return 0;

failed:
	g_io_channel_unref(l2cap_io);
	l2cap_io = NULL;

	if (le_io) {
		g_io_channel_unref(le_io);
		le_io = NULL;
	}

	return -1;
}

int attrib_server_reg_adapter(void *adapter)
{
	DBG(" %s on %s", GATT_SERVER_INTERFACE, adapter_get_path(adapter));

	connection = get_dbus_connection();
	if (g_dbus_register_interface(connection,
				adapter_get_path(adapter),
				GATT_SERVER_INTERFACE,
				gatt_server_methods, gatt_server_signals,
				NULL, NULL, NULL))
		return 0;

	return -1;
}

void attrib_server_unreg_adapter(void *adapter)
{
	DBG(" %s from %s", GATT_SERVER_INTERFACE, adapter_get_path(adapter));

	g_dbus_unregister_interface(connection, adapter_get_path(adapter),
							GATT_SERVER_INTERFACE);
}

void attrib_server_dbus_enable(void)
{
	char filename[PATH_MAX + 1];

	DBG(" Base: 0x%04x", attrib_db_find_end());

	create_name(filename, PATH_MAX, STORAGEDIR, "any", "server");
	rebuild_server_list(filename);
}

void attrib_server_exit(void)
{
	GSList *l;

	g_slist_foreach(database, (GFunc) g_free, NULL);
	g_slist_free(database);

	if (l2cap_io) {
		g_io_channel_unref(l2cap_io);
		g_io_channel_shutdown(l2cap_io, FALSE, NULL);
	}

	if (le_io) {
		g_io_channel_unref(le_io);
		g_io_channel_shutdown(le_io, FALSE, NULL);
	}

	for (l = clients; l; l = l->next) {
		struct gatt_channel *channel = l->data;

		g_slist_free(channel->notify);
		g_slist_free(channel->indicate);

		g_attrib_unref(channel->attrib);
		g_free(channel);
	}

	g_slist_free(clients);

	if (gatt_sdp_handle)
		remove_record_from_server(gatt_sdp_handle);

	if (gap_sdp_handle)
		remove_record_from_server(gap_sdp_handle);
}

uint32_t attrib_create_sdp(uint16_t handle, const char *name)
{
	struct attribute *a;
	uint16_t end;
	uuid_t svc;

	a = find_primary_range(handle, &end);

	if (a == NULL)
		return 0;

	if (a->len == 2)
		sdp_uuid16_create(&svc, att_get_u16(a->data));
	else if (a->len == 16)
		sdp_uuid128_create(&svc, a->data);
	else
		return 0;

	return create_gatt_sdp(&svc, handle, end, name);
}

void attrib_free_sdp(uint32_t sdp_handle)
{
	remove_record_from_server(sdp_handle);
}

uint16_t attrib_db_find_end(void)
{
	uint16_t handle;
	GSList *l;

	for (l = database, handle = 1; l; l = l->next) {
		struct attribute *a = l->data;

		if (a->handle == 0xffff)
			return 0xffff;

		handle = a->handle + 1;
	}

	return handle;
}

uint16_t attrib_db_find_avail(uint16_t nitems)
{
	uint16_t handle;
	GSList *l;

	g_assert(nitems > 0);

	for (l = database, handle = 0; l; l = l->next) {
		struct attribute *a = l->data;

		if (handle && (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
				bt_uuid_cmp(&a->uuid, &snd_uuid) == 0) &&
				a->handle - handle >= nitems)
			/* Note: the range above excludes the current handle */
			return handle;

		if (a->handle == 0xffff)
			return 0;

		handle = a->handle + 1;
	}

	if (0xffff - handle + 1 >= nitems)
		return handle;

	return 0;
}

struct attribute *attrib_db_add(uint16_t handle, bt_uuid_t *uuid, int read_reqs,
				int write_reqs, const uint8_t *value, int len)
{
	struct attribute *a;
	guint h = handle;

	DBG("handle=0x%04x", handle);

	if (g_slist_find_custom(database, GUINT_TO_POINTER(h), handle_cmp))
		return NULL;

	a = g_malloc0(sizeof(struct attribute) + len);
	a->handle = handle;
	memcpy(&a->uuid, uuid, sizeof(bt_uuid_t));
	a->read_reqs = read_reqs;
	a->write_reqs = write_reqs;
	a->len = len;
	memcpy(a->data, value, len);

	database = g_slist_insert_sorted(database, a, attribute_cmp);

	return a;
}

int attrib_db_update(uint16_t handle, bt_uuid_t *uuid, const uint8_t *value,
					int len, struct attribute **attr)
{
	struct attribute *a;
	GSList *l;
	guint h = handle;

	DBG("handle=0x%04x", handle);

	l = g_slist_find_custom(database, GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return -ENOENT;

	a = g_try_realloc(l->data, sizeof(struct attribute) + len);
	if (a == NULL)
		return -ENOMEM;

	l->data = a;
	if (uuid != NULL)
		memcpy(&a->uuid, uuid, sizeof(bt_uuid_t));
	a->len = len;
	memcpy(a->data, value, len);

	attrib_notify_clients(a);

	if (attr)
		*attr = a;

	return 0;
}

int attrib_db_del(uint16_t handle)
{
	struct attribute *a;
	GSList *l;
	guint h = handle;

	DBG("handle=0x%04x", handle);

	l = g_slist_find_custom(database, GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return -ENOENT;

	a = l->data;
	database = g_slist_remove(database, a);
	g_free(a);

	return 0;
}

int attrib_gap_set(uint16_t uuid, const uint8_t *value, int len)
{
	uint16_t handle;

	/* FIXME: Missing Privacy and Reconnection Address */

	switch (uuid) {
	case GATT_CHARAC_DEVICE_NAME:
		handle = name_handle;
		break;
	case GATT_CHARAC_APPEARANCE:
		handle = appearance_handle;
		break;
	default:
		return -ENOSYS;
	}

	return attrib_db_update(handle, NULL, value, len, NULL);
}
