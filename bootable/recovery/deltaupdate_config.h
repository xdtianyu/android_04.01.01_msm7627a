/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#ifndef DELTAUPDATE_CONFIG_H_
#define DELTAUPDATE_CONFIG_H_

//Default location for delta update package
#define DEFAULT_PKG_LOCATION    "/cache/fota"

//Update status file. Acts as an interface between innopath DM client
//and update agent in recovery mode.
#define DELTA_UPDATE_STATUS_FILE    "/cache/fota/ipth_config_dfs.txt"

//Backup of the original status
#define DELTA_UPDATE_STATUS_BACKUP_FILE    "/cache/fota/ipth_config_dfs.bak"

//Number of times device enters into recovery during delta update.
//This prevents the device recycling endlessly in recovery mode.
#define NUM_OF_RECOVERY    "/cache/fota/ipth_num_recovery.txt"

//Recovery retry maximum during delta update
#define MAX_NUM_UPDATE_RECOVERY    (5)

//Contains information about delta update package location
#define FOTA_PROP_FILE    "/data/fota/ipth-muc.prop"

//Predefined AMSS image name
#define RADIO_IMAGE_LOCATION "/sys_boot/image/AMSS.MBN"

//Predefined AMSS backup image name
#define RADIO_IMAGE_LOCAL "/sys_boot/image/AMSS_LOCAL.MBN"

//Contains information about radio delta update
#define RADIO_DIFF_OUTPUT "/cache/fota/radio.diff"

//Indicates device firmware version to be used by DM client
//for server communication. Recovery mode updates this string
//when delta update is complete.
#define VERSION_STRING_NAME    "firmware.version"

//Indicates delta update package location defined in FOTA_PROP_FILE
#define PKG_LOCATION_STRING_NAME    "pkg.location"

//Predefined delta update package name
#define DIFF_PACKAGE_NAME    "ipth_package.bin"

//Android build.prop location to get firmware version information
#define BUILD_PROP_FILE    "/system/build.prop"

//Property string to differentiate firware version.
//Can be configured to other string such as ro.build.fingerprint
#define BUILD_PROP_NAME    "ro.build.version.release"

#define MAX_STRING_LEN    (4096)

//delta update result code based on PUMO spec
#define DELTA_UPDATE_SUCCESS_200   (200)
#define DELTA_UPDATE_FAILED_410    (410)

typedef struct {
    int idx;
    const char *str;
}deltaupdate_config_st;

// Delta update status
enum {
    NO_DELTA_UPDATE,
    START_DELTA_UPDATE,
    DELTA_UPDATE_IN_PROGRESS,
    DELTA_UPDATE_SUCCESSFUL,
    DELTA_UPDATE_FAILED
};

int start_delta_modemupdate(const char *path);
int extract_deltaupdate_binary(const char *path);
int run_modem_deltaupdate(void);

#endif // DELTAUPDATE_CONFIG_H

