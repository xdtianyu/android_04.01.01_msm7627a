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

#include <stdint.h>
#include <string.h>
#include <glib.h>

#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>

#include "log.h"
#include "att.h"
#include "btio.h"
#include "gattrib.h"

struct _GAttrib {
	GIOChannel *io;
	gint refs;
	uint8_t *buf;
	int buflen;
	guint read_watch;
	guint write_watch;
	guint timeout_watch;
	GQueue *queue;
	GSList *events;
	guint next_cmd_id;
	guint next_evt_id;
	GDestroyNotify destroy;
	GAttribDisconnectFunc disconnect;
	GAttribDisconnectFunc disconnect_server;
	gpointer destroy_user_data;
	gpointer disc_user_data;
	gpointer disc_server_data;
};

struct command {
	guint id;
	guint8 opcode;
	guint8 *pdu;
	guint16 len;
	guint8 expected;
	gboolean sent;
	GAttribResultFunc func;
	gpointer user_data;
	GDestroyNotify notify;
};

struct event {
	guint id;
	guint8 expected;
	GAttribNotifyFunc func;
	gpointer user_data;
	GDestroyNotify notify;
};

static guint8 opcode2expected(guint8 opcode)
{
	switch (opcode) {
	case ATT_OP_MTU_REQ:
		return ATT_OP_MTU_RESP;

	case ATT_OP_FIND_INFO_REQ:
		return ATT_OP_FIND_INFO_RESP;

	case ATT_OP_FIND_BY_TYPE_REQ:
		return ATT_OP_FIND_BY_TYPE_RESP;

	case ATT_OP_READ_BY_TYPE_REQ:
		return ATT_OP_READ_BY_TYPE_RESP;

	case ATT_OP_READ_REQ:
		return ATT_OP_READ_RESP;

	case ATT_OP_READ_BLOB_REQ:
		return ATT_OP_READ_BLOB_RESP;

	case ATT_OP_READ_MULTI_REQ:
		return ATT_OP_READ_MULTI_RESP;

	case ATT_OP_READ_BY_GROUP_REQ:
		return ATT_OP_READ_BY_GROUP_RESP;

	case ATT_OP_WRITE_REQ:
		return ATT_OP_WRITE_RESP;

	case ATT_OP_PREP_WRITE_REQ:
		return ATT_OP_PREP_WRITE_RESP;

	case ATT_OP_EXEC_WRITE_REQ:
		return ATT_OP_EXEC_WRITE_RESP;

	case ATT_OP_HANDLE_IND:
		return ATT_OP_HANDLE_CNF;
	}

	return 0;
}

static gboolean is_response(guint8 opcode)
{
	switch (opcode) {
	case ATT_OP_ERROR:
	case ATT_OP_MTU_RESP:
	case ATT_OP_FIND_INFO_RESP:
	case ATT_OP_FIND_BY_TYPE_RESP:
	case ATT_OP_READ_BY_TYPE_RESP:
	case ATT_OP_READ_RESP:
	case ATT_OP_READ_BLOB_RESP:
	case ATT_OP_READ_MULTI_RESP:
	case ATT_OP_READ_BY_GROUP_RESP:
	case ATT_OP_WRITE_RESP:
	case ATT_OP_PREP_WRITE_RESP:
	case ATT_OP_EXEC_WRITE_RESP:
	case ATT_OP_HANDLE_CNF:
		return TRUE;
	}

	return FALSE;
}

GAttrib *g_attrib_ref(GAttrib *attrib)
{
	DBG(" attrib: %p %d", attrib, attrib ? attrib->refs+1 : 0);

	if (!attrib)
		return NULL;

	g_atomic_int_inc(&attrib->refs);

	return attrib;
}

static void command_destroy(struct command *cmd)
{
	if (cmd->notify)
		cmd->notify(cmd->user_data);

	g_free(cmd->pdu);
	g_free(cmd);
}

static void event_destroy(struct event *evt)
{
	if (evt->notify)
		evt->notify(evt->user_data);

	g_free(evt);
}

