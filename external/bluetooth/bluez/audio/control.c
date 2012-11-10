/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2010  Nokia Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
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

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <gdbus.h>

#include "log.h"
#include "error.h"
#include "uinput.h"
#include "adapter.h"
#include "../src/device.h"
#include "device.h"
#include "manager.h"
#include "avdtp.h"
#include "control.h"
#include "sdpd.h"
#include "glib-helper.h"
#include "btio.h"
#include "dbus-common.h"

#define AVCTP_PSM 23
#define AVCTP_BROWSING_PSM 0x001B

/* Message types */
#define AVCTP_COMMAND		0
#define AVCTP_RESPONSE		1

/* Packet types */
#define AVCTP_PACKET_SINGLE	0
#define AVCTP_PACKET_START	1
#define AVCTP_PACKET_CONTINUE	2
#define AVCTP_PACKET_END	3

/* ctype entries */
#define CTYPE_CONTROL		0x0
#define CTYPE_STATUS		0x1
#define CTYPE_NOT_IMPLEMENTED	0x8
#define CTYPE_ACCEPTED		0x9
#define CTYPE_REJECTED		0xA
#define CTYPE_STABLE		0xC
#define CTYPE_NOTIFY		0x3
#define CTYPE_INTERIM		0xF
#define CTYPE_CHANGED		0xD

/* opcodes */
#define OP_UNITINFO		0x30
#define OP_SUBUNITINFO		0x31
#define OP_PASSTHROUGH		0x7c
#define OP_VENDORDEPENDENT	0x0

/* subunits of interest */
#define SUBUNIT_PANEL		0x09

/* operands in passthrough commands */
#define VOL_UP_OP		0x41
#define VOL_DOWN_OP		0x42
#define MUTE_OP			0x43
#define PLAY_OP			0x44
#define STOP_OP			0x45
#define PAUSE_OP		0x46
#define RECORD_OP		0x47
#define REWIND_OP		0x48
#define FAST_FORWARD_OP		0x49
#define EJECT_OP		0x4a
#define FORWARD_OP		0x4b
#define BACKWARD_OP		0x4c

#define QUIRK_NO_RELEASE	1 << 0

/* BT SIG IDs */
#define SIG_ID_BTSIG		0X1958

/* AVRCP1.3 PDU IDs */
#define PDU_GET_CAPABILITY_ID		0x10

/*Player application settings PDU IDs*/
#define PDU_LIST_APP_SETTING_ATTRIBUTES_ID		0x11
#define PDU_LIST_APP_SETTING_VALUES_ID		0x12
#define PDU_GET_CURRENT_APP_SETTING_VALUES_ID		0x13
#define PDU_SET_APP_SETTING_VALUES_ID		0x14
#define PDU_GET_APP_SETTING_ATTRIBUTE_TEXT_ID		0x15
#define PDU_GET_APP_SETTING_VALUE_TEXT_ID		0x16

#define PDU_GET_ELEMENT_ATTRIBUTES	0x20
#define PDU_RGR_NOTIFICATION_ID		0X31
#define PDU_REQ_CONTINUE_RSP_ID		0X40
#define PDU_ABORT_CONTINUE_RSP_ID	0X41
#define PDU_GET_PLAY_STATUS_ID		0x30

/* AVRCP1.3 Capability IDs */
#define CAP_COMPANY_ID		0X2
#define CAP_EVENTS_SUPPORTED_ID	0X3

/* AVRCP1.3 Supported Events */
#define EVENT_PLAYBACK_STATUS_CHANGED	0x1
#define EVENT_TRACK_CHANGED		0x2
#define EVENT_PLAYBACK_POS_CHANGED		0x5
#define EVENT_PLAYER_APPLICATION_SETTING_CHANGED		0x8
#define EVENT_AVAILABLE_PLAYERS_CHANGED	0xa
#define EVENT_ADDRESSED_PLAYER_CHANGED	0xb

/* AVRCP1.3 Error/Staus Codes */
#define ERROR_INVALID_PDU	0x00
#define ERROR_INVALID_PARAMETER	0x01
#define ERROR_PARAM_NOT_FOUND	0X02
#define ERROR_INTERNAL		0X03
#define STATUS_OP_COMPLETED	0X04
#define STATUS_UID_CHANGED	0X05
#define ERROR_INVALID_DIRECTION	0X07
#define ERROR_NO_DIRECTORY	0X08
#define ERROR_UID_NOT_EXIST	0X09

/* AVRCP1.3 MetaData Attributes ID */
#define METADATA_DEFAULT_MASK	0x7F
#define METADATA_TITLE		0X1
#define METADATA_ARTIST		0X2
#define METADATA_ALBUM		0X3
#define METADATA_MEDIA_NUMBER	0X4
#define METADATA_TOTAL_MEDIA	0X5
#define METADATA_GENRE		0X6
#define METADATA_PLAYING_TIME	0X7

#define METADATA_MAX_STRING_LEN	150
#define METADATA_MAX_NUMBER_LEN	40
#define DEFAULT_METADATA_STRING	"Unknown"
#define DEFAULT_METADATA_NUMBER	"1234567890"
#define METADATA_MAXIMUM_CNT	7
#define METADATA_SUPPORTED_CNT	7
#define AVRCP_MAX_PKT_SIZE	512

/* AVRCP1.3 Character set */
#define CHARACTER_SET_UTF8	0X6A

/* AVRCP1.3 Playback status */
#define STATUS_STOPPED	0X00
#define STATUS_PLAYING	0X01
#define STATUS_PAUSED	0X02
#define STATUS_FWD_SEEK	0X03
#define STATUS_REV_SEEK	0X04
#define STATUS_ERROR	0XFF

/* AVRCP1.3 Player Standerd Attributes */
#define ATTRIB_EQUALIZER 0x01
#define ATTRIB_REPEATMODE 0x02
#define ATTRIB_SHUFFLEMODE 0x03
#define ATTRIB_SCANMODE 0x04

static DBusConnection *connection = NULL;
static gchar *input_device_name = NULL;
static GSList *servers = NULL;

#if __BYTE_ORDER == __LITTLE_ENDIAN

struct avctp_header {
	uint8_t ipid:1;
	uint8_t cr:1;
	uint8_t packet_type:2;
	uint8_t transaction:4;
	uint16_t pid;
} __attribute__ ((packed));
#define AVCTP_HEADER_LENGTH 3

struct avrcp_header {
	uint8_t code:4;
	uint8_t _hdr0:4;
	uint8_t subunit_id:3;
	uint8_t subunit_type:5;
	uint8_t opcode;
} __attribute__ ((packed));
#define AVRCP_HEADER_LENGTH 3

struct avrcp_params {
	uint32_t company_id:24;
	uint32_t pdu_id:8;
	uint8_t packet_type:2;
	uint8_t reserved:6;
	uint16_t param_len;
	uint8_t capability_id;
} __attribute__ ((packed));
#define AVRCP_PKT_PARAMS_LEN 8;

#elif __BYTE_ORDER == __BIG_ENDIAN

struct avctp_header {
	uint8_t transaction:4;
	uint8_t packet_type:2;
	uint8_t cr:1;
	uint8_t ipid:1;
	uint16_t pid;
} __attribute__ ((packed));
#define AVCTP_HEADER_LENGTH 3

struct avrcp_header {
	uint8_t _hdr0:4;
	uint8_t code:4;
	uint8_t subunit_type:5;
	uint8_t subunit_id:3;
	uint8_t opcode;
} __attribute__ ((packed));
#define AVRCP_HEADER_LENGTH 3

struct avrcp_params {
	uint32_t pdu_id:8;
	uint32_t company_id:24;
	uint8_t reserved:6;
	uint8_t packet_type:2;
	uint16_t param_len;
	uint8_t capability_id;
} __attribute__ ((packed));
#define AVRCP_PKT_PARAMS_LEN 8;

#else
#error "Unknown byte order"
#endif

struct avctp_state_callback {
	avctp_state_cb cb;
	void *user_data;
	unsigned int id;
};

struct avctp_server {
	bdaddr_t src;
	GIOChannel *io;
	uint32_t tg_record_id;
#ifndef ANDROID
	uint32_t ct_record_id;
#endif
};

struct meta_data {
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *media_number;
	gchar *total_media_count;
	gchar *playing_time;
	gchar *genre;
	gchar *remaining_mdata;
	int remaining_mdata_len;
	uint8_t trans_id_event_track;
	uint8_t trans_id_event_playback;
	uint8_t trans_id_event_playback_pos;
	uint8_t trans_id_event_addressed_player;
	uint8_t trans_id_event_available_palyer;
	uint8_t trans_id_get_play_status;
	gboolean reg_track_changed;
	gboolean reg_playback_status;
	gboolean reg_playback_pos;
	gboolean reg_addressed_player;
	gboolean reg_available_palyer;
	gboolean req_get_play_status;
	gboolean req_get_play_pos;
	uint8_t current_play_status;
	uint32_t current_position;
	guint playstatus_timer;
};

struct meta_data_field {
	uint32_t att_id;
	uint16_t char_set_id;
	uint16_t att_len;
	char val[1];
};
#define METADATA_FIELD_LEN 8

struct player_settings {
	uint32_t pending_get;
	uint8_t pending_transaction_id;
	uint8_t pending_notification_id;
	gboolean is_attr;
	gboolean reg_playersettings_status;
	int supported_attribs;
	uint8_t local_eq_value;
	uint8_t local_repeat_value;
	uint8_t local_shuffle_value;
	uint8_t local_scan_value;
};
struct control {
	struct audio_device *dev;

	avctp_state_t state;

	int uinput;

	GIOChannel *io;
	guint io_id;

	uint16_t mtu;

	gboolean target;

	uint8_t key_quirks[256];

	gboolean ignore_pause;

	struct meta_data *mdata;

	struct player_settings *ply_settings;
};

static struct {
	const char *name;
	uint8_t avrcp;
	uint16_t uinput;
} key_map[] = {
	{ "PLAY",		PLAY_OP,		KEY_PLAYCD },
	{ "STOP",		STOP_OP,		KEY_STOPCD },
	{ "PAUSE",		PAUSE_OP,		KEY_PAUSECD },
	{ "FORWARD",		FORWARD_OP,		KEY_NEXTSONG },
	{ "BACKWARD",		BACKWARD_OP,		KEY_PREVIOUSSONG },
	{ "REWIND",		REWIND_OP,		KEY_REWIND },
	{ "FAST FORWARD",	FAST_FORWARD_OP,	KEY_FASTFORWARD },
	{ NULL }
};

static GSList *avctp_callbacks = NULL;

static void auth_cb(DBusError *derr, void *user_data);

static int send_meta_data(struct control *control, uint8_t trans_id,
				uint8_t att_mask, uint8_t att_count);
static int send_meta_data_continue_response(struct control *control,
				uint8_t trans_id);
static int send_notification(struct control *control,
		uint16_t event_id, uint16_t event_data);
static int send_play_status(struct control *control, uint32_t song_len,
                        uint32_t song_position, uint8_t play_status);

static int send_playback_pos_notification(struct control *control);

static gboolean send_playback_pos_request(gpointer userdata);

