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

#ifndef WFD_SUPPLICANT_H
#define WFD_SUPPLICANT_H

#include "wpa_supplicant_i.h"
#include "wfd/wfd.h"

#define DEFAULT_WFD_ENABLED                            0
#define DEFAULT_DEVICE_TYPE                            WFD_SOURCE
#define WFD_DEFAULT_COUPLED_SINK_SUPPORTED_BY_SOURCE   0
#define WFD_DEFAULT_COUPLED_SINK_SUPPORTED_BY_SINK     0
#define WFD_DEFAULT_AVAILABLE_FOR_SESSION              1
#define WFD_DEFAULT_SERVICE_DISCOVERY_SUPPORTED        0
#define WFD_DEFAULT_PREFERRED_CONNECTIVITY             WFD_P2P
#define WFD_DEFAULT_CONTENT_PROTECTION_SUPPORTED       0
#define WFD_DEFAULT_TIME_SYNC_SUPPORTED                0
#define WFD_DEFAULT_PRIMARYSINK_AUDIO_NOTSUPPORTED     0
#define WFD_DEFAULT_SOURCE_AUDIO_ONLY_SUPPORTED        0
#define WFD_DEFAULT_TDLS_PERSISTENT_GROUP_INTENTED     0
#define WFD_DEFAULT_TDLS_PERSISTENT_GROUP_REINVOKE     0
#define WFD_DEFAULT_SESSION_MGMT_CTRL_PORT             554
#define WFD_DEFAULT_MAX_THROUGHPUT                     10

int wpas_wfd_y_n_str2bin(const char *str, int *result);
int wpas_wfd_device_type_str2bin(const char *str, enum wfd_device_type *result);
int wpas_wfd_preferred_connectivity_str2bin(
	const char *str, enum wfd_preferred_connectivity_type *result);
int wpas_wfd_init(struct wpa_global *global, struct wpa_supplicant *wpa_s);
void wpas_wfd_deinit_global(struct wpa_global *global);
int wpas_wfd_get_bssid(struct wpa_supplicant *wpa_s, u8 *bssid);

#endif /* WFD_SUPPLICANT_H */