static void attrib_destroy(GAttrib *attrib)
{
	GSList *l;
	struct command *c;

	DBG(" attrib: %p", attrib);

	while ((c = g_queue_pop_head(attrib->queue)))
		command_destroy(c);

	g_queue_free(attrib->queue);
	attrib->queue = NULL;

	for (l = attrib->events; l; l = l->next)
		event_destroy(l->data);

	g_slist_free(attrib->events);
	attrib->events = NULL;

	if (attrib->timeout_watch > 0)
		g_source_remove(attrib->timeout_watch);

	if (attrib->write_watch > 0)
		g_source_remove(attrib->write_watch);

	if (attrib->read_watch > 0) {
		g_source_remove(attrib->read_watch);
		g_io_channel_unref(attrib->io);
	}

	g_free(attrib->buf);

	if (attrib->destroy)
		attrib->destroy(attrib->destroy_user_data);

	g_free(attrib);
}

void g_attrib_unref(GAttrib *attrib)
{
	DBG(" attrib: %p %d", attrib, attrib ? attrib->refs-1 : 0);

	if (!attrib)
		return;

	if (g_atomic_int_dec_and_test(&attrib->refs) == FALSE)
		return;

	attrib_destroy(attrib);
}

GIOChannel *g_attrib_get_channel(GAttrib *attrib)
{
	if (!attrib)
		return NULL;

	return attrib->io;
}

gboolean g_attrib_set_disconnect_server_function(GAttrib *attrib,
		GAttribDisconnectFunc disconnect, gpointer user_data)
{
	if (attrib == NULL)
		return FALSE;

	DBG("");

	attrib->disconnect_server = disconnect;
	attrib->disc_server_data = user_data;

	return TRUE;
}

gboolean g_attrib_set_disconnect_function(GAttrib *attrib,
		GAttribDisconnectFunc disconnect, gpointer user_data)
{
	if (attrib == NULL)
		return FALSE;

	attrib->disconnect = disconnect;
	attrib->disc_user_data = user_data;

	return TRUE;
}

gboolean g_attrib_set_destroy_function(GAttrib *attrib,
		GDestroyNotify destroy, gpointer user_data)
{
	if (attrib == NULL)
		return FALSE;

	attrib->destroy = destroy;
	attrib->destroy_user_data = user_data;

	return TRUE;
}

static gboolean disconnect_timeout(gpointer data)
{
	struct _GAttrib *attrib = data;

	DBG(" attrib: %p", attrib);

	attrib_destroy(attrib);

	return FALSE;
}

static gboolean can_write_data(GIOChannel *io, GIOCondition cond,
								gpointer data)
{
	struct _GAttrib *attrib = data;
	struct command *cmd;
	GError *gerr = NULL;
	gsize len;
	GIOStatus iostat;
	gboolean ret = FALSE;

	DBG(" attrib: %p", attrib);

	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		if (attrib->disconnect_server)
			attrib->disconnect_server(attrib->disc_server_data);

		if (attrib->disconnect)
			attrib->disconnect(attrib->disc_user_data);

		return FALSE;
	}

	cmd = g_queue_peek_head(attrib->queue);
	if (cmd == NULL)
		return FALSE;

	iostat = g_io_channel_write_chars(io, (gchar *) cmd->pdu, cmd->len,
								&len, &gerr);

	if (iostat != G_IO_STATUS_NORMAL) {
		ret = FALSE;
		goto done;
	}

	if (cmd->expected == 0) {
		g_queue_pop_head(attrib->queue);
		command_destroy(cmd);
		ret = TRUE;
		goto done;
	}

	cmd->sent = TRUE;

	if (attrib->timeout_watch == 0)
		attrib->timeout_watch = g_timeout_add_seconds(GATT_TIMEOUT,
						disconnect_timeout, attrib);

done:
	g_attrib_unref(attrib);
	return ret;
}

static void destroy_sender(gpointer data)
{
	struct _GAttrib *attrib = data;

	attrib->write_watch = 0;
}