static sdp_record_t *avrcp_ct_record(void)
{
	sdp_list_t *svclass_id, *pfseq, *apseq, *root;
	uuid_t root_uuid, l2cap, avctp, avrct;
	sdp_profile_desc_t profile[1];
	sdp_list_t *aproto, *proto[2];
	sdp_record_t *record;
	sdp_data_t *psm, *version, *features;
	uint16_t lp = AVCTP_PSM;
	uint16_t avrcp_ver = 0x0100, avctp_ver = 0x0103, feat = 0x000f;

	record = sdp_record_alloc();
	if (!record)
		return NULL;

	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root = sdp_list_append(0, &root_uuid);
	sdp_set_browse_groups(record, root);

	/* Service Class ID List */
	sdp_uuid16_create(&avrct, AV_REMOTE_SVCLASS_ID);
	svclass_id = sdp_list_append(0, &avrct);
	sdp_set_service_classes(record, svclass_id);

	/* Protocol Descriptor List */
	sdp_uuid16_create(&l2cap, L2CAP_UUID);
	proto[0] = sdp_list_append(0, &l2cap);
	psm = sdp_data_alloc(SDP_UINT16, &lp);
	proto[0] = sdp_list_append(proto[0], psm);
	apseq = sdp_list_append(0, proto[0]);

	sdp_uuid16_create(&avctp, AVCTP_UUID);
	proto[1] = sdp_list_append(0, &avctp);
	version = sdp_data_alloc(SDP_UINT16, &avctp_ver);
	proto[1] = sdp_list_append(proto[1], version);
	apseq = sdp_list_append(apseq, proto[1]);

	aproto = sdp_list_append(0, apseq);
	sdp_set_access_protos(record, aproto);

	/* Bluetooth Profile Descriptor List */
	sdp_uuid16_create(&profile[0].uuid, AV_REMOTE_PROFILE_ID);
	profile[0].version = avrcp_ver;
	pfseq = sdp_list_append(0, &profile[0]);
	sdp_set_profile_descs(record, pfseq);

	features = sdp_data_alloc(SDP_UINT16, &feat);
	sdp_attr_add(record, SDP_ATTR_SUPPORTED_FEATURES, features);

	sdp_set_info_attr(record, "AVRCP CT", 0, 0);

	free(psm);
	free(version);
	sdp_list_free(proto[0], 0);
	sdp_list_free(proto[1], 0);
	sdp_list_free(apseq, 0);
	sdp_list_free(pfseq, 0);
	sdp_list_free(aproto, 0);
	sdp_list_free(root, 0);
	sdp_list_free(svclass_id, 0);

	return record;
}

static sdp_record_t *avrcp_tg_record(void)
{
	sdp_list_t *svclass_id, *pfseq, *apseq, *root;
	uuid_t root_uuid, l2cap, avctp, avrtg;
	sdp_profile_desc_t profile[1];
	sdp_list_t *aproto, *proto[2];
	sdp_record_t *record;
	sdp_data_t *psm, *version, *features;
	uint16_t lp = AVCTP_PSM;
	uint16_t browsing_psm = AVCTP_BROWSING_PSM;
	uint16_t avrcp_ver = 0x0103, avctp_ver = 0x0103, feat = 0x000f;

#ifdef ANDROID
	feat = 0x0001;
#endif
	record = sdp_record_alloc();
	if (!record)
		return NULL;

	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root = sdp_list_append(0, &root_uuid);
	sdp_set_browse_groups(record, root);

	/* Service Class ID List */
	sdp_uuid16_create(&avrtg, AV_REMOTE_TARGET_SVCLASS_ID);
	svclass_id = sdp_list_append(0, &avrtg);
	sdp_set_service_classes(record, svclass_id);

	/* Protocol Descriptor List */
	sdp_uuid16_create(&l2cap, L2CAP_UUID);
	proto[0] = sdp_list_append(0, &l2cap);
	psm = sdp_data_alloc(SDP_UINT16, &lp);
	proto[0] = sdp_list_append(proto[0], psm);
	apseq = sdp_list_append(0, proto[0]);

	sdp_uuid16_create(&avctp, AVCTP_UUID);
	proto[1] = sdp_list_append(0, &avctp);
	version = sdp_data_alloc(SDP_UINT16, &avctp_ver);
	proto[1] = sdp_list_append(proto[1], version);
	apseq = sdp_list_append(apseq, proto[1]);

	aproto = sdp_list_append(0, apseq);
	sdp_set_access_protos(record, aproto);

	/* Bluetooth Profile Descriptor List */
	sdp_uuid16_create(&profile[0].uuid, AV_REMOTE_PROFILE_ID);
	profile[0].version = avrcp_ver;
	pfseq = sdp_list_append(0, &profile[0]);
	sdp_set_profile_descs(record, pfseq);

	features = sdp_data_alloc(SDP_UINT16, &feat);
	sdp_attr_add(record, SDP_ATTR_SUPPORTED_FEATURES, features);

	sdp_set_info_attr(record, "AVRCP TG", 0, 0);

	free(psm);
	free(version);
	sdp_list_free(proto[0], 0);
	sdp_list_free(proto[1], 0);
	sdp_list_free(apseq, 0);
	sdp_list_free(aproto, 0);
	sdp_list_free(pfseq, 0);
	sdp_list_free(root, 0);
	sdp_list_free(svclass_id, 0);

	return record;
}

static int send_event(int fd, uint16_t type, uint16_t code, int32_t value)
{
	struct uinput_event event;

	memset(&event, 0, sizeof(event));
	event.type	= type;
	event.code	= code;
	event.value	= value;

	return write(fd, &event, sizeof(event));
}

static void send_key(int fd, uint16_t key, int pressed)
{
	if (fd < 0)
		return;

	send_event(fd, EV_KEY, key, pressed);
	send_event(fd, EV_SYN, SYN_REPORT, 0);
}

static gboolean handle_key_op (struct control *control,
				const unsigned char op, int pressed)
{
	int i;
	for (i = 0; key_map[i].name != NULL; i++) {
		uint8_t key_quirks;

		if ((op & 0x7F) != key_map[i].avrcp)
			continue;

		DBG("AVRCP: %s %d", key_map[i].name, pressed);

		key_quirks = control->key_quirks[key_map[i].avrcp];

		if (key_quirks & QUIRK_NO_RELEASE) {
			if (!pressed) {
				DBG("AVRCP: Ignoring release");
				break;
			}

			DBG("AVRCP: treating key press as press + release");
			send_key(control->uinput, key_map[i].uinput, 1);
			send_key(control->uinput, key_map[i].uinput, 0);
			break;
		}

		send_key(control->uinput, key_map[i].uinput, pressed);
		break;
	}

	if (key_map[i].name == NULL) {
		DBG("AVRCP: unknown button 0x%02X pressed =%d",
						op & 0x7F, pressed);
		return FALSE;
	}
	return TRUE;
}

static gboolean handle_panel_passthrough(struct control *control,
					const unsigned char *operands,
					int operand_count)
{
	const char *status;
	int pressed, i;

	if (operand_count == 0)
		return TRUE;

	if (operands[0] & 0x80) {
		status = "released";
		pressed = 0;
	} else {
		status = "pressed";
		pressed = 1;
	}

#ifdef ANDROID
	if ((operands[0] & 0x7F) == PAUSE_OP) {
		if (!sink_is_streaming(control->dev)) {
			if (pressed) {
				uint8_t key_quirks =
					control->key_quirks[PAUSE_OP];
				DBG("AVRCP: Ignoring Pause key - pressed");
				if (!(key_quirks & QUIRK_NO_RELEASE))
					control->ignore_pause = TRUE;
				return TRUE;
			} else if (!pressed && control->ignore_pause) {
				DBG("AVRCP: Ignoring Pause key - released");
				control->ignore_pause = FALSE;
				return TRUE;
			}
		}
	}
#endif
	return handle_key_op(control, operands[0],pressed);
}

static void avctp_disconnected(struct audio_device *dev)
{
	struct control *control = dev->control;

	if (!control)
		return;

	if (control->io) {
		g_io_channel_shutdown(control->io, TRUE, NULL);
		g_io_channel_unref(control->io);
		control->io = NULL;
	}

	if (control->io_id) {
		g_source_remove(control->io_id);
		control->io_id = 0;

		if (control->state == AVCTP_STATE_CONNECTING)
			audio_device_cancel_authorization(dev, auth_cb,
								control);
	}

	if (control->uinput >= 0) {
		char address[18];

		ba2str(&dev->dst, address);
		DBG("AVRCP: closing uinput for %s", address);

		ioctl(control->uinput, UI_DEV_DESTROY);
		close(control->uinput);
		control->uinput = -1;
	}
}

static void avctp_set_state(struct control *control, avctp_state_t new_state)
{
	GSList *l;
	struct audio_device *dev = control->dev;
	avctp_state_t old_state = control->state;
	gboolean value;

	switch (new_state) {
	case AVCTP_STATE_DISCONNECTED:
		DBG("AVCTP Disconnected");

		avctp_disconnected(control->dev);

		if (old_state != AVCTP_STATE_CONNECTED)
			break;

		value = FALSE;
		g_dbus_emit_signal(dev->conn, dev->path,
					AUDIO_CONTROL_INTERFACE,
					"Disconnected", DBUS_TYPE_INVALID);
		emit_property_changed(dev->conn, dev->path,
					AUDIO_CONTROL_INTERFACE, "Connected",
					DBUS_TYPE_BOOLEAN, &value);

		if (!audio_device_is_active(dev, NULL))
			audio_device_set_authorized(dev, FALSE);

		break;
	case AVCTP_STATE_CONNECTING:
		DBG("AVCTP Connecting");
		break;
	case AVCTP_STATE_CONNECTED:
		DBG("AVCTP Connected");
		value = TRUE;
		g_dbus_emit_signal(control->dev->conn, control->dev->path,
				AUDIO_CONTROL_INTERFACE, "Connected",
				DBUS_TYPE_INVALID);
		emit_property_changed(control->dev->conn, control->dev->path,
				AUDIO_CONTROL_INTERFACE, "Connected",
				DBUS_TYPE_BOOLEAN, &value);
		break;
	default:
		error("Invalid AVCTP state %d", new_state);
		return;
	}

	control->state = new_state;

	for (l = avctp_callbacks; l != NULL; l = l->next) {
		struct avctp_state_callback *cb = l->data;
		cb->cb(control->dev, old_state, new_state, cb->user_data);
	}
}

