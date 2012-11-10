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

#ifndef WFD_I_H
#define WFD_I_H

#include "wfd.h"

extern int wpas_wfd_get_bssid(void *ctx, u8 *bssid);

/**
 * struct wfd_data - WFD module data (internal to WFD module)
 */
struct wfd_data {
	/**
	 * cfg - WFD module configuration
	 *
	 * This is included in the same memory allocation with the
	 * struct wfd_data and as such, must not be freed separately.
	 */
	struct wfd_config *cfg;
};

/**
 * struct wfd_message - Parsed WFD message (or WFD IE)
 */
struct wfd_message {
	struct wpabuf *wfd_attributes;
	const u8 *device_info;
	const u8 *session_mgmt_ctrl_port;
	const u8 *device_max_throughput;
	const u8 *associated_bssid;
};

/* wfd_parse.c */
int wfd_parse_wfd_ie(const struct wpabuf *buf, struct wfd_message *msg);
int wfd_parse_ies(const u8 *data, size_t len, struct wfd_message *msg);
void wfd_parse_free(struct wfd_message *msg);
int wfd_attr_text(struct wpabuf *data, char *buf, char *end);

/* wfd_build.c */
u8 *wfd_buf_add_ie_hdr(struct wpabuf *buf);
void wfd_buf_update_ie_hdr(struct wpabuf *buf, u8 *len);
void wfd_buf_add_device_info(struct wpabuf *buf, struct wfd_data *wfd);
void wfd_buf_add_associated_bssid(struct wpabuf *buf,
					const u8 *associated_bssid);

#endif /* WFD_I_H */