static void wake_up_sender(struct _GAttrib *attrib)
{
	if (attrib->write_watch == 0)
		attrib->write_watch = g_io_add_watch_full(attrib->io,
			G_PRIORITY_DEFAULT, G_IO_OUT, can_write_data,
			attrib, destroy_sender);
}

static void transport_error(struct _GAttrib *attrib, uint8_t *buf, gsize len)
{
	uint8_t err[5];
	uint16_t handle;

	switch(buf[0]) {
	case ATT_OP_FIND_INFO_REQ:
	case ATT_OP_FIND_BY_TYPE_REQ:
	case ATT_OP_READ_BY_TYPE_REQ:
	case ATT_OP_READ_REQ:
	case ATT_OP_READ_BLOB_REQ:
	case ATT_OP_READ_MULTI_REQ:
	case ATT_OP_READ_BY_GROUP_REQ:
	case ATT_OP_WRITE_REQ:
	case ATT_OP_PREP_WRITE_REQ:
	case ATT_OP_HANDLE_IND:
		handle = att_get_u16(&buf[1]);
		break;
	default:
		handle = 0;
		break;
	}

	enc_error_resp(buf[0], handle, ATT_ECODE_INVALID_TRANSPORT,
							err, sizeof(err));
	g_attrib_send(attrib, 0, err[0], err, sizeof(err), NULL, NULL, NULL);
}

static gboolean received_data(GIOChannel *io, GIOCondition cond, gpointer data)
{
	struct _GAttrib *attrib = data;
	struct command *cmd = NULL;
	GSList *l;
	uint8_t buf[512], status;
	gsize len;
	GIOStatus iostat;
	gboolean qempty, delivered = FALSE;

	DBG(" io: %p, cond: %d, data: %p", io, cond, data);

	if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		attrib->read_watch = 0;
		if (attrib->disconnect_server)
			attrib->disconnect_server(attrib->disc_server_data);

		if (attrib->disconnect)
			attrib->disconnect(attrib->disc_user_data);

		return FALSE;
	}

	memset(buf, 0, sizeof(buf));

	iostat = g_io_channel_read_chars(io, (gchar *) buf, sizeof(buf),
								&len, NULL);

	if (attrib->timeout_watch > 0 && is_response(buf[0]) == TRUE) {
		g_source_remove(attrib->timeout_watch);
		attrib->timeout_watch = 0;
	}

	if (iostat != G_IO_STATUS_NORMAL) {
		status = ATT_ECODE_IO;
		goto done;
	}

	for (l = attrib->events; l; l = l->next) {
		struct event *evt = l->data;

		if (evt->expected == buf[0] ||
			evt->expected == GATTRIB_ALL_EVENTS ||
			(evt->expected == GATTRIB_ALL_REQS && !(buf[0] & 1))) {
			delivered = TRUE;
			evt->func(buf, len, evt->user_data);
		}
	}

	if (is_response(buf[0]) == FALSE) {
		/* If response expected and no delivery, return error */
		if (!delivered && opcode2expected(buf[0]))
			transport_error(attrib, buf, len);

		return TRUE;
	}

	/* Auto-elevate security if remote device complains */
	if (buf[0] == ATT_OP_ERROR && (buf[4] == ATT_ECODE_INSUFF_ENC ||
					buf[4] == ATT_ECODE_AUTHENTICATION)) {
		BtIOSecLevel sec_level = BT_IO_SEC_LOW;

		bt_io_get(io, BT_IO_L2CAP, NULL,
			BT_IO_OPT_SEC_LEVEL, &sec_level,
			BT_IO_OPT_INVALID);

		/* If already at high, give up and process as normal */
		if (sec_level == BT_IO_SEC_HIGH)
			goto process_response;
		else if (sec_level < BT_IO_SEC_MEDIUM)
			sec_level = BT_IO_SEC_MEDIUM;
		else
			sec_level = BT_IO_SEC_HIGH;

		if (bt_io_set(io, BT_IO_L2CAP, NULL,
				BT_IO_OPT_SEC_LEVEL, sec_level,
				BT_IO_OPT_INVALID)) {
			goto done;
		}
	}