static gboolean control_cb(GIOChannel *chan, GIOCondition cond,
				gpointer data)
{
	struct control *control = data;
	unsigned char buf[1024], *operands;
	struct avctp_header *avctp;
	struct avrcp_header *avrcp;
	struct avrcp_params *params;
	int ret, packet_size, operand_count, sock;
	struct meta_data *mdata = control->mdata;
	struct player_settings *ply_settings = control->ply_settings;
	uint8_t param_len; // the network to host converted value

	if (cond & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		goto failed;

	sock = g_io_channel_unix_get_fd(control->io);

	ret = read(sock, buf, sizeof(buf));
	if (ret <= 0)
		goto failed;

	DBG("Got %d bytes of data for AVCTP session %p", ret, control);

	if ((unsigned int) ret < sizeof(struct avctp_header)) {
		error("Too small AVCTP packet");
		goto failed;
	}

	packet_size = ret;

	avctp = (struct avctp_header *) buf;

	DBG("AVCTP transaction %u, packet type %u, C/R %u, IPID %u, "
			"PID 0x%04X",
			avctp->transaction, avctp->packet_type,
			avctp->cr, avctp->ipid, ntohs(avctp->pid));

	ret -= sizeof(struct avctp_header);
	if ((unsigned int) ret < sizeof(struct avrcp_header)) {
		error("Too small AVRCP packet");
		goto failed;
	}

	avrcp = (struct avrcp_header *) (buf + sizeof(struct avctp_header));

	ret -= sizeof(struct avrcp_header);

	operands = buf + sizeof(struct avctp_header) + sizeof(struct avrcp_header);
	operand_count = ret;
	params = (struct avrcp_params *)(buf + sizeof(struct avctp_header) + sizeof(struct avrcp_header));

	DBG("AVRCP %s 0x%01X, subunit_type 0x%02X, subunit_id 0x%01X, "
			"opcode 0x%02X, %d operands",
			avctp->cr ? "response" : "command",
			avrcp->code, avrcp->subunit_type, avrcp->subunit_id,
			avrcp->opcode, operand_count);

	if (avctp->packet_type != AVCTP_PACKET_SINGLE) {
		avctp->cr = AVCTP_RESPONSE;
		avrcp->code = CTYPE_NOT_IMPLEMENTED;
	} else if (avctp->pid != htons(AV_REMOTE_SVCLASS_ID)) {
		avctp->ipid = 1;
		avctp->cr = AVCTP_RESPONSE;
		avrcp->code = CTYPE_REJECTED;
	} else if (avctp->cr == AVCTP_COMMAND &&
			avrcp->code == CTYPE_CONTROL &&
			avrcp->subunit_type == SUBUNIT_PANEL &&
			avrcp->opcode == OP_PASSTHROUGH) {
		gboolean handled = handle_panel_passthrough(control, operands, operand_count);
		avctp->cr = AVCTP_RESPONSE;
		if (handled == TRUE)
			avrcp->code = CTYPE_ACCEPTED;
		else
			avrcp->code = CTYPE_REJECTED;
	} else if (avctp->cr == AVCTP_COMMAND &&
			avrcp->code == CTYPE_STATUS &&
			(avrcp->opcode == OP_UNITINFO
			|| avrcp->opcode == OP_SUBUNITINFO)) {
		avctp->cr = AVCTP_RESPONSE;
		avrcp->code = CTYPE_STABLE;
		/* The first operand should be 0x07 for the UNITINFO response.
		 * Neither AVRCP (section 22.1, page 117) nor AVC Digital
		 * Interface Command Set (section 9.2.1, page 45) specs
		 * explain this value but both use it */
		if (operand_count >= 1 && avrcp->opcode == OP_UNITINFO)
			operands[0] = 0x07;
		if (operand_count >= 2)
			operands[1] = SUBUNIT_PANEL << 3;
		DBG("reply to %s", avrcp->opcode == OP_UNITINFO ?
				"OP_UNITINFO" : "OP_SUBUNITINFO");
	} else if (avctp->cr == AVCTP_COMMAND &&
			(avrcp->code == CTYPE_STATUS || avrcp->code == CTYPE_NOTIFY
			|| avrcp->code == CTYPE_CONTROL) &&
			avrcp->opcode == OP_VENDORDEPENDENT) {
		DBG("Got Vendor Dep opcode");
		if (params->pdu_id == PDU_GET_CAPABILITY_ID) {
			DBG("Pdu id is PDU_GET_CAPABILITY_ID");
			avctp->cr = AVCTP_RESPONSE;
			operands = (unsigned char *)params + AVRCP_PKT_PARAMS_LEN;
			if (params->capability_id == CAP_COMPANY_ID) {
				avrcp->code = CTYPE_STABLE;
				params->param_len = htons(5);
				operands[0] = 0x1; // Capability Count
				operands[1] = 0x00;
				//BT SIG Company id is 0x1958
				operands[2] = 0x19;
				operands[3] = 0x58;
				packet_size = packet_size + 4;
			} else if (params->capability_id == CAP_EVENTS_SUPPORTED_ID) {
				avrcp->code = CTYPE_STABLE;
				params->param_len = htons(6);
				operands[0] = 0x4; // Capability Count
				operands[1] = EVENT_PLAYBACK_STATUS_CHANGED;
				operands[2] = EVENT_TRACK_CHANGED;
				operands[3] = EVENT_PLAYBACK_POS_CHANGED;
				operands[4] = EVENT_PLAYER_APPLICATION_SETTING_CHANGED;
				packet_size = packet_size + 5;
			} else {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PARAMETER;
			}
		} else if (params->pdu_id == PDU_GET_ELEMENT_ATTRIBUTES) {
			DBG("Pdu id is PDU_GET_ELEMENT_ATTRIBUTES");
			operands = (unsigned char *)params;
			operands += AVRCP_PKT_PARAMS_LEN;
			operands += 7;
			uint8_t att_count = (int)(*operands);
			DBG("Received att_count is %d", att_count);
			uint32_t *att_id = (uint32_t *)(operands+1);
			uint8_t att_mask = 0;
			uint8_t index = 0;
			for (index = 0; index < att_count; index++) {
				int att_val = htonl(*att_id);
				att_mask |=  1 << (att_val - 1);
				att_id += 1;
			}
			if (att_count == 0) {
				att_count = METADATA_SUPPORTED_CNT;
				att_mask = METADATA_DEFAULT_MASK;
			}
			DBG("MetaData mask is %d", att_mask);
			if (att_count > METADATA_MAXIMUM_CNT) {
				att_count = METADATA_SUPPORTED_CNT;
				att_mask = METADATA_DEFAULT_MASK;
			}
			DBG("MetaData mask is %d att_count is %d", att_mask, att_count);
			send_meta_data(control, avctp->transaction, att_mask, att_count);
			return TRUE;
		} else if (params->pdu_id == PDU_REQ_CONTINUE_RSP_ID) {
			if (mdata->remaining_mdata_len == 0) {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PARAMETER;
			} else {
				send_meta_data_continue_response(control, avctp->transaction);
				return TRUE;
			}

		} else if (params->pdu_id == PDU_ABORT_CONTINUE_RSP_ID) {
			if (mdata->remaining_mdata_len == 0) {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PARAMETER;
			} else {
				mdata->remaining_mdata_len = 0;
				g_free(mdata->remaining_mdata);
				mdata->remaining_mdata = NULL;
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_ACCEPTED;
				packet_size -= 1;
			}
		} else if (params->pdu_id == PDU_RGR_NOTIFICATION_ID) {
			avctp->cr = AVCTP_RESPONSE;
			if (params->capability_id == EVENT_TRACK_CHANGED) {
				mdata->trans_id_event_track = avctp->transaction;
				mdata->reg_track_changed = TRUE;
				avrcp->code = CTYPE_INTERIM;
				operands = (unsigned char *)params;
				operands += AVRCP_PKT_PARAMS_LEN;
				int index;
				if (mdata->current_play_status == STATUS_STOPPED) {
					for (index = 0; index < 8; index++, operands++)
						*operands = 0xFF;
				} else {
					for (index = 0; index < 8; index++, operands++)
						*operands = 0x00;
				}
				params->param_len = htons(0x9);
				packet_size += 4;
			} else if (params->capability_id == EVENT_PLAYBACK_STATUS_CHANGED) {
				mdata->trans_id_event_playback = avctp->transaction;
				mdata->reg_playback_status = TRUE;
				avrcp->code = CTYPE_INTERIM;
				params->param_len = htons(0x2);
				operands = (unsigned char *)params;
				operands += AVRCP_PKT_PARAMS_LEN;
				*operands = mdata->current_play_status;
				packet_size -= 3;
			} else if (params->capability_id == EVENT_PLAYBACK_POS_CHANGED) {
				uint32_t *wordoperand;
				uint32_t timeout;
				mdata->trans_id_event_playback_pos = avctp->transaction;
				mdata->reg_playback_pos = TRUE;
				avrcp->code = CTYPE_INTERIM;
				params->param_len = htons(0x5);
				operands = (unsigned char *)params;
				operands += AVRCP_PKT_PARAMS_LEN;
				wordoperand = (uint32_t *)operands;
				timeout = ntohl(*wordoperand);
				DBG("playback position req for %d", timeout);
				//add validation for time peroid
				if (timeout > 0) {
					mdata->playstatus_timer =
							g_timeout_add_seconds(timeout,
								send_playback_pos_request,
								control);

					if (mdata->current_play_status == STATUS_STOPPED)
						*wordoperand = htonl(0xffffffff);
					else
						*wordoperand = htonl(mdata->current_position);
				} else {
					DBG("invalid timer so not registering for change");
					avctp->cr = AVCTP_RESPONSE;
					avrcp->code = CTYPE_REJECTED;
					param_len = ntohs(params->param_len);
					packet_size -= param_len;
					params->param_len = htons(0x1);
					params->capability_id = ERROR_INVALID_PARAMETER;
					packet_size += 1;
				}
			} else if (params->capability_id ==
							EVENT_PLAYER_APPLICATION_SETTING_CHANGED) {
				ply_settings->pending_notification_id = avctp->transaction;
				ply_settings->reg_playersettings_status = TRUE;
				avrcp->code = CTYPE_INTERIM;
				operands = (unsigned char *)params;
				operands += AVRCP_PKT_PARAMS_LEN;
				packet_size -= 4;
				*operands = ply_settings->supported_attribs;
				operands++; packet_size++;
				// if more attributes supported their local values to be
				// populated below.
				*operands = ATTRIB_REPEATMODE;
				operands++; packet_size++;
				*operands = ply_settings->local_repeat_value;
				operands++; packet_size++;
				*operands = ATTRIB_SHUFFLEMODE;
				operands++; packet_size++;
				*operands = ply_settings->local_shuffle_value;
				packet_size++;
				// 1 for event id, 1 for number attribs, 4 for values.
				params->param_len = htons(0x6);
			} else if (params->capability_id <
					EVENT_PLAYER_APPLICATION_SETTING_CHANGED) {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_NOT_IMPLEMENTED;
				param_len = ntohs(params->param_len);
				packet_size -= param_len;
				params->param_len = htons(0x0);
			} else {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				param_len = ntohs(params->param_len);
				packet_size -= param_len;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PARAMETER;
				packet_size += 1;
			}
		} else if (params->pdu_id == PDU_GET_PLAY_STATUS_ID) {
			g_dbus_emit_signal(control->dev->conn, control->dev->path,
					AUDIO_CONTROL_INTERFACE, "GetPlayStatus",
					DBUS_TYPE_INVALID);
			mdata->trans_id_get_play_status = avctp->transaction;
			mdata->req_get_play_status = TRUE;
			return TRUE;
		} else if (params->pdu_id == PDU_LIST_APP_SETTING_ATTRIBUTES_ID) {
			g_dbus_emit_signal(control->dev->conn, control->dev->path,
					AUDIO_CONTROL_INTERFACE, "ListPlayerAttributes",
					DBUS_TYPE_INVALID);
			ply_settings->pending_get = PDU_LIST_APP_SETTING_ATTRIBUTES_ID;
			ply_settings->pending_transaction_id = avctp->transaction;
			return TRUE;
		} else if (params->pdu_id == PDU_LIST_APP_SETTING_VALUES_ID) {
			g_dbus_emit_signal(control->dev->conn, control->dev->path,
					AUDIO_CONTROL_INTERFACE, "ListAttributeValues",
					DBUS_TYPE_BYTE, &params->capability_id,
					DBUS_TYPE_INVALID);
			ply_settings->pending_get = PDU_LIST_APP_SETTING_VALUES_ID;
			ply_settings->pending_transaction_id = avctp->transaction;
			return TRUE;
		} else if (params->pdu_id == PDU_GET_CURRENT_APP_SETTING_VALUES_ID) {
			uint8_t attribCount = params->capability_id;
			uint8_t *attribArray = g_new0(uint8_t, params->capability_id);
			uint8_t *op = (uint8_t *)params, i;

			DBG("attribute count is %d",attribCount);
			op += AVRCP_PKT_PARAMS_LEN;

			for (i = 0; i < attribCount; i++) {
				attribArray[i] = op[i];
				DBG("attribute is %d",attribArray[i]);
			}
			g_dbus_emit_signal(control->dev->conn, control->dev->path,
					AUDIO_CONTROL_INTERFACE, "GetAttributeValues",
					DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &attribArray, attribCount,
					DBUS_TYPE_INVALID);
			g_free(attribArray);
			ply_settings->pending_get = PDU_GET_CURRENT_APP_SETTING_VALUES_ID;
			ply_settings->pending_transaction_id = avctp->transaction;
			return TRUE;
		} else if (params->pdu_id == PDU_SET_APP_SETTING_VALUES_ID) {
			uint8_t attribCount = params->capability_id;
			uint8_t arraySize = attribCount*2;
			uint8_t *attribValueArray = g_new0(uint8_t, arraySize);
			uint8_t *op = (uint8_t *)params, i;
			gboolean is_valid = TRUE;
			gboolean is_supported = FALSE;
			DBG("attribute count is %d",attribCount);
			op += AVRCP_PKT_PARAMS_LEN;

			for (i = 0; i < arraySize; i++) {
				attribValueArray[i] = op[i];
				DBG("attribute/value is %d",attribValueArray[i]);
				if ((attribValueArray[i] >= 0x05  &&
					attribValueArray[i] <= 0x7f) ||
					(attribValueArray[i] > 0x7f && (i%2 != 0))) {
					is_valid = FALSE;
					break;
				} // when more params supported update below
				if ((i%2 == 0) &&
					((attribValueArray[i] == ATTRIB_REPEATMODE) ||
					(attribValueArray[i] == ATTRIB_SHUFFLEMODE)))
					is_supported = TRUE;
			}
			if ((is_valid == FALSE) || (is_supported == FALSE)) {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				param_len = ntohs(params->param_len);
				packet_size -= param_len;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PARAMETER;
				packet_size += 1;
				ret = write(sock, buf, packet_size);
				g_free(attribValueArray);
				return TRUE;
			}
			g_dbus_emit_signal(control->dev->conn, control->dev->path,
					AUDIO_CONTROL_INTERFACE, "SetAttributeValues",
					DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
					&attribValueArray, arraySize,
					DBUS_TYPE_INVALID);
			g_free(attribValueArray);
			ply_settings->pending_get = 0; // no return for set command
			ply_settings->pending_transaction_id = 0;
			params->param_len = htons(0x0);
			avctp->cr = AVCTP_RESPONSE;
			avrcp->code = CTYPE_ACCEPTED;
			packet_size -= 3;
		} else if (params->pdu_id == PDU_GET_APP_SETTING_ATTRIBUTE_TEXT_ID) {
			uint8_t attribCount = params->capability_id;
			uint8_t *attribArray = g_new0(uint8_t, params->capability_id);
			uint8_t *op = (uint8_t *)params, i;
			gboolean is_valid = TRUE;

			DBG("attribute count is %d",attribCount);
			op += AVRCP_PKT_PARAMS_LEN;

			for (i = 0; i < attribCount; i++) {
				attribArray[i] = op[i];
				DBG("attribute is %d",attribArray[i]);
				if ((attribArray[i] >= 0x05  &&
					attribArray[i] <= 0x7f)) {
					is_valid = FALSE;
					break;
				}
			}
			if (is_valid == FALSE) {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				param_len = ntohs(params->param_len);
				packet_size -= param_len;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PARAMETER;
				packet_size += 1;
				ret = write(sock, buf, packet_size);
				g_free(attribArray);
				return TRUE;
			}
			g_dbus_emit_signal(control->dev->conn, control->dev->path,
					AUDIO_CONTROL_INTERFACE, "ListPlayerAttributesText",
					DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &attribArray, attribCount,
					DBUS_TYPE_INVALID);
			g_free(attribArray);
			ply_settings->pending_get = PDU_GET_APP_SETTING_ATTRIBUTE_TEXT_ID;
			ply_settings->pending_transaction_id = avctp->transaction;
			ply_settings->is_attr = TRUE;
			return TRUE;
		} else if (params->pdu_id == PDU_GET_APP_SETTING_VALUE_TEXT_ID) {
			uint8_t attribValue = params->capability_id;
			uint8_t *op = (uint8_t *)params+AVRCP_PKT_PARAMS_LEN;
			uint8_t valueCount = *op;
			uint8_t *valueArray = g_new0(uint8_t, valueCount);
			int i;
			gboolean is_valid = TRUE;

			DBG("attribute value is %d while count is %d",attribValue, valueCount);

			op++;
			for (i = 0; i < valueCount; i++) {
				valueArray[i] = op[i];
				DBG("value is %d",valueArray[i]);
				if ((valueArray[i] >= 0x05  &&
					valueArray[i] <= 0x7f)) {
					is_valid = FALSE;
					break;
				}
			}
			if (is_valid == FALSE) {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				param_len = ntohs(params->param_len);
				packet_size -= param_len;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PARAMETER;
				packet_size += 1;
				ret = write(sock, buf, packet_size);
				g_free(valueArray);
				return TRUE;
			}
			g_dbus_emit_signal(control->dev->conn, control->dev->path,
					AUDIO_CONTROL_INTERFACE, "ListAttributeValuesText",
					DBUS_TYPE_BYTE, &attribValue,
					DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &valueArray, valueCount,
					DBUS_TYPE_INVALID);
			g_free(valueArray);
			ply_settings->pending_get = PDU_GET_APP_SETTING_VALUE_TEXT_ID;
			ply_settings->pending_transaction_id = avctp->transaction;
			ply_settings->is_attr = FALSE;
			return TRUE;
		} else {
				avctp->cr = AVCTP_RESPONSE;
				avrcp->code = CTYPE_REJECTED;
				params->param_len = htons(0x1);
				params->capability_id = ERROR_INVALID_PDU;
				packet_size += 1;
		}
	} else {
		avctp->cr = AVCTP_RESPONSE;
		avrcp->code = CTYPE_REJECTED;
	}
	ret = write(sock, buf, packet_size);
	if (ret != packet_size)
		goto failed;

	return TRUE;

failed:
	DBG("AVCTP session %p got disconnected", control);
	avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
	return FALSE;
}

static int uinput_create(char *name)
{
	struct uinput_dev dev;
	int fd, err, i;

	fd = open("/dev/uinput", O_RDWR);
	if (fd < 0) {
		fd = open("/dev/input/uinput", O_RDWR);
		if (fd < 0) {
			fd = open("/dev/misc/uinput", O_RDWR);
			if (fd < 0) {
				err = errno;
				error("Can't open input device: %s (%d)",
							strerror(err), err);
				return -err;
			}
		}
	}

	memset(&dev, 0, sizeof(dev));
	if (name)
		strncpy(dev.name, name, UINPUT_MAX_NAME_SIZE - 1);

	dev.id.bustype = BUS_BLUETOOTH;
	dev.id.vendor  = 0x0000;
	dev.id.product = 0x0000;
	dev.id.version = 0x0000;

	if (write(fd, &dev, sizeof(dev)) < 0) {
		err = errno;
		error("Can't write device information: %s (%d)",
						strerror(err), err);
		close(fd);
		errno = err;
		return -err;
	}

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_REL);
	ioctl(fd, UI_SET_EVBIT, EV_REP);
	ioctl(fd, UI_SET_EVBIT, EV_SYN);

	for (i = 0; key_map[i].name != NULL; i++)
		ioctl(fd, UI_SET_KEYBIT, key_map[i].uinput);

	if (ioctl(fd, UI_DEV_CREATE, NULL) < 0) {
		err = errno;
		error("Can't create uinput device: %s (%d)",
						strerror(err), err);
		close(fd);
		errno = err;
		return -err;
	}

	return fd;
}

