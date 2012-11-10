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
#include "wfd_supplicant.h"
#include "config.h"
#include "p2p/p2p.h"
#include "driver_i.h"


/**
 * wpas_wfd_y_n_str2bin - Convert y/n string to binary value
 * @global: String
 * @result: Binary value
 * Returns: 0 on success, -1 on failure
 */
int wpas_wfd_y_n_str2bin(const char *str, int *result)
{
	if (os_strcmp(str, "y") == 0)
		*result = 1;
	else if (os_strcmp(str, "n") == 0)
		*result = 0;
	else
		return -1;
	return 0;
}


/**
 * wpas_wfd_device_type_str2bin - Convert WFD Device Type
 * string to binary value
 * @global: String
 * @result: Binary value
 * Returns: 0 on success, -1 on failure
 */
int wpas_wfd_device_type_str2bin(const char *str,
				 enum wfd_device_type *result)
{
	if (os_strcmp(str, "source") == 0)
		*result = WFD_SOURCE;
	else if (os_strcmp(str, "primary_sink") == 0)
		*result = WFD_PRIMARY_SINK;
	else if (os_strcmp(str, "secondary_sink") == 0)
		*result = WFD_SECONDARY_SINK;
	else if (os_strcmp(str, "source_primary_sink") == 0)
		*result = WFD_SOURCE_PRIMARY_SINK;
	else
		return -1;
	return 0;
}


/**
 * wpas_wfd_preferred_connectivity_str2bin - Convert
 * Preferred Connectivity string to binary value
 * @global: String
 * @result: Binary value
 * Returns: 0 on success, -1 on failure
 */
int wpas_wfd_preferred_connectivity_str2bin(
	const char *str, enum wfd_preferred_connectivity_type *result)
{
	if (os_strcmp(str, "p2p") == 0)
		*result = WFD_P2P;
	else if (os_strcmp(str, "tdls") == 0)
		*result = WFD_TDLS;
	else
		return -1;
	return 0;
}


int wpas_wfd_parse_config(struct wpa_config *conf, struct wfd_config *wfd)
{

	if (conf->wfd_enable &&
		wpas_wfd_y_n_str2bin(conf->wfd_enable, &wfd->enabled) < 0) {
		wpa_printf(MSG_ERROR, "WFD: Invalid wfd_enable value");
		return -1;
	}

	if (conf->wfd_device_type &&
		wpas_wfd_device_type_str2bin(conf->wfd_device_type,
						&wfd->device_type)) {
		wpa_printf(MSG_ERROR, "WFD: Invalid wfd_device_type value");
		return -1;
	}

	if (conf->wfd_coupled_sink_supported_by_source &&
		wpas_wfd_y_n_str2bin(conf->wfd_coupled_sink_supported_by_source,
				&wfd->coupled_sink_supported_by_source)) {
		wpa_printf(MSG_ERROR,
		 "WFD: Invalid wfd_coupled_sink_supported_by_source value");
		return -1;
	}

	if (conf->wfd_coupled_sink_supported_by_sink &&
		wpas_wfd_y_n_str2bin(conf->wfd_coupled_sink_supported_by_sink,
				&wfd->coupled_sink_supported_by_sink)) {
		wpa_printf(MSG_ERROR,
		 "WFD: Invalid wfd_coupled_sink_supported_by_sink value");
		return -1;
	}

	if (conf->wfd_available_for_session &&
		wpas_wfd_y_n_str2bin(conf->wfd_available_for_session,
					 &wfd->available_for_session)) {
		wpa_printf(MSG_ERROR,
			"WFD: Invalid wfd_available_for_session value");
		return -1;
	}

	if (conf->wfd_service_discovery_supported &&
		wpas_wfd_y_n_str2bin(conf->wfd_service_discovery_supported,
				 &wfd->service_discovery_supported)) {
		wpa_printf(MSG_ERROR,
		  "WFD: Invalid wfd_service_discovery_supported value");
		return -1;
	}

	if (conf->wfd_preferred_connectivity &&
		wpas_wfd_preferred_connectivity_str2bin(
				conf->wfd_preferred_connectivity,
				&wfd->preferred_connectivity)) {
		wpa_printf(MSG_ERROR,
			 "WFD: Invalid wfd_preferred_connectivity value");
		return -1;
	}

	if (conf->wfd_content_protection_supported &&
		wpas_wfd_y_n_str2bin(conf->wfd_content_protection_supported,
				 &wfd->content_protection_supported)) {
		wpa_printf(MSG_ERROR,
			"WFD: Invalid wfd_content_protection_supported value");
		return -1;
	}

	if (conf->wfd_time_sync_supported &&
		wpas_wfd_y_n_str2bin(conf->wfd_time_sync_supported,
				&wfd->time_sync_supported)) {
		wpa_printf(MSG_ERROR,
			"WFD: Invalid wfd_time_sync_supported value");
		return -1;
	}

	if (conf->primarysink_audio_notsupported &&
		wpas_wfd_y_n_str2bin(conf->primarysink_audio_notsupported,
				&wfd->primarysink_audio_notsupported)) {
		wpa_printf(MSG_ERROR,
			"WFD: Invalid primarysink_audio_notsupported value");
		return -1;
	}

	if (conf->source_audio_only_supported &&
		wpas_wfd_y_n_str2bin(conf->source_audio_only_supported,
				&wfd->source_audio_only_supported)) {
		wpa_printf(MSG_ERROR,
			"WFD: Invalid source_audio_only_supported value");
		return -1;
	}

	if (conf->tdls_persistent_group_intended &&
		wpas_wfd_y_n_str2bin(conf->tdls_persistent_group_intended,
				&wfd->tdls_persistent_group_intended)) {
		wpa_printf(MSG_ERROR,
			"WFD: Invalid tdls_persistent_group_intended value");
		return -1;
	}

	if (conf->tdls_persistent_group_reinvoke &&
		wpas_wfd_y_n_str2bin(conf->tdls_persistent_group_reinvoke,
				&wfd->tdls_persistent_group_reinvoke)) {
		wpa_printf(MSG_ERROR,
			"WFD: Invalid tdls_persistent_group_reinvoke value");
		return -1;
	}

	wfd->session_mgmt_ctrl_port = conf->wfd_session_mgmt_ctrl_port;
	wfd->device_max_throughput = conf->wfd_device_max_throughput;

	return 0;
}