process_response:
	cmd = g_queue_pop_head(attrib->queue);
	if (cmd == NULL) {
		/* Keep the watch if we have events to report */
		return attrib->events != NULL;
	}

	if (buf[0] == ATT_OP_ERROR) {
		status = buf[4];
		goto done;
	}

	if (cmd->expected != buf[0]) {
		status = ATT_ECODE_IO;
		goto done;
	}

	status = 0;

done:
	qempty = attrib->queue == NULL || g_queue_is_empty(attrib->queue);

	if (cmd) {
		if (cmd->func)
			cmd->func(status, buf, len, cmd->user_data);

		command_destroy(cmd);
	}

	if (!qempty)
		wake_up_sender(attrib);

	return TRUE;
}

GAttrib *g_attrib_new(GIOChannel *io)
{
	struct _GAttrib *attrib;
	uint16_t omtu, cid;

	g_io_channel_set_encoding(io, NULL, NULL);
	g_io_channel_set_buffered(io, FALSE);

	attrib = g_try_new0(struct _GAttrib, 1);
	if (attrib == NULL)
		return NULL;

	attrib->io = g_io_channel_ref(io);
	attrib->queue = g_queue_new();

	attrib->read_watch = g_io_add_watch(attrib->io,
			G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			received_data, attrib);

	if (bt_io_get(attrib->io, BT_IO_L2CAP, NULL,
			BT_IO_OPT_OMTU, &omtu,
			BT_IO_OPT_CID, &cid,
			BT_IO_OPT_INVALID)) {
		if (omtu == 0 || omtu > ATT_MAX_MTU)
			omtu = ATT_MAX_MTU;
	} else
		omtu = ATT_DEFAULT_LE_MTU;

	DBG(" cid %d mtu %d", cid, omtu);

	if (cid == ATT_CID)
		omtu = ATT_DEFAULT_LE_MTU;

	attrib->buf = g_malloc0(omtu);
	attrib->buflen = omtu;

	return g_attrib_ref(attrib);
}

guint g_attrib_send(GAttrib *attrib, guint id, guint8 opcode,
			const guint8 *pdu, guint16 len, GAttribResultFunc func,
			gpointer user_data, GDestroyNotify notify)
{
	struct command *c;

	if(!attrib || !pdu)
		return 0;

	c = g_try_new0(struct command, 1);
	if (c == NULL)
		return 0;

	c->opcode = opcode;
	c->expected = opcode2expected(opcode);
	c->pdu = g_malloc(len);
	memcpy(c->pdu, pdu, len);
	c->len = len;
	c->func = func;
	c->user_data = user_data;
	c->notify = notify;

	if (id) {
		c->id = id;
		g_queue_push_head(attrib->queue, c);
	} else {
		c->id = ++attrib->next_cmd_id;
		g_queue_push_tail(attrib->queue, c);
	}

	g_attrib_ref(attrib);

	if (g_queue_get_length(attrib->queue) == 1)
		wake_up_sender(attrib);

	return c->id;
}

static gint command_cmp_by_id(gconstpointer a, gconstpointer b)
{
	const struct command *cmd = a;
	guint id = GPOINTER_TO_UINT(b);

	return cmd->id - id;
}

gboolean g_attrib_cancel(GAttrib *attrib, guint id)
{
	GList *l;
	struct command *cmd;

	if (attrib == NULL || attrib->queue == NULL)
		return FALSE;

	l = g_queue_find_custom(attrib->queue, GUINT_TO_POINTER(id),
							command_cmp_by_id);
	if (l == NULL)
		return FALSE;

	cmd = l->data;

	if (cmd == g_queue_peek_head(attrib->queue) && cmd->sent)
		cmd->func = NULL;
	else {
		g_queue_remove(attrib->queue, cmd);
		command_destroy(cmd);
	}

	return TRUE;
}