static void init_uinput(struct control *control)
{
	struct audio_device *dev = control->dev;
	char address[18], name[248 + 1], *uinput_dev_name;

	device_get_name(dev->btd_dev, name, sizeof(name));
	if (g_str_equal(name, "Nokia CK-20W")) {
		control->key_quirks[FORWARD_OP] |= QUIRK_NO_RELEASE;
		control->key_quirks[BACKWARD_OP] |= QUIRK_NO_RELEASE;
		control->key_quirks[PLAY_OP] |= QUIRK_NO_RELEASE;
		control->key_quirks[PAUSE_OP] |= QUIRK_NO_RELEASE;
	}

	ba2str(&dev->dst, address);

	/* Use device name from config file if specified */
	uinput_dev_name = input_device_name;
	if (!uinput_dev_name)
		uinput_dev_name = address;

	if (dev->uinput >= 0) {
		ioctl(dev->uinput, UI_DEV_DESTROY);
		close(dev->uinput);
		dev->uinput = -1;
	}
	control->uinput = uinput_create(uinput_dev_name);
	if (control->uinput < 0)
		error("AVRCP: failed to init uinput for %s", address);
	else
		DBG("AVRCP: uinput initialized for %s", address);
}

static void avctp_connect_cb(GIOChannel *chan, GError *err, gpointer data)
{
	struct control *control = data;
	char address[18];
	uint16_t imtu;
	GError *gerr = NULL;

	if (err) {
		avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
		error("%s", err->message);
		return;
	}

	bt_io_get(chan, BT_IO_L2CAP, &gerr,
			BT_IO_OPT_DEST, &address,
			BT_IO_OPT_IMTU, &imtu,
			BT_IO_OPT_INVALID);
	if (gerr) {
		avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
		error("%s", gerr->message);
		g_error_free(gerr);
		return;
	}

	DBG("AVCTP: connected to %s", address);

	if (!control->io)
		control->io = g_io_channel_ref(chan);

	init_uinput(control);

	avctp_set_state(control, AVCTP_STATE_CONNECTED);
	control->mtu = imtu;
	control->io_id = g_io_add_watch(chan,
				G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
				(GIOFunc) control_cb, control);
}

static void auth_cb(DBusError *derr, void *user_data)
{
	struct control *control = user_data;
	GError *err = NULL;
	struct audio_device *dev = control->dev;
	struct avdtp *session;

	if (control->io_id) {
		g_source_remove(control->io_id);
		control->io_id = 0;
	}

	if (derr && dbus_error_is_set(derr)) {
		error("Access denied: %s", derr->message);
		avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
		return;
	}

	if (dev->sink &&
		!avdtp_is_connected(&dev->src, &dev->dst)) {
		session = avdtp_get(&dev->src, &dev->dst);
		if (session) {
			DBG("sending connect");
			sink_setup_stream(dev->sink, session);
			avdtp_unref(session);
		}
	}

	if (!bt_io_accept(control->io, avctp_connect_cb, control,
								NULL, &err)) {
		error("bt_io_accept: %s", err->message);
		g_error_free(err);
		avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
	}
}

static void avctp_confirm_cb(GIOChannel *chan, gpointer data)
{
	struct control *control = NULL;
	struct audio_device *dev;
	char address[18];
	bdaddr_t src, dst;
	GError *err = NULL;

	bt_io_get(chan, BT_IO_L2CAP, &err,
			BT_IO_OPT_SOURCE_BDADDR, &src,
			BT_IO_OPT_DEST_BDADDR, &dst,
			BT_IO_OPT_DEST, address,
			BT_IO_OPT_INVALID);
	if (err) {
		error("%s", err->message);
		g_error_free(err);
		g_io_channel_shutdown(chan, TRUE, NULL);
		return;
	}

	dev = manager_get_device(&src, &dst, TRUE);
	if (!dev) {
		error("Unable to get audio device object for %s", address);
		goto drop;
	}

	if (!dev->control) {
		btd_device_add_uuid(dev->btd_dev, AVRCP_REMOTE_UUID);
		if (!dev->control)
			goto drop;
	}

	control = dev->control;

	if (control->io) {
		error("Refusing unexpected connect from %s", address);
		goto drop;
	}

	avctp_set_state(control, AVCTP_STATE_CONNECTING);
	control->io = g_io_channel_ref(chan);

	if (audio_device_request_authorization(dev, AVRCP_TARGET_UUID,
						auth_cb, dev->control) < 0)
		goto drop;

	control->io_id = g_io_add_watch(chan, G_IO_ERR | G_IO_HUP | G_IO_NVAL,
							control_cb, control);
	return;

drop:
	if (!control || !control->io)
		g_io_channel_shutdown(chan, TRUE, NULL);
	if (control)
		avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
}

static GIOChannel *avctp_server_socket(const bdaddr_t *src, gboolean master)
{
	GError *err = NULL;
	GIOChannel *io;

	io = bt_io_listen(BT_IO_L2CAP, NULL, avctp_confirm_cb, NULL,
				NULL, &err,
				BT_IO_OPT_SOURCE_BDADDR, src,
				BT_IO_OPT_PSM, AVCTP_PSM,
				BT_IO_OPT_SEC_LEVEL, BT_IO_SEC_MEDIUM,
				BT_IO_OPT_MASTER, master,
				BT_IO_OPT_INVALID);
	if (!io) {
		error("%s", err->message);
		g_error_free(err);
	}

	return io;
}

gboolean avrcp_connect(struct audio_device *dev)
{
	struct control *control = dev->control;
	GError *err = NULL;
	GIOChannel *io;

	if (control->state > AVCTP_STATE_DISCONNECTED)
		return TRUE;

	avctp_set_state(control, AVCTP_STATE_CONNECTING);

	io = bt_io_connect(BT_IO_L2CAP, avctp_connect_cb, control, NULL, &err,
				BT_IO_OPT_SOURCE_BDADDR, &dev->src,
				BT_IO_OPT_DEST_BDADDR, &dev->dst,
				BT_IO_OPT_PSM, AVCTP_PSM,
				BT_IO_OPT_INVALID);
	if (err) {
		avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
		error("%s", err->message);
		g_error_free(err);
		return FALSE;
	}

	control->io = io;

	return TRUE;
}

void avrcp_disconnect(struct audio_device *dev)
{
	struct control *control = dev->control;

	if (!(control && control->io))
		return;

	avctp_set_state(control, AVCTP_STATE_DISCONNECTED);
}

