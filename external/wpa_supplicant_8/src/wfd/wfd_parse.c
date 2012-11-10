/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include "common.h"
#include "common/ieee802_11_common.h"
#include "common/ieee802_11_defs.h"
#include "wfd_i.h"


static int wfd_parse_attribute(u8 id, const u8 *data, u8 len,
					struct wfd_message *msg)
{
	const u8 *pos;

	switch (id) {
	case WFD_ATTR_DEVICE_INFO:
		if (len < 6) {
			wpa_printf(MSG_DEBUG,
				"WFD: Too short Device Information "
				"attribute (length %d)", len);
			return -1;
		}
		pos = data;
		msg->device_info = pos;
		pos += 2;
		msg->session_mgmt_ctrl_port = pos;
		pos += 2;
		msg->device_max_throughput = pos;
		wpa_printf(MSG_DEBUG,
		    "WFD: * Device Information: device information 0x%02x%02x",
			msg->device_info[0], msg->device_info[1]);
		wpa_printf(MSG_DEBUG,
				"   session management control port 0x%02x%02x"
				"   device maximum throughput 0x%02x%02x",
					msg->session_mgmt_ctrl_port[0],
					msg->session_mgmt_ctrl_port[1],
					msg->device_max_throughput[0],
					msg->device_max_throughput[1]);
		break;
	case WFD_ATTR_ASSOC_BSSID:
		if (len < ETH_ALEN) {
			wpa_printf(MSG_DEBUG, "WFD: Too short Associated BSSID "
					"attribute (length %d)", len);
			return -1;
		}
		msg->associated_bssid = data;
		wpa_printf(MSG_DEBUG, "WFD: * Associated BSSID " MACSTR,
				MAC2STR(msg->associated_bssid));
		break;
	default:
		wpa_printf(MSG_DEBUG, "WFD: Skipped unknown attribute %d "
				"(length %d)", id, len);
		break;
	}

	return 0;
}


/**
 * wfd_parse_wfd_ie - Parse WFD IE
 * @buf: Concatenated WFD IE(s) payload
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller is responsible for clearing the msg data structure before
 * calling this function.
 */
int wfd_parse_wfd_ie(const struct wpabuf *buf, struct wfd_message *msg)
{
	const u8 *pos = wpabuf_head_u8(buf);
	const u8 *end = pos + wpabuf_len(buf);

	wpa_printf(MSG_DEBUG, "WFD: Parsing WFD IE");

	while (pos < end) {
		u16 attr_len;
		if (pos + 1 >= end) {
			wpa_printf(MSG_DEBUG, "WFD: Invalid WFD attribute");
			return -1;
		}
		attr_len = WPA_GET_BE16(pos + 1);
		wpa_printf(MSG_DEBUG, "WFD: Attribute %d length %u",
			   pos[0], attr_len);
		if (pos + 3 + attr_len > end) {
			wpa_printf(MSG_DEBUG, "WFD: Attribute underflow "
				   "(len=%u left=%d)",
				   attr_len, (int) (end - pos - 3));
			wpa_hexdump(MSG_MSGDUMP, "WFD: Data", pos, end - pos);
			return -1;
		}
		if (wfd_parse_attribute(pos[0], pos + 3, attr_len, msg))
			return -1;
		pos += 3 + attr_len;
	}

	return 0;
}


/**
 * wfd_parse_ies - Parse WFD message IEs (WFD IEs)
 * @data: IEs from the message
 * @len: Length of data buffer in octets
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller is responsible for clearing the msg data structure before
 * calling this function.
 *
 * Note: Caller must free temporary memory allocations by calling
 * wfd_parse_free() when the parsed data is not needed anymore.
 */
int wfd_parse_ies(const u8 *data, size_t len, struct wfd_message *msg)
{

	msg->wfd_attributes = ieee802_11_vendor_ie_concat(data, len,
							WFD_IE_VENDOR_TYPE);
	if (msg->wfd_attributes &&
		wfd_parse_wfd_ie(msg->wfd_attributes, msg)) {
		wpa_printf(MSG_DEBUG, "WFD: Failed to parse WFD IE data");
		if (msg->wfd_attributes)
			wpa_hexdump_buf(MSG_MSGDUMP, "WFD: WFD IE data",
					msg->wfd_attributes);
		wfd_parse_free(msg);
		return -1;
	}

	return 0;
}


/**
 * wfd_parse_free - Free temporary data from WFD parsing
 * @msg: Parsed attributes
 */
void wfd_parse_free(struct wfd_message *msg)
{
	wpabuf_free(msg->wfd_attributes);
	msg->wfd_attributes = NULL;
}


/**
 * wfd_attr_text - Build text format description of WFD IE
 * attributes
 * @data: WFD IE contents
 * @buf: Buffer for returning text
 * @end: Pointer to the end of the buf area
 * Returns: Number of octets written to the buffer or -1 on faikure
 *
 * This function can be used to parse WFD IE contents into text
 * format field=value lines.
 */
int wfd_attr_text(struct wpabuf *data, char *buf, char *end)
{
	struct wfd_message msg;
	char *pos = buf;
	int ret;

	os_memset(&msg, 0, sizeof(msg));
	if (wfd_parse_wfd_ie(data, &msg))
		return -1;

	if (msg.device_info) {
		ret = os_snprintf(pos, end - pos,
				"wfd_device_info=0x%02x%02x\n",
				msg.device_info[0], msg.device_info[1]);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (msg.session_mgmt_ctrl_port) {
		ret = os_snprintf(pos, end - pos,
			"wfd_session_management_control_port=0x%02x%02x\n",
				 msg.session_mgmt_ctrl_port[0],
				msg.session_mgmt_ctrl_port[1]);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (msg.device_max_throughput) {
		ret = os_snprintf(pos, end - pos,
				"wfd_device_maximum_throughput=0x%02x%02x\n",
				msg.device_max_throughput[0],
				msg.device_max_throughput[1]);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (msg.associated_bssid) {
		ret = os_snprintf(pos, end - pos,
				"wfd_associated_bssid=" MACSTR "\n",
					MAC2STR(msg.associated_bssid));
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}

