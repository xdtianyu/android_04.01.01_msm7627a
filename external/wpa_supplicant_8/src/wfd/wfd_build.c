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
#include "common/ieee802_11_defs.h"
#include "wfd_i.h"


u8 *wfd_buf_add_ie_hdr(struct wpabuf *buf)
{
	u8 *len;

	/* WFD IE header */
	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	len = wpabuf_put(buf, 1); /* IE length to be filled */
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, WFD_OUI_TYPE);
	wpa_printf(MSG_DEBUG, "WFD: * WFD IE header");
	return len;
}


void wfd_buf_update_ie_hdr(struct wpabuf *buf, u8 *len)
{
	/* Update WFD IE length */
	*len = (u8 *)wpabuf_put(buf, 0) - len - 1;
}


void wfd_buf_add_device_info(struct wpabuf *buf, struct wfd_data *wfd)
{
	u8 *len;
	u16 device_info;

	/* WFD Device Information */
	wpabuf_put_u8(buf, WFD_ATTR_DEVICE_INFO);
	len = wpabuf_put(buf, 2); /* IE length to be filled */

	/* Device Information */
	device_info = 0;

	switch (wfd->cfg->device_type) {
	case WFD_SOURCE:
		device_info |= WFD_DEVICE_INFO_SOURCE;
		break;
	case WFD_PRIMARY_SINK:
		device_info |= WFD_DEVICE_INFO_PRIMARY_SINK;
		break;
	case WFD_SECONDARY_SINK:
		device_info |= WFD_DEVICE_INFO_SECONDARY_SINK;
		break;
	case WFD_SOURCE_PRIMARY_SINK:
		device_info |= WFD_DEVICE_INFO_SOURCE_PRIMARY_SINK;
	}

	if (wfd->cfg->available_for_session)
		device_info |= WFD_DEVICE_INFO_AVAILABLE;
	else
		device_info |= WFD_DEVICE_INFO_NOT_AVAILABLE;

	switch (wfd->cfg->preferred_connectivity) {
	case WFD_P2P:
		device_info |= WFD_DEVICE_INFO_P2P;
		break;
	case WFD_TDLS:
		device_info |= WFD_DEVICE_INFO_TDLS;
	}

	if (wfd->cfg->coupled_sink_supported_by_source)
		device_info |= WFD_DEVICE_INFO_COUPLED_SINK_SUPPORTED_BY_SOURCE;
	if (wfd->cfg->coupled_sink_supported_by_sink)
		device_info |= WFD_DEVICE_INFO_COUPLED_SINK_SUPPORTED_BY_SINK;
	if (wfd->cfg->service_discovery_supported)
		device_info |= WFD_DEVICE_INFO_SERVICE_DISCOVERY_SUPPORTED;
	if (wfd->cfg->content_protection_supported)
		device_info |= WFD_DEVICE_INFO_CONTENT_PROTECTION_SUPPORTED;
	if (wfd->cfg->time_sync_supported)
		device_info |= WFD_DEVICE_INFO_TIME_SYNC_SUPPORTED;
	if (wfd->cfg->primarysink_audio_notsupported)
		device_info |= WFD_DEVICE_INFO_AUDIO_UNSUPPORTED_AT_PRIMARY_SINK;
	if (wfd->cfg->source_audio_only_supported)
		device_info |= WFD_DEVICE_INFO_AUDIO_ONLY_SUPPORT_AT_SOURCE;
	if (wfd->cfg->tdls_persistent_group_intended)
		device_info |= WFD_DEVICE_INFO_TDLS_PERSISTENT_GROUP_INTENDED;
	if (wfd->cfg->tdls_persistent_group_reinvoke)
		device_info |= WFD_DEVICE_INFO_TDLS_PERSISTENT_GROUP_REINVOKE;

	wpabuf_put_be16(buf, device_info);

	/* Session Management Control Port and Device Maximum Throughput */
	wpabuf_put_be16(buf, wfd->cfg->session_mgmt_ctrl_port);
	wpabuf_put_be16(buf, wfd->cfg->device_max_throughput);

	/* Update attribute length */
	WPA_PUT_BE16(len, (u8 *)wpabuf_put(buf, 0) - len - 2);
	wpa_printf(MSG_DEBUG, "WFD: * Device Information");
}


void wfd_buf_add_associated_bssid(struct wpabuf *buf,
					const u8 *associated_bssid)
{
	/* WFD Associated BSSID */
	wpabuf_put_u8(buf, WFD_ATTR_ASSOC_BSSID);
	wpabuf_put_be16(buf, ETH_ALEN);
	wpabuf_put_data(buf, associated_bssid, ETH_ALEN);
	wpa_printf(MSG_DEBUG, "WFD: * Associated BSSID " MACSTR,
				MAC2STR(associated_bssid));
}

