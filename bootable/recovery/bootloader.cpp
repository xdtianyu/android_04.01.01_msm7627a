/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bootloader.h"
#include "common.h"
#include "mtdutils/mtdutils.h"
#include "roots.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <cutils/properties.h>
#include <unistd.h>
#include <fcntl.h>

static int get_bootloader_message_mtd(struct bootloader_message *out, const Volume* v);
static int set_bootloader_message_mtd(const struct bootloader_message *in, const Volume* v);
static int get_bootloader_message_block(struct bootloader_message *out, const Volume* v);
static int set_bootloader_message_block(const struct bootloader_message *in, const Volume* v);
static void wait_for_device(const char* fn);

int get_bootloader_message(struct bootloader_message *out) {
    Volume* v = volume_for_path("/misc");
    if (v == NULL) {
      LOGE("Cannot load volume /misc!\n");
      return -1;
    }
    if (strcmp(v->fs_type, "mtd") == 0) {
        return get_bootloader_message_mtd(out, v);
    } else if (strcmp(v->fs_type, "emmc") == 0) {
        return get_bootloader_message_block(out, v);
    }
    LOGE("unknown misc partition fs_type \"%s\"\n", v->fs_type);
    return -1;
}

int set_bootloader_message(const struct bootloader_message *in) {
    Volume* v = volume_for_path("/misc");
    if (v == NULL) {
      LOGE("Cannot load volume /misc!\n");
      return -1;
    }
    if (strcmp(v->fs_type, "mtd") == 0) {
        return set_bootloader_message_mtd(in, v);
    } else if (strcmp(v->fs_type, "emmc") == 0) {
        return set_bootloader_message_block(in, v);
    }
    LOGE("unknown misc partition fs_type \"%s\"\n", v->fs_type);
    return -1;
}

// ------------------------------
// for misc partitions on MTD
// ------------------------------

static const int MISC_PAGES = 3;         // number of pages to save
static const int MISC_COMMAND_PAGE = 1;  // bootloader command is this page