gboolean g_attrib_cancel_all(GAttrib *attrib)
{
	struct command *c, *head = NULL;
	gboolean first = TRUE;

	if (attrib == NULL || attrib->queue == NULL)
		return FALSE;

	while ((c = g_queue_pop_head(attrib->queue))) {
		if (first && c->sent) {
			/* If the command was sent ignore its callback ... */
			c->func = NULL;
			head = c;
			continue;
		}

		first = FALSE;
		command_destroy(c);
	}

	if (head) {
		/* ... and put it back in the queue */
		g_queue_push_head(attrib->queue, head);
	}

	return TRUE;
}

gboolean g_attrib_set_debug(GAttrib *attrib,
		GAttribDebugFunc func, gpointer user_data)
{
	return TRUE;
}

uint8_t *g_attrib_get_buffer(GAttrib *attrib, int *len)
{
	if (len == NULL)
		return NULL;

	*len = attrib->buflen;

	return attrib->buf;
}

gboolean g_attrib_set_mtu(GAttrib *attrib, int mtu)
{
	if (mtu < ATT_DEFAULT_LE_MTU)
		mtu = ATT_DEFAULT_LE_MTU;

	if (mtu > ATT_MAX_MTU)
		mtu = ATT_MAX_MTU;

	if (!bt_io_set(attrib->io, BT_IO_L2CAP, NULL,
			BT_IO_OPT_OMTU, mtu,
			BT_IO_OPT_INVALID))
		return FALSE;

	attrib->buf = g_realloc(attrib->buf, mtu);

	attrib->buflen = mtu;

	return TRUE;
}

guint g_attrib_register(GAttrib *attrib, guint8 opcode,
				GAttribNotifyFunc func, gpointer user_data,
				GDestroyNotify notify)
{
	struct event *event;

	event = g_try_new0(struct event, 1);
	if (event == NULL)
		return 0;

	event->expected = opcode;
	event->func = func;
	event->user_data = user_data;
	event->notify = notify;
	event->id = ++attrib->next_evt_id;

	attrib->events = g_slist_append(attrib->events, event);

	DBG(" attrib %p events %p event: %p - opcode %d - func %p id %d",
		attrib, attrib->events, event, opcode, func, event->id);

	return event->id;
}

static gint event_cmp_by_id(gconstpointer a, gconstpointer b)
{
	const struct event *evt = a;
	guint id = GPOINTER_TO_UINT(b);

	return evt->id - id;
}

gboolean g_attrib_is_encrypted(GAttrib *attrib)
{
	return g_attrib_sec_level(attrib) > BT_IO_SEC_LOW;
}

BtIOSecLevel g_attrib_sec_level(GAttrib *attrib)
{
	BtIOSecLevel sec_level;

	if (!bt_io_get(attrib->io, BT_IO_L2CAP, NULL,
			BT_IO_OPT_SEC_LEVEL, &sec_level,
			BT_IO_OPT_INVALID))
		return BT_IO_SEC_LOW;

	return sec_level;
}

gboolean g_attrib_unregister(GAttrib *attrib, guint id)
{
	struct event *evt;
	GSList *l;

	l = g_slist_find_custom(attrib->events, GUINT_TO_POINTER(id),
							event_cmp_by_id);
	if (l == NULL)
		return FALSE;

	evt = l->data;

	attrib->events = g_slist_remove(attrib->events, evt);

	if (evt->notify)
		evt->notify(evt->user_data);

	g_free(evt);

	return TRUE;
}

gboolean g_attrib_unregister_all(GAttrib *attrib)
{
	GSList *l;

	if (attrib->events == NULL)
		return FALSE;

	for (l = attrib->events; l; l = l->next) {
		struct event *evt = l->data;

		if (evt->notify)
			evt->notify(evt->user_data);

		g_free(evt);
	}

	g_slist_free(attrib->events);
	attrib->events = NULL;

	return TRUE;
}
