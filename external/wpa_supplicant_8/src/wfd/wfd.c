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


int wfd_add_peer_info(void *msg_ctx, struct wfd_peer_info *wfd_info,
					  const u8 *ies, size_t ies_len)
{
	struct wfd_message wfd_msg;
	u16 device_info;

	os_memset(&wfd_msg, 0, sizeof(wfd_msg));
	if (wfd_parse_ies(ies, ies_len, &wfd_msg)) {
		wpa_msg(msg_ctx, MSG_DEBUG,
				"WFD: Failed to parse WFD IE for a device entry");
		wfd_parse_free(&wfd_msg);
		return -1;
	}

	wfd_info->wfd_supported = (wfd_msg.wfd_attributes != NULL);
	if (!wfd_info->wfd_supported) {
		wpa_msg(msg_ctx, MSG_DEBUG,
				"WFD: No WFD IE found, device does not support WFD");
		wfd_parse_free(&wfd_msg);
		return 0;
	}

	if (!wfd_msg.device_info) {
		wpa_msg(msg_ctx, MSG_DEBUG,
				"WFD: No device info field in WFD Device Information Subelement");
		wfd_parse_free(&wfd_msg);
		return -1;
	}
	device_info = WPA_GET_BE16(wfd_msg.device_info);

	switch (device_info & WFD_DEVICE_INFO_DEVICE_TYPE) {
	case WFD_DEVICE_INFO_SOURCE:
		wfd_info->device_type = WFD_SOURCE;
	break;
	case WFD_DEVICE_INFO_PRIMARY_SINK:
		wfd_info->device_type = WFD_PRIMARY_SINK;
	break;
	case WFD_DEVICE_INFO_SECONDARY_SINK:
		wfd_info->device_type = WFD_SECONDARY_SINK;
	break;
	case WFD_DEVICE_INFO_SOURCE_PRIMARY_SINK:
		wfd_info->device_type = WFD_SOURCE_PRIMARY_SINK;
	}

	switch (device_info & WFD_DEVICE_INFO_AVAILABLE_FOR_SESSION) {
	case WFD_DEVICE_INFO_NOT_AVAILABLE:
		wfd_info->available_for_session = 0;
	break;
	case WFD_DEVICE_INFO_AVAILABLE:
		wfd_info->available_for_session = 1;
	break;
	default:
		wpa_msg(msg_ctx, MSG_DEBUG,
		"WFD: invalid Available for Session field in Device Info Subelement");
		wfd_parse_free(&wfd_msg);
	return -1;
	}

	switch (device_info & WFD_DEVICE_INFO_PREFERRED_CONNECTIVITY) {
	case WFD_DEVICE_INFO_P2P:
		wfd_info->preferred_connectivity = WFD_P2P;
	break;
	case WFD_DEVICE_INFO_TDLS:
		wfd_info->preferred_connectivity = WFD_TDLS;
	}

	wfd_info->coupled_sink_supported_by_source =
		(device_info &
			WFD_DEVICE_INFO_COUPLED_SINK_SUPPORTED_BY_SOURCE) != 0;
	wfd_info->coupled_sink_supported_by_sink =
		(device_info &
			WFD_DEVICE_INFO_COUPLED_SINK_SUPPORTED_BY_SINK) != 0;
	wfd_info->service_discovery_supported =
		(device_info &
			WFD_DEVICE_INFO_SERVICE_DISCOVERY_SUPPORTED) != 0;
	wfd_info->content_protection_supported =
		(device_info &
			WFD_DEVICE_INFO_CONTENT_PROTECTION_SUPPORTED) != 0;
	wfd_info->time_sync_supported =
		(device_info & WFD_DEVICE_INFO_TIME_SYNC_SUPPORTED) != 0;

	if (!wfd_msg.session_mgmt_ctrl_port) {
		wpa_msg(msg_ctx, MSG_DEBUG,
				"WFD: No session mgmt ctrl port field"
				"in WFD Device Information Subelement");
		wfd_parse_free(&wfd_msg);
		return -1;
	}
	wfd_info->session_mgmt_ctrl_port =
			WPA_GET_BE16(wfd_msg.session_mgmt_ctrl_port);

	if (!wfd_msg.device_max_throughput) {
		wpa_msg(msg_ctx, MSG_DEBUG,
				"WFD: No device max throughput field in"
				"WFD Device Information Subelement");
		wfd_parse_free(&wfd_msg);
		return -1;
	}
	wfd_info->device_max_throughput =
			WPA_GET_BE16(wfd_msg.device_max_throughput);

	if (!wfd_msg.associated_bssid)
		wfd_info->is_associated_with_ap = 0;
	else {
		wfd_info->is_associated_with_ap = 1;
		os_memcpy(wfd_info->associated_bssid,
				wfd_msg.associated_bssid, ETH_ALEN);
	}

	wfd_parse_free(&wfd_msg);
	return 0;
}


const char *wfd_device_type_text(enum wfd_device_type device_type)
{
	switch (device_type) {
	case WFD_SOURCE:
		return "source";
	case WFD_PRIMARY_SINK:
		return "primary_sink";
	case WFD_SECONDARY_SINK:
		return "secondary_sink";
	case WFD_SOURCE_PRIMARY_SINK:
		return "source_primary_sink";
	default:
		return "";
	}
}


const char *wfd_preferred_connectivity_text(
	enum wfd_preferred_connectivity_type preferred_connectivity)
{
	switch (preferred_connectivity) {
	case WFD_P2P:
		return "p2p";
	case WFD_TDLS:
		return "tdls";
	default:
		return "";
	}
}


int wfd_scan_result_text(const u8 *ies, size_t ies_len, char *buf, char *end)
{
	struct wpabuf *wfd_ie;
	int ret;

	wfd_ie = ieee802_11_vendor_ie_concat(ies, ies_len, WFD_IE_VENDOR_TYPE);
	if (wfd_ie == NULL)
		return 0;

	ret = wfd_attr_text(wfd_ie, buf, end);
	wpabuf_free(wfd_ie);
	return ret;
}


struct wfd_data *wfd_init(const struct wfd_config *cfg)
{
	struct wfd_data *wfd;

	wfd = os_zalloc(sizeof(*wfd) + sizeof(*cfg));
	if (wfd == NULL)
		return NULL;
	wfd->cfg = (struct wfd_config *) (wfd + 1);
	os_memcpy(wfd->cfg, cfg, sizeof(*cfg));
	return wfd;
}


void wfd_deinit(struct wfd_data *wfd)
{
	os_free(wfd);
}


void wfd_add_wfd_ie(void *ctx, struct wfd_data *wfd, struct wpabuf *ies)
{
	u8 *len;
	u8 associated_bssid[6];

	if (wfd->cfg->enabled) {
		len = wfd_buf_add_ie_hdr(ies);
		wfd_buf_add_device_info(ies, wfd);
		if (!wpas_wfd_get_bssid(ctx, associated_bssid))
			if (!is_zero_ether_addr(associated_bssid))
				wfd_buf_add_associated_bssid(ies,
						associated_bssid);
		wfd_buf_update_ie_hdr(ies, len);
	}
}


struct wfd_config *wfd_get_config(struct wfd_data *wfd)
{
	return wfd->cfg;
}
