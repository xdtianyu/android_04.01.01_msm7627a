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

#ifndef WFD_H
#define WFD_H

enum wfd_device_type {
	WFD_SOURCE,
	WFD_PRIMARY_SINK,
	WFD_SECONDARY_SINK,
	WFD_SOURCE_PRIMARY_SINK
};

enum wfd_preferred_connectivity_type {
	WFD_P2P,
	WFD_TDLS
};

/**
 * struct wfd_peer_info - WFD peer information
 */
struct wfd_peer_info {
	/**
	 * wfd_supported - WFD is supported by this device
	 */
	int wfd_supported;

	/**
	 * device_type - Type of WFD device
	 */
	enum wfd_device_type device_type;

	/**
	 * coupled_sink_supported_by_source - Coupled sink operation is
	 * supported by source device
	 */
	int coupled_sink_supported_by_source;

	/**
	 * coupled_sink_supported_by_sink - Coupled sink operation is
	 * supported by sink device
	 */
	int coupled_sink_supported_by_sink;

	/**
	 * available_for_session - Available for WFD session
	 */
	int available_for_session;

	/**
	 * service_discovery_supported - WFD service discovery is supported
	 */
	int service_discovery_supported;

	/**
	 * preferred_connectivity - Preferred connectivity scheme
	 */
	enum wfd_preferred_connectivity_type preferred_connectivity;

	/**
	 * content_protection_supported - Content protection using
	 * HDCP2.0 is supported
	 */
	int content_protection_supported;

	/**
	 * time_sync_supported - Time synchronization using 802.1AS is
	 * supported
	 */
	int time_sync_supported;

	/**
	 * session_mgmt_ctrl_port - TCP port at which the device listens
	 * for RTSP messages
	 */
	u16 session_mgmt_ctrl_port;

	/**
	 * device_max_throughput - Maximum average throughput capability
	 * of the device represented in multiples of 1 Mbps
	 */
	u16 device_max_throughput;

	/**
	 * is_associated_with_ap - Is the device associated with an AP
	 */
	int is_associated_with_ap;

	/**
	 * associated_bssid - Address of the AP that the device is
	 * associated with
	 */
	u8 associated_bssid[ETH_ALEN];
};

/**
 * struct wfd_config - WFD configuration
 *
 * This configuration is provided to the WFD module during
 * initialization with wfd_init().
 */
struct wfd_config {
	/**
	 * enabled - WFD is currently enabled
	 */
	int enabled;

	/**
	 * device_type - Type of WFD device
	 */
	enum wfd_device_type device_type;

	/**
	 * coupled_sink_supported_by_source - Coupled sink operation is
	 * supported by source device
	 */
	int coupled_sink_supported_by_source;

	/**
	 * coupled_sink_supported_by_sink - Coupled sink operation is
	 * supported by sink device
	 */
	int coupled_sink_supported_by_sink;

	/**
	 * available_for_session - Available for WFD session
	 */
	int available_for_session;

	/**
	 * service_discovery_supported - WFD service discovery is supported
	 */
	int service_discovery_supported;

	/**
	 * preferred_connectivity - Preferred connectivity scheme
	*/
	enum wfd_preferred_connectivity_type preferred_connectivity;

	/**
	 * content_protection_supported - Content protection using
	 * HDCP2.0 is supported
	 */
	int content_protection_supported;

	/**
	* time_sync_supported - Time synchronization using 802.1AS is
	* supported
	*/
	int time_sync_supported;

	/**
	 * primarysink_audio_notsupported - Primary sink does not support
	 * audio
	 */
	int primarysink_audio_notsupported;

	/**
	 * source_audio_only_supported - Source supports audio only
	 * session
	 */
	int source_audio_only_supported;

	/**
	 * tdls_persistent_group_intended - tdls persistent group
	 * intended
	 */
	int tdls_persistent_group_intended;

	/**
	 * tdls_persistent_group_reinvoke - tdls persistent group
	 * reinvoke
	 */
	int tdls_persistent_group_reinvoke;

	/**
	 * session_mgmt_ctrl_port - TCP port at which the device listens
	 * for RTSP messages
	 */
	u16 session_mgmt_ctrl_port;

	/**
	 * device_max_throughput - Maximum average throughput capability
	 * of the device represented in multiples of 1Mbps
	 */
	u16 device_max_throughput;
};


/* WFD module initialization/deinitialization */

/**
 * wfd_init - Initialize WFD module
 * @cfg: WFD module configuration
 * Returns: Pointer to private data or %NULL on failure
 *
 * This function is used to initialize global WFD module context (one per
 * device). The WFD module will keep a copy of the configuration data, so the
 * caller does not need to maintain this structure
 */
struct wfd_data *wfd_init(const struct wfd_config *wfd);

/**
 * wfd_deinit - Deinitialize WFD module
 * @wfd: WFD module context from wfd_init()
 */
void wfd_deinit(struct wfd_data *wfd);


/* Generic helper functions */

/**
 * wfd_scan_result_text - Build text format description of WFD IE
 * @ies: Information elements from scan results
 * @ies_len: ies buffer length in octets
 * @buf: Buffer for returning text
 * @end: Pointer to the end of the buf area
 * Returns: Number of octets written to the buffer or -1 on failure
 *
 * This function can be used to parse WFD IE contents into text format
 * field=value lines.
 */
int wfd_scan_result_text(const u8 *ies, size_t ies_len, char *buf, char *end);

/**
 * wfd_add_wfd_ie - Add WFD IE to set of IEs
 * @ctx: Context
 * @wfd: WFD module context from wfd_init()
 * @ies: Buffer to which WFD IE is added
 */
void wfd_add_wfd_ie(void *ctx, struct wfd_data *wfd, struct wpabuf *ies);

/**
 * wfd_add_peer_info - Add WFD information to discovered P2P peer device
 * @wfd: WFD module context from wfd_init()
 * @msg_ctx: Message context
 * @wfd_info: WFD information to be filled in
 * @ies: IEs from the Beacon or Probe Response frame
 * @ies_len: Length of ies buffer in octets
 * Returns: 0 on success, -1 on failure
 */
int wfd_add_peer_info(void *msg_ctx, struct wfd_peer_info *wfd_info,
					  const u8 *ies, size_t ies_len);

/**
 * wfd_device_type_text - Return text for device type
 * @device_type: device type
 * Returns: text for that device type
 */
const char *wfd_device_type_text(enum wfd_device_type device_type);

/**
 * wfd_perferred_connectivity_text - Return text for perferred connectivity
 * @perferred_connectivity: perferred connectivity
 * Returns: text for that perferred connectivity
 */
const char *wfd_preferred_connectivity_text(
	enum wfd_preferred_connectivity_type preferred_connectivity);

/**
 * wfd_get_config - Gets the current WFD configuration
 * @wfd: WFD module context from wfd_init()
 * Returns: configuration
 */
struct wfd_config *wfd_get_config(struct wfd_data *wfd);

#endif /* WFD_H */