static int get_bootloader_message_mtd(struct bootloader_message *out,
                                      const Volume* v) {
    size_t write_size;
    mtd_scan_partitions();
    const MtdPartition *part = mtd_find_partition_by_name(v->device);
    if (part == NULL || mtd_partition_info(part, NULL, NULL, &write_size)) {
        LOGE("Can't find %s\n", v->device);
        return -1;
    }

    MtdReadContext *read = mtd_read_partition(part);
    if (read == NULL) {
        LOGE("Can't open %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }

    const ssize_t size = write_size * MISC_PAGES;
    char data[size];
    ssize_t r = mtd_read_data(read, data, size);
    if (r != size) LOGE("Can't read %s\n(%s)\n", v->device, strerror(errno));
    mtd_read_close(read);
    if (r != size) return -1;

    memcpy(out, &data[write_size * MISC_COMMAND_PAGE], sizeof(*out));
    return 0;
}
static int set_bootloader_message_mtd(const struct bootloader_message *in,
                                      const Volume* v) {
    size_t write_size;
    mtd_scan_partitions();
    const MtdPartition *part = mtd_find_partition_by_name(v->device);
    if (part == NULL || mtd_partition_info(part, NULL, NULL, &write_size)) {
        LOGE("Can't find %s\n", v->device);
        return -1;
    }

    MtdReadContext *read = mtd_read_partition(part);
    if (read == NULL) {
        LOGE("Can't open %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }

    ssize_t size = write_size * MISC_PAGES;
    char data[size];
    ssize_t r = mtd_read_data(read, data, size);
    if (r != size) LOGE("Can't read %s\n(%s)\n", v->device, strerror(errno));
    mtd_read_close(read);
    if (r != size) return -1;

    memcpy(&data[write_size * MISC_COMMAND_PAGE], in, sizeof(*in));

    MtdWriteContext *write = mtd_write_partition(part);
    if (write == NULL) {
        LOGE("Can't open %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    if (mtd_write_data(write, data, size) != size) {
        LOGE("Can't write %s\n(%s)\n", v->device, strerror(errno));
        mtd_write_close(write);
        return -1;
    }
    if (mtd_write_close(write)) {
        LOGE("Can't finish %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }

    LOGI("Set boot command \"%s\"\n", in->command[0] != 255 ? in->command : "");
    return 0;
}

int set_fota_cookie()
{
    if (target_is_emmc())
        return set_fota_cookie_mmc();
    else
        return set_fota_cookie_mtd();
}

int reset_fota_cookie()
{
    if (target_is_emmc())
        return reset_fota_cookie_mmc();
    else
        return reset_fota_cookie_mtd();
}
// FOTA cookie indicates that an android or modem image package
// is available for delta update
int set_fota_cookie_mtd(void)
{
    size_t write_size;

    mtd_scan_partitions();
    const MtdPartition *part = mtd_find_partition_by_name("FOTA");

    if (part == NULL || mtd_partition_info(part, NULL, NULL, &write_size)) {
        LOGE("Can't find FOTA\n");
        return -1;
    }

    MtdReadContext *read = mtd_read_partition(part);
    if (read == NULL) {
        LOGE("Can't open FOTA\n(%s)\n", strerror(errno));
        return -1;
    }

    ssize_t size = write_size; //writing 1 page is enough
    char data[size];
    ssize_t r = mtd_read_data(read, data, size);
    if (r != size) LOGE("Can't read FOTA\n(%s)\n", strerror(errno));
    mtd_read_close(read);
    if (r != size) return -1;

    //setting FOTA cookie value, 0x64645343
    memset(data, 0x0, sizeof(data));
    data[0] = 0x43;
    data[1] = 0x53;
    data[2] = 0x64;
    data[3] = 0x64;

    MtdWriteContext *write = mtd_write_partition(part);
    if (write == NULL) {
        LOGE("Can't open FOTA\n(%s)\n", strerror(errno));
        return -1;
    }
    if (mtd_write_data(write, data, size) != size) {
        LOGE("Can't write FOTA\n(%s)\n", strerror(errno));
        mtd_write_close(write);
        return -1;
    }
    if (mtd_write_close(write)) {
        LOGE("Can't finish FOTA\n(%s)\n", strerror(errno));
        return -1;
    }

    LOGI("Set FOTA cookie done.\n");
    return 0;
}

//Write FOTA cookie for MMC device
int set_fota_cookie_mmc(void)
{
    int count = 0;
    Volume* v = volume_for_path("/FOTA");
     if (v == NULL) {
         LOGE("Cannot load volume /FOTA\n");
         return -1;
    }
    wait_for_device(v->device);

    int fd = open(v->device, O_RDWR|O_SYNC);
    if (fd < 0) {
        LOGE("Can't open %s\n(%s)\n", v->device, strerror(errno));
        return -1;
     }

    char data[512];
    memset(data, 0x0, sizeof(data));
    data[0] = 0x43;
    data[1] = 0x53;
    data[2] = 0x64;
    data[3] = 0x64;

    count = write(fd,(char *)data,512);
    if (count <= 0) {
        LOGE("Failed writing %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    if (close(fd) != 0) {
        LOGE("Failed closing %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    return 0;
}

int reset_fota_cookie_mtd(void)
{
    size_t write_size;

    mtd_scan_partitions();
    const MtdPartition *part = mtd_find_partition_by_name("FOTA");
    if (part == NULL || mtd_partition_info(part, NULL, NULL, &write_size)) {
        LOGE("Can't find FOTA\n");
        return -1;
    }

    MtdReadContext *read = mtd_read_partition(part);
    if (read == NULL) {
        LOGE("Can't open FOTA\n(%s)\n", strerror(errno));
        return -1;
    }

    ssize_t size = write_size; //writing 1 page is enough
    char data[size];
    ssize_t r = mtd_read_data(read, data, size);
    if (r != size) LOGE("Can't read FOTA\n(%s)\n", strerror(errno));
    mtd_read_close(read);
    if (r != size) return -1;

    //Resetting FOTA cookie value
    memset(data, 0x0, sizeof(data));

    MtdWriteContext *write = mtd_write_partition(part);
    if (write == NULL) {
        LOGE("Can't open FOTA\n(%s)\n", strerror(errno));
        return -1;
    }
    if (mtd_write_data(write, data, size) != size) {
        LOGE("Can't write FOTA\n(%s)\n", strerror(errno));
        mtd_write_close(write);
        return -1;
    }
    if (mtd_write_close(write)) {
        LOGE("Can't finish FOTA\n(%s)\n", strerror(errno));
        return -1;
    }

    LOGI("Reset FOTA cookie done.\n");
    return 0;
}

int reset_fota_cookie_mmc(void)
{
    int count = 0;
    Volume* v = volume_for_path("/FOTA");
     if (v == NULL) {
         LOGE("Cannot load volume /FOTA\n");
         return -1;
    }
    wait_for_device(v->device);

    int fd = open(v->device, O_RDWR|O_SYNC);
    if (fd < 0) {
        LOGE("Can't open %s\n(%s)\n", v->device, strerror(errno));
        return -1;
     }

    char data[512];
    memset(data, 0x0, sizeof(data));

    count = write(fd,(char *)data,512);
    if (count <= 0) {
        LOGE("Failed writing %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    if (close(fd) != 0) {
        LOGE("Failed closing %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    return 0;
}


// ------------------------------------
// for misc partitions on block devices
// ------------------------------------

static void wait_for_device(const char* fn) {
    int tries = 0;
    int ret;
    struct stat buf;
    do {
        ++tries;
        ret = stat(fn, &buf);
        if (ret) {
            printf("stat %s try %d: %s\n", fn, tries, strerror(errno));
            sleep(1);
        }
    } while (ret && tries < 10);
    if (ret) {
        printf("failed to stat %s\n", fn);
    }
}

static int get_bootloader_message_block(struct bootloader_message *out,
                                        const Volume* v) {
    wait_for_device(v->device);
    FILE* f = fopen(v->device, "rb");
    if (f == NULL) {
        LOGE("Can't open %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    struct bootloader_message temp;
    int count = fread(&temp, sizeof(temp), 1, f);
    if (count != 1) {
        LOGE("Failed reading %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    if (fclose(f) != 0) {
        LOGE("Failed closing %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    memcpy(out, &temp, sizeof(temp));
    return 0;
}

static int set_bootloader_message_block(const struct bootloader_message *in,
                                        const Volume* v) {
    wait_for_device(v->device);
    FILE* f = fopen(v->device, "wb");
    if (f == NULL) {
        LOGE("Can't open %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    int count = fwrite(in, sizeof(*in), 1, f);
    if (count != 1) {
        LOGE("Failed writing %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    if (fclose(f) != 0) {
        LOGE("Failed closing %s\n(%s)\n", v->device, strerror(errno));
        return -1;
    }
    return 0;
}

int target_is_emmc()
{
    char emmc[PROPERTY_VALUE_MAX];
    int result = 0;

    property_get("ro.boot.emmc", emmc, "");

    if (!strncmp(emmc, "true", 4))
        result = 1;
    else
        result = 0;

    return result;
}