/**
 * wpas_wfd_init - Initialize WFD module for %wpa_supplicant
 * @global: Pointer to global data from wpa_supplicant_init()
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * Returns: 0 on success, -1 on failure
 */
int wpas_wfd_init(struct wpa_global *global, struct wpa_supplicant *wpa_s)
{
	struct wfd_config wfd;

	if (global->wfd)
		return 0;

	os_memset(&wfd, 0, sizeof(wfd));
	wfd.enabled = DEFAULT_WFD_ENABLED;
	wfd.device_type = DEFAULT_DEVICE_TYPE;
	wfd.coupled_sink_supported_by_source =
				WFD_DEFAULT_COUPLED_SINK_SUPPORTED_BY_SOURCE;
	wfd.coupled_sink_supported_by_sink =
				WFD_DEFAULT_COUPLED_SINK_SUPPORTED_BY_SINK;
	wfd.available_for_session = WFD_DEFAULT_AVAILABLE_FOR_SESSION;
	wfd.service_discovery_supported =
				WFD_DEFAULT_SERVICE_DISCOVERY_SUPPORTED;
	wfd.preferred_connectivity = WFD_DEFAULT_PREFERRED_CONNECTIVITY;
	wfd.content_protection_supported =
				WFD_DEFAULT_CONTENT_PROTECTION_SUPPORTED;
	wfd.time_sync_supported = WFD_DEFAULT_TIME_SYNC_SUPPORTED;
	wfd.session_mgmt_ctrl_port = WFD_DEFAULT_SESSION_MGMT_CTRL_PORT;
	wfd.device_max_throughput = WFD_DEFAULT_MAX_THROUGHPUT;
	wfd.primarysink_audio_notsupported = WFD_DEFAULT_PRIMARYSINK_AUDIO_NOTSUPPORTED;
	wfd.source_audio_only_supported = WFD_DEFAULT_SOURCE_AUDIO_ONLY_SUPPORTED;
	wfd.tdls_persistent_group_intended = WFD_DEFAULT_TDLS_PERSISTENT_GROUP_INTENTED;
	wfd.tdls_persistent_group_reinvoke = WFD_DEFAULT_TDLS_PERSISTENT_GROUP_REINVOKE;

	if (wpas_wfd_parse_config(wpa_s->conf, &wfd))
		return -1;

	global->wfd = wfd_init(&wfd);
	if (global->wfd == NULL)
		return -1;

	p2p_set_wfd_data(global->p2p, global->wfd);

	return 0;
}


/**
 * wpas_wfd_deinit_global - Deinitialize global WFD module
 * @global: Pointer to global data from wpa_supplicant_init()
 *
 * This function deinitializes the global (per device) WFD
 * module.
 */
void wpas_wfd_deinit_global(struct wpa_global *global)
{
	if (global->wfd == NULL)
		return;
	wfd_deinit(global->wfd);
	global->wfd = NULL;
}


/**
 * wpas_wfd_update_config - Update WFD configuration
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 */
void wpas_wfd_update_config(struct wpa_supplicant *wpa_s)
{
	struct wfd_data *wfd = wpa_s->global->wfd;

	if (wfd == NULL)
		return;

	wpas_wfd_parse_config(wpa_s->conf, wfd_get_config(wpa_s->global->wfd));
}


/**
 * wpas_wfd_get_bssid - Get the current association BSSID
 * @wpa_s: Pointer to wpa_supplicant data from wpa_supplicant_add_iface()
 * @bssid: Buffer for BSSID (size of ETH_ALEN)
 * Returns: 0 on success, -1 on failure
 */
int wpas_wfd_get_bssid(struct wpa_supplicant *wpa_s, u8 *bssid)
{
	return wpa_drv_get_bssid(wpa_s, bssid);
}