int avrcp_register(DBusConnection *conn, const bdaddr_t *src, GKeyFile *config)
{
	sdp_record_t *record;
	gboolean tmp, master = TRUE;
	GError *err = NULL;
	struct avctp_server *server;

	if (config) {
		tmp = g_key_file_get_boolean(config, "General",
							"Master", &err);
		if (err) {
			DBG("audio.conf: %s", err->message);
			g_error_free(err);
		} else
			master = tmp;
		err = NULL;
		input_device_name = g_key_file_get_string(config,
			"AVRCP", "InputDeviceName", &err);
		if (err) {
			DBG("audio.conf: %s", err->message);
			input_device_name = NULL;
			g_error_free(err);
		}
	}

	server = g_new0(struct avctp_server, 1);
	if (!server)
		return -ENOMEM;

	if (!connection)
		connection = dbus_connection_ref(conn);

	record = avrcp_tg_record();
	if (!record) {
		error("Unable to allocate new service record");
		g_free(server);
		return -1;
	}

	if (add_record_to_server(src, record) < 0) {
		error("Unable to register AVRCP target service record");
		g_free(server);
		sdp_record_free(record);
		return -1;
	}
	server->tg_record_id = record->handle;

#ifndef ANDROID
	record = avrcp_ct_record();
	if (!record) {
		error("Unable to allocate new service record");
		g_free(server);
		return -1;
	}

	if (add_record_to_server(src, record) < 0) {
		error("Unable to register AVRCP controller service record");
		sdp_record_free(record);
		g_free(server);
		return -1;
	}
	server->ct_record_id = record->handle;
#endif

	server->io = avctp_server_socket(src, master);
	if (!server->io) {
#ifndef ANDROID
		remove_record_from_server(server->ct_record_id);
#endif
		remove_record_from_server(server->tg_record_id);
		g_free(server);
		return -1;
	}

	bacpy(&server->src, src);

	servers = g_slist_append(servers, server);

	return 0;
}

static struct avctp_server *find_server(GSList *list, const bdaddr_t *src)
{
	for (; list; list = list->next) {
		struct avctp_server *server = list->data;

		if (bacmp(&server->src, src) == 0)
			return server;
	}

	return NULL;
}

void avrcp_unregister(const bdaddr_t *src)
{
	struct avctp_server *server;

	server = find_server(servers, src);
	if (!server)
		return;

	servers = g_slist_remove(servers, server);

#ifndef ANDROID
	remove_record_from_server(server->ct_record_id);
#endif
	remove_record_from_server(server->tg_record_id);

	g_io_channel_shutdown(server->io, TRUE, NULL);
	g_io_channel_unref(server->io);
	g_free(server);

	if (servers)
		return;

	dbus_connection_unref(connection);
	connection = NULL;
}

static DBusMessage *control_is_connected(DBusConnection *conn,
						DBusMessage *msg,
						void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	DBusMessage *reply;
	dbus_bool_t connected;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	connected = (control->state == AVCTP_STATE_CONNECTED);

	dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &connected,
					DBUS_TYPE_INVALID);

	return reply;
}

static int avctp_send_passthrough(struct control *control, uint8_t op)
{
	unsigned char buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + 2];
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	uint8_t *operands = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];
	int sk = g_io_channel_unix_get_fd(control->io);
	static uint8_t transaction = 0;

	memset(buf, 0, sizeof(buf));

	avctp->transaction = transaction++;
	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_COMMAND;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);

	avrcp->code = CTYPE_CONTROL;
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_PASSTHROUGH;

	operands[0] = op & 0x7f;
	operands[1] = 0;

	if (write(sk, buf, sizeof(buf)) < 0)
		return -errno;

	/* Button release */
	avctp->transaction = transaction++;
	operands[0] |= 0x80;

	if (write(sk, buf, sizeof(buf)) < 0)
		return -errno;

	return 0;
}

static DBusMessage *volume_up(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	DBusMessage *reply;
	int err;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	if (control->state != AVCTP_STATE_CONNECTED)
		return btd_error_not_connected(msg);

	if (!control->target)
		return btd_error_not_supported(msg);

	err = avctp_send_passthrough(control, VOL_UP_OP);
	if (err < 0)
		return btd_error_failed(msg, strerror(-err));

	return dbus_message_new_method_return(msg);
}

static DBusMessage *volume_down(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	DBusMessage *reply;
	int err;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	if (control->state != AVCTP_STATE_CONNECTED)
		return btd_error_not_connected(msg);

	if (!control->target)
		return btd_error_not_supported(msg);

	err = avctp_send_passthrough(control, VOL_DOWN_OP);
	if (err < 0)
		return btd_error_failed(msg, strerror(-err));

	return dbus_message_new_method_return(msg);
}

static DBusMessage *update_notification(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	struct meta_data *mdata = control->mdata;
	DBusMessage *reply;
	uint16_t event_id;
	uint64_t event_data;
	int err;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT16, &event_id,
						DBUS_TYPE_UINT64, &event_data,
						DBUS_TYPE_INVALID) == FALSE) {
		return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
	}

	DBG("Notification data is %d %d", (int)event_id, (int)event_data);

	if (control->state != AVCTP_STATE_CONNECTED) {
		if (event_id == EVENT_PLAYBACK_STATUS_CHANGED && mdata != NULL)
			mdata->current_play_status = (uint8_t)event_data;
		return g_dbus_create_error(msg,
			ERROR_INTERFACE ".NotConnected",
				"Device not Connected");
	}
	send_notification(control, event_id, event_data);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *update_play_status(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	struct meta_data *mdata = control->mdata;
	DBusMessage *reply;
	uint32_t duration, position;
	uint32_t play_status;
	int err;
        DBG("update_play_status called");

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &duration,
						DBUS_TYPE_UINT32, &position,
						DBUS_TYPE_UINT32, &play_status,
						DBUS_TYPE_INVALID) == FALSE) {
		return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
	}

	DBG("PlayStatus data is %d %d %d", duration, position, play_status);
	mdata->current_play_status = (uint8_t)play_status;
	mdata->current_position = (uint32_t)position;

	if (control->state != AVCTP_STATE_CONNECTED)
		return g_dbus_create_error(msg,
			ERROR_INTERFACE ".NotConnected",
				"Device not Connected");

	if (mdata->req_get_play_status == TRUE)
		send_play_status(control, duration, position, play_status);

	if ((mdata->req_get_play_pos == TRUE) &&
		(play_status == STATUS_PLAYING))
			send_playback_pos_notification(control);

	return dbus_message_new_method_return(msg);
}


static DBusMessage *update_metadata(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	struct meta_data *mdata = control->mdata;
	DBusMessage *reply;
	const gchar *title, *artist, *album, *genre;
	const gchar *media_number, *total_media_count, *playing_time;
	int err;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &title,
						DBUS_TYPE_STRING, &artist,
						DBUS_TYPE_STRING, &album,
						DBUS_TYPE_STRING, &media_number,
						DBUS_TYPE_STRING, &total_media_count,
						DBUS_TYPE_STRING, &playing_time,
						DBUS_TYPE_STRING, &genre,
						DBUS_TYPE_INVALID) == FALSE) {
		return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
	}

	DBG("MetaData is %s %s %s %s %s %s %s", title, artist, album, media_number,
			total_media_count, playing_time, genre);
	if (strlen(title) < METADATA_MAX_STRING_LEN)
		strcpy(mdata->title, title);
	else {
		strncpy(mdata->title, title, (METADATA_MAX_STRING_LEN-1));
		mdata->title[METADATA_MAX_STRING_LEN-1] = '\0';
	}
	if (strlen(artist) < METADATA_MAX_STRING_LEN)
		strcpy(mdata->artist, artist);
	else {
		strncpy(mdata->artist, artist, (METADATA_MAX_STRING_LEN-1));
		mdata->artist[METADATA_MAX_STRING_LEN-1] = '\0';
	}
	if (strlen(album) < METADATA_MAX_STRING_LEN)
		strcpy(mdata->album, album);
	else {
		strncpy(mdata->album, album, (METADATA_MAX_STRING_LEN-1));
		mdata->album[METADATA_MAX_STRING_LEN-1] = '\0';
	}
	if (strlen(media_number) < METADATA_MAX_NUMBER_LEN)
		strcpy(mdata->media_number, media_number);
	else {
		strncpy(mdata->media_number, media_number, (METADATA_MAX_NUMBER_LEN-1));
		mdata->media_number[METADATA_MAX_STRING_LEN-1] = '\0';
	}
	if (strlen(total_media_count) < METADATA_MAX_NUMBER_LEN)
		strcpy(mdata->total_media_count, total_media_count);
	else {
		strncpy(mdata->total_media_count, total_media_count, (METADATA_MAX_NUMBER_LEN-1));
		mdata->total_media_count[METADATA_MAX_STRING_LEN-1] = '\0';
	}
	if (strlen(playing_time) < METADATA_MAX_NUMBER_LEN)
		strcpy(mdata->playing_time, playing_time);
	else {
		strncpy(mdata->playing_time, playing_time, (METADATA_MAX_NUMBER_LEN-1));
		mdata->playing_time[METADATA_MAX_STRING_LEN-1] = '\0';
	}
	if (strlen(genre) < METADATA_MAX_STRING_LEN)
		strcpy(mdata->genre, genre);
	else {
		strncpy(mdata->genre, genre, (METADATA_MAX_STRING_LEN-1));
		mdata->genre[METADATA_MAX_STRING_LEN-1] = '\0';
	}

	return dbus_message_new_method_return(msg);
}

static void fill_header (struct control *control, unsigned char **pBuf, int pdu_id) {
	unsigned char *buf = *pBuf;
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	struct avrcp_params *params =
		(struct avrcp_params *)
				(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	uint8_t *op = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];
	struct player_settings *ply_settings = control->ply_settings;

	memset(buf, 0, sizeof(buf));

	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_RESPONSE;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);

	if (pdu_id == PDU_RGR_NOTIFICATION_ID) {
		avctp->transaction = ply_settings->pending_notification_id;
		avrcp->code = CTYPE_CHANGED;
	}
	else {
		ply_settings->pending_get = 0;
		avctp->transaction = ply_settings->pending_transaction_id;
		avrcp->code = CTYPE_STABLE;
	}
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_VENDORDEPENDENT;

	//BT SIG Company id is 0x1958
	op[0] = 0x00;
	op[1] = 0x19;
	op[2] = 0x58;

	params->pdu_id = pdu_id;
	params->packet_type = AVCTP_PACKET_SINGLE;
}

static int send_supported_attributes( struct control *control, uint8_t *attribute_ids, uint8_t len)
{
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	unsigned char *buf = g_new0(unsigned char, header_len + len +1);
	struct avrcp_params *params =
			(struct avrcp_params *)
				(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	int total_len =0, sk = g_io_channel_unix_get_fd(control->io);
	struct player_settings *ply_settings = control->ply_settings;
	uint8_t *op ;
	int ret = 0, i;

	DBG("send_attributes_supported called");
	if (ply_settings->pending_get != PDU_LIST_APP_SETTING_ATTRIBUTES_ID)
		goto cleanup;

	fill_header(control, &buf, PDU_LIST_APP_SETTING_ATTRIBUTES_ID);

	params->param_len = htons(len+1);
	op = (uint8_t *)params;

	op += AVRCP_PKT_PARAMS_LEN;
	op -= 1;
	*op = len;
	op++;
	for(i = 0; i < len; i++) {
		op[i] = attribute_ids[i];
	}
	total_len = header_len + len ;
	ret = write(sk, buf, total_len);

cleanup:
	g_free(buf);
	return ret;
}

static void fill_error_header( struct control *control, unsigned char **pBuf, uint8_t pdu_id) {
	unsigned char *buf = *pBuf;
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	struct avrcp_params *params =
			(struct avrcp_params *)
				(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	struct player_settings *ply_settings = control->ply_settings;
	uint8_t *op = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];

	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_RESPONSE;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);
	avctp->transaction = ply_settings->pending_transaction_id;
	ply_settings->pending_get = 0;
	avrcp->code = CTYPE_REJECTED;
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_VENDORDEPENDENT;

	//BT SIG Company id is 0x1958
	op[0] = 0x00;
	op[1] = 0x19;
	op[2] = 0x58;

	params->pdu_id = pdu_id;
	params->param_len = htons(0x1);
	params->capability_id = ERROR_INVALID_PARAMETER;
}

static int send_supported_values( struct control *control, uint8_t *value_ids, uint8_t len)
{
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	unsigned char *buf = g_new0(unsigned char, header_len + len +1);
	struct avrcp_params *params =
			(struct avrcp_params *)
				(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	int total_len =0, sk = g_io_channel_unix_get_fd(control->io);
	struct player_settings *ply_settings = control->ply_settings;
	uint8_t *op ;
	int ret = 0, i;

	DBG("send_values_values called");
	if (ply_settings->pending_get != PDU_LIST_APP_SETTING_VALUES_ID)
		goto cleanup;

	if( len <= 1) {
		fill_error_header( control, &buf, PDU_LIST_APP_SETTING_VALUES_ID);
		ret = write(sk,buf, header_len);
		goto cleanup;
	}
	fill_header(control, &buf, PDU_LIST_APP_SETTING_VALUES_ID);

	params->param_len = htons(len+1);
	op = (uint8_t *)params;

	op += AVRCP_PKT_PARAMS_LEN;
	op -= 1;
	*op = len;
	op++;
	for(i = 0; i < len; i++) {
		op[i] = value_ids[i];
	}
	total_len = header_len + len ;
	ret = write(sk, buf, total_len);

cleanup:
	g_free(buf);
	return ret;
}

static int get_valid_values(struct control *control, uint8_t *value_ids, uint8_t len)
{
	int valid_items = 0, i;
	struct player_settings *ply_settings = control->ply_settings;

	for (i = 0; i < len/2; i++) {
		switch (value_ids[2*i]) {
			case ATTRIB_EQUALIZER:
				if (value_ids[2*i+1] != 0x00) {
					ply_settings->local_eq_value = value_ids[2*i+1];
					valid_items++;
				}
			break;
			case ATTRIB_REPEATMODE:
				if (value_ids[2*i+1] != 0x00) {
					ply_settings->local_repeat_value = value_ids[2*i+1];
					valid_items++;
				}
			break;
			case ATTRIB_SHUFFLEMODE:
				if (value_ids[2*i+1] != 0x00) {
					ply_settings->local_shuffle_value = value_ids[2*i+1];
					valid_items++;
				}
			break;
			case ATTRIB_SCANMODE:
				if (value_ids[2*i+1] != 0x00) {
					ply_settings->local_scan_value = value_ids[2*i+1];
					valid_items++;
				}
			break;
		}
	}

	return valid_items;
}

static int send_attribute_values( struct control *control, uint8_t *value_ids, uint8_t len)
{
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	unsigned char *buf = g_new0(unsigned char, header_len + len +1);
	struct avrcp_params *params =
			(struct avrcp_params *)
				(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	int total_len = 0, sk = g_io_channel_unix_get_fd(control->io);
	struct player_settings *ply_settings = control->ply_settings;
	uint8_t *op ;
	int ret = 0, i;

	DBG("send_attributes_values called");
	get_valid_values(control, value_ids, len);

	if (ply_settings->pending_get == PDU_GET_CURRENT_APP_SETTING_VALUES_ID) {
		DBG("get cmd");
		if (0 == get_valid_values(control, value_ids, len)) {
			fill_error_header( control, &buf, PDU_GET_CURRENT_APP_SETTING_VALUES_ID);
			ret = write(sk,buf, header_len);
			goto cleanup;
		}
		fill_header(control, &buf, PDU_GET_CURRENT_APP_SETTING_VALUES_ID);
		params->param_len = htons(len+1);
		op = (uint8_t *)params;

		op += AVRCP_PKT_PARAMS_LEN;
		op -= 1;
		total_len = header_len + len;
	} else if (ply_settings->reg_playersettings_status == TRUE) {
		DBG("notification cmd");
		ply_settings->reg_playersettings_status = FALSE;
		if (0 == get_valid_values(control, value_ids, len))
			goto cleanup;
		fill_header(control, &buf, PDU_RGR_NOTIFICATION_ID);
		params->param_len = htons(len+2);
		op = (uint8_t *)params;

		op += AVRCP_PKT_PARAMS_LEN;
		op -= 1;
		*op = EVENT_PLAYER_APPLICATION_SETTING_CHANGED;
		op++;
		total_len = header_len + len + 1;
	} else {
		DBG("no mapping request");
		goto cleanup;
	}

	*op = len/2;
	op++;
	for(i = 0; i < len; i++) {
		op[i] = value_ids[i];
	}
	DBG("total len is %d", total_len);
	ret = write(sk, buf, total_len);

cleanup:
	g_free(buf);
	return ret;
}

static int getAttrStrLen(char *attr_str) {
	uint8_t *str = (uint8_t *)attr_str;
	int len = 0;
	while(str[len] != 0x00)
		len++;
	return len;
}

static int get_params_length(char **attr_strs, int len) {
	int total_len = 0, i;
	total_len += 1; //LENGTH_FOR_NUMBER_OF_PARAMS is 1byte
	total_len += 4*len; // 1 byte for attid, 2 bytes for charset, 1byte for len
	for (i = 0; i < len; i++)
		total_len += getAttrStrLen(attr_strs[i]);
	DBG("total len is %d",total_len);
	return total_len;
}

static int get_valid_value_text(char **attr_strs, int len) {
	int valid_items = 0, i;
	for (i = 0; i < len; i++)
		if (0 != getAttrStrLen(attr_strs[i]))
			valid_items ++;
	return valid_items;
}

static int send_attr_value_text( struct control *control,
				uint8_t *attr, char **attr_str, uint8_t len)
{
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	unsigned char *buf;
	struct avrcp_params *params;
	int total_len = 0, sk = g_io_channel_unix_get_fd(control->io);
	struct player_settings *ply_settings = control->ply_settings;
	uint8_t *op ;
	uint16_t *charset;
	int ret = 0, i, str_len, total_params_len;
	uint8_t pdu_id;

	DBG("send_attributes_text called");
	pdu_id = (ply_settings->is_attr == TRUE) ? PDU_GET_APP_SETTING_ATTRIBUTE_TEXT_ID :
			PDU_GET_APP_SETTING_VALUE_TEXT_ID;
	if (ply_settings->pending_get != pdu_id) {
            DBG("invalid pdu id");
            return 0;
	}

	if (0 == get_valid_value_text(attr_str, len)) {
	    buf = g_new0(unsigned char, header_len+1);
		fill_error_header( control, &buf, pdu_id);
		ret = write(sk,buf, header_len);
		goto cleanup;
	}

	total_params_len = get_params_length (attr_str, len);
	buf = g_new0(unsigned char, header_len + total_params_len +1);
	params = (struct avrcp_params *)(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	fill_header(control, &buf, pdu_id);

	params->param_len = htons(total_params_len);
	op = (uint8_t *)params;

	op += AVRCP_PKT_PARAMS_LEN;
	op -= 1;
	*op = len;
	op++;
	for(i = 0; i < len; i++) {
		*op = attr[i]; op++;
		charset = (uint16_t *)op;
		*charset = htons(CHARACTER_SET_UTF8);
		op += 2;
		str_len = getAttrStrLen(attr_str[i]);
		DBG("attr_str is %s",attr_str[i]);
		*op = str_len;
		op++;
		memcpy(op, attr_str[i], str_len);
		op += str_len;
	}
	total_len = header_len + total_params_len -1;
	DBG("write being called with len %d",total_len);
	ret = write(sk, buf, total_len);
	DBG("ret value for write is %d",ret);

cleanup:
	g_free(buf);
	return ret;
}

static gboolean send_playback_pos_request(gpointer userdata) {
	struct control *control = userdata;
	struct meta_data *mdata = control->mdata;

	if (mdata->playstatus_timer) {
		g_source_remove(mdata->playstatus_timer);
		mdata->playstatus_timer = 0;
	}

	g_dbus_emit_signal(control->dev->conn, control->dev->path,
						AUDIO_CONTROL_INTERFACE, "GetPlayStatus",
						DBUS_TYPE_INVALID);
	mdata->req_get_play_pos = TRUE;
	return FALSE; // one time timer is this.
}

static int send_playback_pos_notification(struct control *control) {
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	unsigned char buf[header_len + 4];
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	struct avrcp_params *params =
			(struct avrcp_params *)
				(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	int total_len = 0, sk = g_io_channel_unix_get_fd(control->io);
	struct meta_data *mdata = control->mdata;
	uint8_t *op = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];
	int ret = 0, i;
	uint32_t *word_op;

	DBG("send playback position %d", total_len);
	mdata->reg_playback_pos = FALSE;
	mdata->req_get_play_pos = FALSE;

	if (mdata->playstatus_timer) {
		g_source_remove(mdata->playstatus_timer);
		mdata->playstatus_timer = 0;
	}

	memset(buf, 0, sizeof(buf));

	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_RESPONSE;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);
	avctp->transaction = mdata->trans_id_event_playback_pos;
	avrcp->code = CTYPE_CHANGED;
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_VENDORDEPENDENT;

	//BT SIG Company id is 0x1958
	op[0] = 0x00;
	op[1] = 0x19;
	op[2] = 0x58;

	params->pdu_id = PDU_RGR_NOTIFICATION_ID;
	params->packet_type = AVCTP_PACKET_SINGLE;
	params->param_len = htons(0x5);
	op = (uint8_t *)params;

	op += AVRCP_PKT_PARAMS_LEN;
	op -= 1;
	*op = EVENT_PLAYBACK_POS_CHANGED;
	op++;
	word_op = (uint32_t *)op;
	if (mdata->current_play_status == STATUS_STOPPED)
		*word_op = htonl(0xffffffff);
	else
		*word_op = htonl(mdata->current_position);
	total_len = header_len + 4;
	DBG("total len is %d", total_len);
	ret = write(sk, buf, total_len);
	return ret;
}

static DBusMessage *update_supported_attributes(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	DBusMessage *reply;
	uint8_t *attribute_ids;
	uint32_t len;
	int err;
	DBG("update_supported_attributes called");

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
						&attribute_ids, &len,
						DBUS_TYPE_INVALID) == FALSE) {
		return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
	}

	DBG("Number of attributes supported is %d", len);

	if (control->state != AVCTP_STATE_CONNECTED)
		return g_dbus_create_error(msg,
			ERROR_INTERFACE ".NotConnected",
				"Device not Connected");

	send_supported_attributes(control, attribute_ids, len);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *update_attribute_values(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	DBusMessage *reply;
	uint8_t *value_ids;
	uint32_t  len, attrib;
	int err;
	DBG("update_attribute_values called");

	if (dbus_message_get_args(msg, NULL,
						DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
						&value_ids, &len,
						DBUS_TYPE_INVALID) == FALSE) {
		return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
	}

	DBG("Number of values supported is %d", len);

	if (control->state != AVCTP_STATE_CONNECTED)
		return g_dbus_create_error(msg,
			ERROR_INTERFACE ".NotConnected",
				"Device not Connected");

	send_supported_values(control, value_ids, len);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *update_current_values(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	DBusMessage *reply;
	uint8_t *value_ids;
	uint32_t len;
	int err;
	DBG("update_current_values called");

	if (dbus_message_get_args(msg, NULL,
						DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
						&value_ids, &len,
						DBUS_TYPE_INVALID) == FALSE) {
		return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
	}

	DBG("Number of values supported is %d", len);

	if (control->state != AVCTP_STATE_CONNECTED)
		return g_dbus_create_error(msg,
			ERROR_INTERFACE ".NotConnected",
				"Device not Connected");

	send_attribute_values(control, value_ids, len);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *update_attrib_values_text(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	struct audio_device *device = data;
	struct control *control = device->control;
	DBusMessage *reply;
	char **attr_strs;
	uint32_t len, len_str;
	uint8_t *attr_ids;
	int err;
	DBG("update_attribute_values_text called");

	if (dbus_message_get_args(msg, NULL,
						DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
						&attr_ids, &len,
						DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
						&attr_strs, &len_str,
						DBUS_TYPE_INVALID) == FALSE) {
		return g_dbus_create_error(msg, ERROR_INTERFACE ".InvalidArguments",
			"Invalid arguments in method call");
	}

	DBG("Number of values supported is %d", len);

	if (control->state != AVCTP_STATE_CONNECTED)
		return g_dbus_create_error(msg,
			ERROR_INTERFACE ".NotConnected",
				"Device not Connected");

	send_attr_value_text(control, attr_ids, attr_strs, len);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *control_get_properties(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct audio_device *device = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	gboolean value;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	/* Connected */
	value = (device->control->state == AVCTP_STATE_CONNECTED);
	dict_append_entry(&dict, "Connected", DBUS_TYPE_BOOLEAN, &value);

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static GDBusMethodTable control_methods[] = {
	{ "IsConnected",	"",	"b",	control_is_connected,
						G_DBUS_METHOD_FLAG_DEPRECATED },
	{ "GetProperties",	"",	"a{sv}",control_get_properties },
	{ "VolumeUp",		"",	"",	volume_up },
	{ "VolumeDown",		"",	"",	volume_down },
	{ "UpdateMetaData",	"sssssss",	"",	update_metadata },
	{ "UpdatePlayStatus",	"uuu",	"",	update_play_status },
	{ "UpdateNotification",	"qt",	"",	update_notification },
	{ "UpdateSupportedAttributes",		"ay",	"", update_supported_attributes},
	{ "UpdateSupportedValues",		"ay",	"", update_attribute_values},
	{ "UpdateCurrentValues",		"ay",	"", update_current_values},
	{ "UpdateAttributesText",		"ayas",	"", update_attrib_values_text},
	{ "UpdateValuesText",		"ayas",	"", update_attrib_values_text},
	{ NULL, NULL, NULL, NULL }
};

static GDBusSignalTable control_signals[] = {
	{ "Connected",			"",	G_DBUS_SIGNAL_FLAG_DEPRECATED},
	{ "Disconnected",		"",	G_DBUS_SIGNAL_FLAG_DEPRECATED},
	{ "PropertyChanged",		"sv"	},
	{ "GetPlayStatus",		""	},
	{ "ListPlayerAttributes",		""	},
	{ "ListAttributeValues",		"y"	},
	{ "GetAttributeValues",		"ay"	},
	{ "SetAttributeValues",		"ay"	},
	{ "ListPlayerAttributesText",		"ay"	},
	{ "ListAttributeValuesText",		"yay"	},
	{ NULL, NULL }
};

static void metadata_cleanup(struct meta_data *mdata) {
	if (mdata->title) {
		g_free(mdata->title);
		mdata->title = NULL;
	}
	if (mdata->artist) {
		g_free(mdata->artist);
		mdata->artist = NULL;
	}
	if (mdata->album) {
		g_free(mdata->album);
		mdata->album = NULL;
	}
	if (mdata->media_number) {
		g_free(mdata->media_number);
		mdata->media_number = NULL;
	}
	if (mdata->total_media_count) {
		g_free(mdata->total_media_count);
		mdata->total_media_count = NULL;
	}
	if (mdata->playing_time) {
		g_free(mdata->playing_time);
		mdata->playing_time = NULL;
	}
	if (mdata->remaining_mdata) {
		g_free(mdata->remaining_mdata);
		mdata->remaining_mdata = NULL;
		mdata->remaining_mdata_len = 0;
	}
	if (mdata->genre) {
		g_free(mdata->genre);
		mdata->genre = NULL;
	}
	if (mdata->playstatus_timer) {
		g_source_remove(mdata->playstatus_timer);
		mdata->playstatus_timer = 0;
	}

}

static void path_unregister(void *data)
{
	struct audio_device *dev = data;
	struct control *control = dev->control;

	DBG("Unregistered interface %s on path %s",
		AUDIO_CONTROL_INTERFACE, dev->path);

	if (control->state != AVCTP_STATE_DISCONNECTED)
		avctp_disconnected(dev);

	metadata_cleanup(control->mdata);
	g_free(control->mdata);
	g_free(control->ply_settings);
	g_free(control);
	dev->control = NULL;
}

void control_unregister(struct audio_device *dev)
{
	g_dbus_unregister_interface(dev->conn, dev->path,
		AUDIO_CONTROL_INTERFACE);
}

void control_update(struct audio_device *dev, uint16_t uuid16)
{
	struct control *control = dev->control;

	if (uuid16 == AV_REMOTE_TARGET_SVCLASS_ID)
		control->target = TRUE;
}

void control_suspend(struct audio_device *dev)
{
	struct control *control = dev->control;
	if (!control) {
		if (dev->uinput < 0)
			dev->uinput	= uinput_create("AVRCP");

		if (dev->uinput >= 0) {
			DBG("sending key event for suspend");
			send_key(dev->uinput, KEY_PAUSECD, 1);
			send_key(dev->uinput, KEY_PAUSECD, 0);
		}
	} else {
		handle_key_op(control, PAUSE_OP, 1);
		handle_key_op(control, PAUSE_OP, 0);
	}
}

void control_resume(struct audio_device *dev)
{
	struct control *control = dev->control;
	if (!control) {
		if (dev->uinput < 0)
			dev->uinput	= uinput_create("AVRCP");

		if (dev->uinput >= 0) {
			send_key(dev->uinput, KEY_PLAYCD, 1);
			send_key(dev->uinput, KEY_PLAYCD, 0);
		}
	} else {
		handle_key_op(control, PLAY_OP, 1);
		handle_key_op(control, PLAY_OP, 0);
	}
}
static void init_player_settings(struct control *control)
{
	struct player_settings *ply_settings = control->ply_settings;
	memset(ply_settings, 0, sizeof(struct player_settings));
	ply_settings->local_shuffle_value = 0x1;
	ply_settings->local_repeat_value = 0x1;
	ply_settings->supported_attribs = 2;
}

struct control *control_init(struct audio_device *dev, uint16_t uuid16)
{
	struct control *control;
	struct meta_data *mdata;

	if (!g_dbus_register_interface(dev->conn, dev->path,
					AUDIO_CONTROL_INTERFACE,
					control_methods, control_signals, NULL,
					dev, path_unregister))
		return NULL;

	DBG("Registered interface %s on path %s",
		AUDIO_CONTROL_INTERFACE, dev->path);

	control = g_new0(struct control, 1);
	control->dev = dev;
	control->state = AVCTP_STATE_DISCONNECTED;
	control->uinput = -1;

	if (uuid16 == AV_REMOTE_TARGET_SVCLASS_ID)
		control->target = TRUE;

	mdata = g_new0(struct meta_data, 1);
	if (!mdata) {
		DBG("No Memory available for meta data");
		return NULL;
	}
	mdata->title = g_new0(gchar, METADATA_MAX_STRING_LEN);
	mdata->artist = g_new0(gchar, METADATA_MAX_STRING_LEN);
	mdata->album = g_new0(gchar, METADATA_MAX_STRING_LEN);
	mdata->media_number = g_new0(gchar, METADATA_MAX_NUMBER_LEN);
	mdata->total_media_count = g_new0(gchar, METADATA_MAX_NUMBER_LEN);
	mdata->playing_time = g_new0(gchar, METADATA_MAX_NUMBER_LEN);
	mdata->genre = g_new0(gchar, METADATA_MAX_STRING_LEN);

	if (!(mdata->title) || !(mdata->artist) || !(mdata->album) ||
			!(mdata->media_number) || !(mdata->total_media_count) ||
			!(mdata->playing_time) || !(mdata->genre)) {
		DBG("No Memory available for meta data");
		metadata_cleanup(mdata);
		g_free(mdata);
		return NULL;
	}

	strcpy(mdata->title,DEFAULT_METADATA_STRING);
	strcpy(mdata->artist,DEFAULT_METADATA_STRING);
	strcpy(mdata->album,DEFAULT_METADATA_STRING);
	strcpy(mdata->media_number,DEFAULT_METADATA_NUMBER);
	strcpy(mdata->total_media_count,DEFAULT_METADATA_NUMBER);
	strcpy(mdata->playing_time,DEFAULT_METADATA_NUMBER);
	strcpy(mdata->genre, DEFAULT_METADATA_STRING);
	mdata->remaining_mdata = NULL;
	mdata->remaining_mdata_len = 0;
	mdata->trans_id_event_track = 0;
	mdata->trans_id_event_playback = 0;
	mdata->trans_id_event_playback_pos = 0;
	mdata->trans_id_event_addressed_player = 0;
	mdata->trans_id_event_available_palyer = 0;
	mdata->trans_id_get_play_status = 0;
	mdata->reg_track_changed = FALSE;
	mdata->reg_playback_status = FALSE;
	mdata->reg_playback_pos = FALSE;
	mdata->reg_addressed_player = FALSE;
	mdata->reg_available_palyer = FALSE;
	mdata->req_get_play_status = FALSE;
	mdata->req_get_play_pos = FALSE;
	mdata->current_play_status = STATUS_STOPPED;
	mdata->current_position = 0xffffffff;

	control->mdata = mdata;
	control->ply_settings = g_new0(struct player_settings, 1);
	init_player_settings(control);

	return control;
}

gboolean control_is_active(struct audio_device *dev)
{
	struct control *control = dev->control;

	if (control && control->state != AVCTP_STATE_DISCONNECTED)
		return TRUE;

	return FALSE;
}

unsigned int avctp_add_state_cb(avctp_state_cb cb, void *user_data)
{
	struct avctp_state_callback *state_cb;
	static unsigned int id = 0;

	state_cb = g_new(struct avctp_state_callback, 1);
	state_cb->cb = cb;
	state_cb->user_data = user_data;
	state_cb->id = ++id;

	avctp_callbacks = g_slist_append(avctp_callbacks, state_cb);

	return state_cb->id;
}

gboolean avctp_remove_state_cb(unsigned int id)
{
	GSList *l;

	for (l = avctp_callbacks; l != NULL; l = l->next) {
		struct avctp_state_callback *cb = l->data;
		if (cb && cb->id == id) {
			avctp_callbacks = g_slist_remove(avctp_callbacks, cb);
			g_free(cb);
			return TRUE;
		}
	}

	return FALSE;
}

static int send_meta_data_continue_response(struct control *control,
						uint8_t trans_id)
{
	struct meta_data *mdata = control->mdata;
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	int meta_data_len = mdata->remaining_mdata_len + header_len - 1;
	unsigned char buf[meta_data_len];
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	struct avrcp_params *params = (struct avrcp_params *)(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	struct meta_data_field *mdata_field= (struct meta_data_field *)(&buf[header_len]);
	int len = 0, total_len =0, sk = g_io_channel_unix_get_fd(control->io);
	uint8_t *op = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];

	memset(buf, 0, sizeof(buf));

	avctp->transaction = trans_id;
	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_RESPONSE;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);

	avrcp->code = CTYPE_STABLE;
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_VENDORDEPENDENT;

	//BT SIG Company id is 0x1958
	op[0] = 0x00;
	op[1] = 0x19;
	op[2] = 0x58;
	params->pdu_id = PDU_GET_ELEMENT_ATTRIBUTES;
	params->packet_type = AVCTP_PACKET_END;

	op = (uint8_t *)params;
	op += AVRCP_PKT_PARAMS_LEN;
	op--;
	memcpy(op, mdata->remaining_mdata, mdata->remaining_mdata_len);
	g_free(mdata->remaining_mdata);
	mdata->remaining_mdata = NULL;
	mdata->remaining_mdata_len = 0;

	if ((meta_data_len)  > AVRCP_MAX_PKT_SIZE) {
		len = AVRCP_MAX_PKT_SIZE - AVRCP_HEADER_LENGTH -
					AVRCP_PKT_PARAMS_LEN -1;
		len += 1;
		params->param_len = htons(len);
		total_len = AVRCP_MAX_PKT_SIZE + AVCTP_HEADER_LENGTH;
		params->packet_type = AVCTP_PACKET_CONTINUE;
		meta_data_len -= len;
		mdata->remaining_mdata = g_new0(gchar, meta_data_len);
		if (!(mdata->remaining_mdata)) {
			return -ENOMEM;
		}
		mdata->remaining_mdata_len = meta_data_len;
		len = AVRCP_MAX_PKT_SIZE + AVCTP_HEADER_LENGTH;
		memcpy(mdata->remaining_mdata, &buf[len], meta_data_len);
	} else {
		params->param_len = htons(meta_data_len - header_len + 1);
		total_len = meta_data_len;
	}

	return write(sk, buf, total_len);
}

static int send_meta_data(struct control *control, uint8_t trans_id,
				uint8_t att_mask, uint8_t att_count)
{
	struct meta_data *mdata = control->mdata;
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	int meta_data_len = strlen(mdata->title) + strlen(mdata->artist) +
		strlen(mdata->album) + strlen(mdata->media_number) +
		strlen(mdata->total_media_count) + strlen(mdata->playing_time) +
		strlen(mdata->genre) + (METADATA_FIELD_LEN*att_count) + header_len;
	unsigned char *buf = g_new0(unsigned char, meta_data_len);
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	struct avrcp_params *params = (struct avrcp_params *)(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	struct meta_data_field *mdata_field = (struct meta_data_field *)(&buf[header_len]);
	int len = 0, total_len =0, sk = g_io_channel_unix_get_fd(control->io), ret = 0;
	uint8_t *op = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];

	memset(buf, 0, sizeof(buf));

	avctp->transaction = trans_id;
	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_RESPONSE;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);

	avrcp->code = CTYPE_STABLE;
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_VENDORDEPENDENT;

	//BT SIG Company id is 0x1958
	op[0] = 0x00;
	op[1] = 0x19;
	op[2] = 0x58;
	params->pdu_id = PDU_GET_ELEMENT_ATTRIBUTES;
	params->packet_type = AVCTP_PACKET_SINGLE;
	meta_data_len = METADATA_FIELD_LEN * att_count;
	params->capability_id = att_count;
	DBG("Att mask is %d", att_mask);

	if (att_mask & (1 << (METADATA_TITLE - 1))) {
		mdata_field->att_id = htonl(METADATA_TITLE);
		mdata_field->char_set_id = htons(CHARACTER_SET_UTF8);
		len = strlen(mdata->title);
		mdata_field->att_len = htons(len);
		strncpy(mdata_field->val, mdata->title, len);
		meta_data_len += len;
		DBG("METADATA_TITLE %d %d", len, meta_data_len);
	}

	if (att_mask & (1 << (METADATA_ARTIST - 1))) {
		op = (uint8_t *)mdata_field;
		if (len > 0) {
			op += METADATA_FIELD_LEN;
			op += len;
		}
		mdata_field = (struct meta_data_field *)op;
		mdata_field->att_id = htonl(METADATA_ARTIST);
		mdata_field->char_set_id = htons(CHARACTER_SET_UTF8);
		len = strlen(mdata->artist);
		mdata_field->att_len = htons(len);
		strncpy(mdata_field->val, mdata->artist, len);
		meta_data_len += len;
	}

	if (att_mask & (1 << (METADATA_ALBUM - 1))) {
		op = (uint8_t *)mdata_field;
		if (len > 0) {
			op += METADATA_FIELD_LEN;
			op += len;
		}
		mdata_field = (struct meta_data_field *)op;
		mdata_field->att_id = htonl(METADATA_ALBUM);
		mdata_field->char_set_id = htons(CHARACTER_SET_UTF8);
		len = strlen(mdata->album);
		mdata_field->att_len = htons(len);
		strncpy(mdata_field->val, mdata->album, len);
		meta_data_len += len;
	}

	if (att_mask & (1 << (METADATA_MEDIA_NUMBER - 1))) {
		op = (uint8_t *)mdata_field;
		if (len > 0) {
			op += METADATA_FIELD_LEN;
			op += len;
		}
		mdata_field = (struct meta_data_field *)op;
		mdata_field->att_id = htonl(METADATA_MEDIA_NUMBER);
		mdata_field->char_set_id = htons(CHARACTER_SET_UTF8);
		len = strlen(mdata->media_number);
		mdata_field->att_len = htons(len);
		strncpy(mdata_field->val, mdata->media_number, len);
		meta_data_len += len;
	}

	if (att_mask & (1 << (METADATA_TOTAL_MEDIA - 1))) {
		op = (uint8_t *)mdata_field;
		if (len > 0) {
			op += METADATA_FIELD_LEN;
			op += len;
		}
		mdata_field = (struct meta_data_field *)op;
		mdata_field->att_id = htonl(METADATA_TOTAL_MEDIA);
		mdata_field->char_set_id = htons(CHARACTER_SET_UTF8);
		len = strlen(mdata->total_media_count);
		mdata_field->att_len = htons(len);
		strncpy(mdata_field->val, mdata->total_media_count, len);
		meta_data_len += len;
	}

	if (att_mask & (1 << (METADATA_PLAYING_TIME - 1))) {
		op = (uint8_t *)mdata_field;
		if (len > 0) {
			op += METADATA_FIELD_LEN;
			op += len;
		}
		mdata_field = (struct meta_data_field *)op;
		mdata_field->att_id = htonl(METADATA_PLAYING_TIME);
		mdata_field->char_set_id = htons(CHARACTER_SET_UTF8);
		len = strlen(mdata->playing_time);
		mdata_field->att_len = htons(len);
		strncpy(mdata_field->val, mdata->playing_time, len);
		meta_data_len += len;
	}

	if (att_mask & (1 << (METADATA_GENRE - 1))) {
		op = (uint8_t *)mdata_field;
		if (len > 0) {
			op += METADATA_FIELD_LEN;
			op += len;
		}
		mdata_field = (struct meta_data_field *)op;
		mdata_field->att_id = htonl(METADATA_GENRE);
		mdata_field->char_set_id = htons(CHARACTER_SET_UTF8);
		len = strlen(mdata->genre);
		mdata_field->att_len = htons(len);
		strncpy(mdata_field->val, mdata->genre, len);
		meta_data_len += len;
		DBG("METADATA_GENRE %d %d", len, meta_data_len);
	}

	if ((meta_data_len + header_len - AVCTP_HEADER_LENGTH)
							> AVRCP_MAX_PKT_SIZE) {
		DBG("meta len is %d header len is %d", meta_data_len, header_len);
		len = AVRCP_MAX_PKT_SIZE - AVRCP_HEADER_LENGTH -
					AVRCP_PKT_PARAMS_LEN;
		len += 1;
		params->param_len = htons(len);
		total_len = AVRCP_MAX_PKT_SIZE + AVCTP_HEADER_LENGTH;
		params->packet_type = AVCTP_PACKET_START;
		meta_data_len = meta_data_len - len + 1;
		mdata->remaining_mdata = g_new0(gchar, meta_data_len);
		if (!(mdata->remaining_mdata)) {
			g_free(buf);
			return -ENOMEM;
		}
		mdata->remaining_mdata_len = meta_data_len;
		DBG("Remain meta data len is %d", mdata->remaining_mdata_len);
		len = AVRCP_MAX_PKT_SIZE + AVCTP_HEADER_LENGTH;
		memcpy(mdata->remaining_mdata, &buf[len], meta_data_len);
	} else {
		// 1 byte for NumberOfParams, this already part of header_len
		params->param_len = htons(meta_data_len+1);
		total_len = meta_data_len + header_len;
	}
	ret = write(sk, buf, total_len);
	g_free(buf);
	return ret;
}

static int send_notification(struct control *control,
		uint16_t event_id, uint16_t event_data)
{
	struct meta_data *mdata = control->mdata;
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	unsigned char buf[header_len + 8];
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	struct avrcp_params *params = (struct avrcp_params *)(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	int len = 0, total_len = 0, sk = g_io_channel_unix_get_fd(control->io);
	uint8_t *op = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];
	uint32_t *tid;

	memset(buf, 0, sizeof(buf));

	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_RESPONSE;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);

	avrcp->code = CTYPE_CHANGED;
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_VENDORDEPENDENT;

	//BT SIG Company id is 0x1958
	op[0] = 0x00;
	op[1] = 0x19;
	op[2] = 0x58;
	params->pdu_id = PDU_RGR_NOTIFICATION_ID;
	params->packet_type = AVCTP_PACKET_SINGLE;
	params->capability_id = event_id;
	op = (uint8_t *)params;
	op += AVRCP_PKT_PARAMS_LEN;
	tid = (uint32_t *)op;
	switch(event_id) {
		case EVENT_TRACK_CHANGED:
			if (mdata->reg_playback_pos == TRUE)
				send_playback_pos_request(control);

			if (mdata->reg_track_changed == FALSE)
				return 0;
			*tid = htonl(0x00);
			tid++;
			*tid = htonl(event_data);
			avctp->transaction = mdata->trans_id_event_track;
			mdata->reg_track_changed = FALSE;
			params->param_len = htons(0x9);
			total_len = 22;
			break;
		case EVENT_PLAYBACK_STATUS_CHANGED:
			mdata->current_play_status = (uint8_t) event_data;
			if (mdata->reg_playback_pos == TRUE)
				send_playback_pos_request(control);

			if (mdata->reg_playback_status == FALSE)
				return 0;
			*op = event_data;
			avctp->transaction = mdata->trans_id_event_playback;
			mdata->reg_playback_status = FALSE;
			params->param_len = htons(0x2);
			total_len = 15;
			break;
		case EVENT_ADDRESSED_PLAYER_CHANGED:
			if (mdata->reg_addressed_player == FALSE)
				return 0;
			*op = 0x0; // Player Id
			op++;
			*op = event_data;
			op++;
			*op = 0x00; // UID Counter
			op++;
			*op = 0x00;
			avctp->transaction = mdata->trans_id_event_addressed_player;
			mdata->reg_addressed_player = FALSE;
			params->param_len = htons(0x5);
			total_len = 18;
			break;
		case EVENT_AVAILABLE_PLAYERS_CHANGED:
			if (mdata->reg_available_palyer == FALSE)
				return 0;
			avctp->transaction = mdata->trans_id_event_available_palyer;
			mdata->reg_available_palyer = FALSE;
			params->param_len = htons(0x1);
			total_len = 14;
			break;
	}
	DBG("Send Notification totallen %d", total_len);
	return write(sk, buf, total_len);
}

static int send_play_status(struct control *control, uint32_t song_len,
			uint32_t song_position, uint8_t play_status)
{
	struct meta_data *mdata = control->mdata;
	int header_len = AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH + AVRCP_PKT_PARAMS_LEN;
	unsigned char buf[header_len + 8];
	struct avctp_header *avctp = (void *) buf;
	struct avrcp_header *avrcp = (void *) &buf[AVCTP_HEADER_LENGTH];
	struct avrcp_params *params = (struct avrcp_params *)(&buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH]);
	int len = 0, total_len =0, sk = g_io_channel_unix_get_fd(control->io);
	uint8_t *op = &buf[AVCTP_HEADER_LENGTH + AVRCP_HEADER_LENGTH];
	uint32_t *tid;

	memset(buf, 0, sizeof(buf));
	DBG("send_play_status called");

	if (mdata->req_get_play_status == FALSE)
		return 0;

	DBG("send_play_status executing");
	mdata->req_get_play_status = FALSE;
	avctp->packet_type = AVCTP_PACKET_SINGLE;
	avctp->cr = AVCTP_RESPONSE;
	avctp->pid = htons(AV_REMOTE_SVCLASS_ID);
	avctp->transaction = mdata->trans_id_get_play_status;

	avrcp->code = CTYPE_STABLE;
	avrcp->subunit_type = SUBUNIT_PANEL;
	avrcp->opcode = OP_VENDORDEPENDENT;

	//BT SIG Company id is 0x1958
	op[0] = 0x00;
	op[1] = 0x19;
	op[2] = 0x58;
	params->pdu_id = PDU_GET_PLAY_STATUS_ID;
	params->packet_type = AVCTP_PACKET_SINGLE;
	params->param_len = htons(0x9);
	op = (uint8_t *)params;
	op += AVRCP_PKT_PARAMS_LEN;
	op -= 1;
	tid = (uint32_t *)op;
	*tid = htonl(song_len);
	tid++;
	*tid = htonl(song_position);
	tid++;
	op = (uint8_t *)tid;
	*op = play_status;
	total_len = 22;
	return write(sk, buf, total_len);
}

