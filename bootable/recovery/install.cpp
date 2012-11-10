/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "install.h"
#include "mincrypt/rsa.h"
#include "minui/minui.h"
#include "minzip/SysUtil.h"
#include "minzip/Zip.h"
#include "mtdutils/mounts.h"
#include "mtdutils/mtdutils.h"
#include "roots.h"
#include "verifier.h"
#include "ui.h"
#include "bootloader.h"

#include "deltaupdate_config.h"

#define ASSUMED_UPDATE_BINARY_NAME  "META-INF/com/google/android/update-binary"
#define ASSUMED_DELTAUPDATE_BINARY_NAME  "META-INF/com/google/android/ipth_dua"
#define RUN_DELTAUPDATE_AGENT  "/tmp/ipth_dua"
#define PUBLIC_KEYS_FILE "/res/keys"
#define RADIO_DIFF_NAME "radio.diff"

extern RecoveryUI* ui;
// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;


static const char *LAST_INSTALL_FILE = "/cache/recovery/last_install";

const ZipEntry* radio_diff;
// If the package contains an update binary, extract it and run it.
static int
try_update_binary(const char *path, ZipArchive *zip, int* wipe_cache) {
    const ZipEntry* binary_entry =
            mzFindZipEntry(zip, ASSUMED_UPDATE_BINARY_NAME);
    if (binary_entry == NULL) {
        mzCloseZipArchive(zip);
        return INSTALL_CORRUPT;
    }
    fprintf(stderr, "try_update_binary(path(%s))\n",path);

    radio_diff = mzFindZipEntry(zip, RADIO_DIFF_NAME);

    if (radio_diff == NULL) {
        fprintf(stderr, "%s not found\n", RADIO_DIFF_NAME);
    }
    else
    {
        char* diff_file = RADIO_DIFF_OUTPUT;
        int fd_diff = creat(diff_file, 0777);

        fprintf(stderr, "%s found\n", RADIO_DIFF_NAME);

        if (fd_diff < 0) {
            fprintf(stderr, "Can't make %s\n", diff_file);
        }
        else
        {
            bool ok_diff = mzExtractZipEntryToFile(zip, radio_diff, fd_diff);
            close(fd_diff);

            if (!ok_diff) {
                fprintf(stderr, "Can't copy %s\n", RADIO_DIFF_NAME);
            }
        }
    }

    const char* binary = "/tmp/update_binary";
    unlink(binary);
    int fd = creat(binary, 0755);
    if (fd < 0) {
        mzCloseZipArchive(zip);
        LOGE("Can't make %s\n", binary);
        return INSTALL_ERROR;
    }
    bool ok = mzExtractZipEntryToFile(zip, binary_entry, fd);
    close(fd);
    mzCloseZipArchive(zip);

    if (!ok) {
        LOGE("Can't copy %s\n", ASSUMED_UPDATE_BINARY_NAME);
        return INSTALL_ERROR;
    }

    int pipefd[2];
    pipe(pipefd);

    // When executing the update binary contained in the package, the
    // arguments passed are:
    //
    //   - the version number for this interface
    //
    //   - an fd to which the program can write in order to update the
    //     progress bar.  The program can write single-line commands:
    //
    //        progress <frac> <secs>
    //            fill up the next <frac> part of of the progress bar
    //            over <secs> seconds.  If <secs> is zero, use
    //            set_progress commands to manually control the
    //            progress of this segment of the bar
    //
    //        set_progress <frac>
    //            <frac> should be between 0.0 and 1.0; sets the
    //            progress bar within the segment defined by the most
    //            recent progress command.
    //
    //        firmware <"hboot"|"radio"> <filename>
    //            arrange to install the contents of <filename> in the
    //            given partition on reboot.
    //
    //            (API v2: <filename> may start with "PACKAGE:" to
    //            indicate taking a file from the OTA package.)
    //
    //            (API v3: this command no longer exists.)
    //
    //        ui_print <string>
    //            display <string> on the screen.
    //
    //   - the name of the package zip file.
    //

    const char** args = (const char**)malloc(sizeof(char*) * 5);
    args[0] = binary;
    args[1] = EXPAND(RECOVERY_API_VERSION);   // defined in Android.mk
    char* temp = (char*)malloc(10);
    sprintf(temp, "%d", pipefd[1]);
    args[2] = temp;
    args[3] = (char*)path;
    args[4] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        execv(binary, (char* const*)args);
        fprintf(stdout, "E:Can't run %s (%s)\n", binary, strerror(errno));
        _exit(-1);
    }
    close(pipefd[1]);

    *wipe_cache = 0;

    char buffer[1024];
    FILE* from_child = fdopen(pipefd[0], "r");
    while (fgets(buffer, sizeof(buffer), from_child) != NULL) {
        char* command = strtok(buffer, " \n");
        if (command == NULL) {
            continue;
        } else if (strcmp(command, "progress") == 0) {
            char* fraction_s = strtok(NULL, " \n");
            char* seconds_s = strtok(NULL, " \n");

            float fraction = strtof(fraction_s, NULL);
            int seconds = strtol(seconds_s, NULL, 10);

            ui->ShowProgress(fraction * (1-VERIFICATION_PROGRESS_FRACTION), seconds);
        } else if (strcmp(command, "set_progress") == 0) {
            char* fraction_s = strtok(NULL, " \n");
            float fraction = strtof(fraction_s, NULL);
            ui->SetProgress(fraction);
        } else if (strcmp(command, "ui_print") == 0) {
            char* str = strtok(NULL, "\n");
            if (str) {
                ui->Print("%s", str);
            } else {
                ui->Print("\n");
            }
        } else if (strcmp(command, "wipe_cache") == 0) {
            *wipe_cache = 1;
        } else if (strcmp(command, "clear_display") == 0) {
            ui->SetBackground(RecoveryUI::NONE);
        } else {
            LOGE("unknown command [%s]\n", command);
        }
    }
    fclose(from_child);

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOGE("Error in %s\n(Status %d)\n", path, WEXITSTATUS(status));
        return INSTALL_ERROR;
    }

    return INSTALL_SUCCESS;
}

// Reads a file containing one or more public keys as produced by
// DumpPublicKey:  this is an RSAPublicKey struct as it would appear
// as a C source literal, eg:
//
//  "{64,0xc926ad21,{1795090719,...,-695002876},{-857949815,...,1175080310}}"
//
// (Note that the braces and commas in this example are actual
// characters the parser expects to find in the file; the ellipses
// indicate more numbers omitted from this example.)
//
// The file may contain multiple keys in this format, separated by
// commas.  The last key must not be followed by a comma.
//
// Returns NULL if the file failed to parse, or if it contain zero keys.
static RSAPublicKey*
load_keys(const char* filename, int* numKeys) {
    RSAPublicKey* out = NULL;
    *numKeys = 0;

    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        LOGE("opening %s: %s\n", filename, strerror(errno));
        goto exit;
    }

    {
        int i;
        bool done = false;
        while (!done) {
            ++*numKeys;
            out = (RSAPublicKey*)realloc(out, *numKeys * sizeof(RSAPublicKey));
            RSAPublicKey* key = out + (*numKeys - 1);
            if (fscanf(f, " { %i , 0x%x , { %u",
                       &(key->len), &(key->n0inv), &(key->n[0])) != 3) {
                goto exit;
            }
            if (key->len != RSANUMWORDS) {
                LOGE("key length (%d) does not match expected size\n", key->len);
                goto exit;
            }
            for (i = 1; i < key->len; ++i) {
                if (fscanf(f, " , %u", &(key->n[i])) != 1) goto exit;
            }
            if (fscanf(f, " } , { %u", &(key->rr[0])) != 1) goto exit;
            for (i = 1; i < key->len; ++i) {
                if (fscanf(f, " , %u", &(key->rr[i])) != 1) goto exit;
            }
            fscanf(f, " } } ");

            // if the line ends in a comma, this file has more keys.
            switch (fgetc(f)) {
            case ',':
                // more keys to come.
                break;

            case EOF:
                done = true;
                break;

            default:
                LOGE("unexpected character between keys\n");
                goto exit;
            }
        }
    }

    fclose(f);
    return out;

exit:
    if (f) fclose(f);
    free(out);
    *numKeys = 0;
    return NULL;
}

static int
really_install_package(const char *path, int* wipe_cache)
{
    ui->SetBackground(RecoveryUI::INSTALLING);
    ui->Print("Finding update package...\n");
    ui->SetProgressType(RecoveryUI::INDETERMINATE);
    LOGI("Update location: %s\n", path);

    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return INSTALL_CORRUPT;
    }

    ui->Print("Opening update package...\n");

    int numKeys;
    RSAPublicKey* loadedKeys = load_keys(PUBLIC_KEYS_FILE, &numKeys);
    if (loadedKeys == NULL) {
        LOGE("Failed to load keys\n");
        return INSTALL_CORRUPT;
    }
    LOGI("%d key(s) loaded from %s\n", numKeys, PUBLIC_KEYS_FILE);

    // Give verification half the progress bar...
    ui->Print("Verifying update package...\n");
    ui->SetProgressType(RecoveryUI::DETERMINATE);
    ui->ShowProgress(VERIFICATION_PROGRESS_FRACTION, VERIFICATION_PROGRESS_TIME);

    int err;
    err = verify_file(path, loadedKeys, numKeys);
    free(loadedKeys);
    LOGI("verify_file returned %d\n", err);
    if (err != VERIFY_SUCCESS) {
        LOGE("signature verification failed\n");
        return INSTALL_CORRUPT;
    }

    /* Try to open the package.
     */
    ZipArchive zip;
    err = mzOpenZipArchive(path, &zip);
    if (err != 0) {
        LOGE("Can't open %s\n(%s)\n", path, err != -1 ? strerror(err) : "bad");
        return INSTALL_CORRUPT;
    }

    /* Verify and install the contents of the package.
     */
    ui->Print("Installing update...\n");
    return try_update_binary(path, &zip, wipe_cache);
}

int
install_package(const char* path, int* wipe_cache, const char* install_file)
{
    FILE* install_log = fopen_path(install_file, "w");
    if (install_log) {
        fputs(path, install_log);
        fputc('\n', install_log);
    } else {
        LOGE("failed to open last_install: %s\n", strerror(errno));
    }
    int result = really_install_package(path, wipe_cache);
    if (install_log) {
        fputc(result == INSTALL_SUCCESS ? '1' : '0', install_log);
        fputc('\n', install_log);
        fclose(install_log);
    }
    return result;
}

int extract_deltaupdate_binary(const char *path)
{
    int err;
    ZipArchive zip;

    // Try to open the package.
    err = mzOpenZipArchive(path, &zip);
    if (err != 0) {
        LOGE("Can't open %s\n(%s)\n", path, err != -1 ? strerror(err) : "bad");
        return INSTALL_ERROR;
    }

    const ZipEntry* dua_entry =
            mzFindZipEntry(&zip, ASSUMED_DELTAUPDATE_BINARY_NAME);
    if (dua_entry == NULL) {
        mzCloseZipArchive(&zip);
       LOGE("Can't find %s\n", ASSUMED_DELTAUPDATE_BINARY_NAME);
        return INSTALL_ERROR;
    }

    char* deltaupdate_agent = RUN_DELTAUPDATE_AGENT;
    unlink(deltaupdate_agent);
    int fd = creat(deltaupdate_agent, 0755);
    if (fd < 0) {
        mzCloseZipArchive(&zip);
        LOGE("Can't make %s\n", deltaupdate_agent);
        return INSTALL_ERROR;
    }

    bool ok = mzExtractZipEntryToFile(&zip, dua_entry, fd);
    close(fd);
    mzCloseZipArchive(&zip);

    if (!ok) {
        LOGE("Can't copy %s\n", ASSUMED_DELTAUPDATE_BINARY_NAME);
        return INSTALL_ERROR;
    }

    return 0;
}

int run_modem_deltaupdate(void)
{
    int ret;

    pid_t duapid = fork();

    if (duapid == -1)
    {
        LOGE("fork failed. Returning error.\n");
        return INSTALL_ERROR;
    }

    if (duapid == 0)
    {//child process
     /*
      * argv[0] ipth_dua exeuable command itself
      * argv[1] false(default) - old binary update as a block / true - old
      * binary update as a file
      * argv[2] old binary file name. Will be used as partition name if argv[1]
      * is false
      * argv[3] diff package name
      * argv[4] flash memory block size in KB
      */
       char** args = (char **)malloc(sizeof(char*) * 5);
       if (target_is_emmc()) {
           args[0] = RUN_DELTAUPDATE_AGENT;
           args[1] = "true";
           args[2] = RADIO_IMAGE_LOCAL;
           args[3] = RADIO_DIFF_OUTPUT;
           args[4] = "256";
           args[5] = NULL;
       } else {
           args[0] = RUN_DELTAUPDATE_AGENT;
           args[1] = "false";
           args[2] = "AMSS";
           args[3] = RADIO_DIFF_OUTPUT;
           args[4] = "256";
           args[5] = NULL;
       }

       execv(RUN_DELTAUPDATE_AGENT, args);
       fprintf(stdout, "E:Can't run %s (%s)\n", RUN_DELTAUPDATE_AGENT, strerror(errno));
       _exit(-1);
    }

    //parents process
    waitpid(duapid, &ret, 0);
    if (!WIFEXITED(ret) || WEXITSTATUS(ret) != 0) {
        LOGE("Error in %s\n(Status %d)\n", RUN_DELTAUPDATE_AGENT, WEXITSTATUS(ret));
        return INSTALL_ERROR;
    }

    return INSTALL_SUCCESS;
}

int get_amss_backup(const char* amss_path_name1, const char* amss_path_name2)
{
    FILE *fp_read, *fp_write;
    char *buffer;
    unsigned max_size = (256 * 1024);
    unsigned num_read = 0;
    int ret = 0;

    fp_read = fopen_path(amss_path_name1,"rb");
    if (fp_read == NULL) {
        LOGE("Failed to open %s\n",amss_path_name1);
        return -1;
    }

    fp_write = fopen_path(amss_path_name2,"wb+");
    if (fp_write == NULL) {
        LOGE("Failed to open %s\n",amss_path_name2);
        fclose(fp_read);
        return -1;
    }
    buffer = (char *) malloc(sizeof(char)*max_size);
    if (buffer == NULL) {
        LOGE("Failed to allocate buffer\n");
        fclose(fp_read);
        fclose(fp_write);
        return -1;
    }
    while (!feof(fp_read)) {
           if ((num_read = fread(buffer, 1, max_size, fp_read)) < 0) {
               LOGE("Failed to read from file :%s\n",amss_path_name1);
               ret = -1;
               goto fail;
           }
           if(fwrite(buffer, 1, num_read, fp_write) < 0) {
               LOGE("Failed to write to file :%s\n",amss_path_name2);
               ret = -1;
               goto fail;
           }
    }

fail:
    fclose(fp_read);
    fclose(fp_write);
    free(buffer);
    return ret;
}

int get_amss_location(const char* amss_path_name)
{
    FILE* fp;
    int i = 0;

    fp = fopen_path(amss_path_name, "rw");

    if (fp == NULL) {
        LOGI("Failed to open %s\n",amss_path_name);
        return -1;
    } else {
        fclose(fp);
    }

    if (access(amss_path_name, F_OK) != 0) {
        LOGI("amss image does not exist %s\n", amss_path_name);
        return -1;
    }

    LOGI("amss image path name: %s\n", amss_path_name);

    return 0;
}

int start_delta_modemupdate(const char *path)
{
    int ret = 0;

    if (radio_diff == NULL)
    {
        LOGE("No modem package available.\n");
        LOGE("No modem update needed. returning O.K\n");
        return DELTA_UPDATE_SUCCESS_200;
    }

    // If the package contains an delta update binary for modem update, extract it
    ret = extract_deltaupdate_binary(path);
    if(ret != 0)
    {
       LOGE("idev_extractDua returned error(%d)\n", ret);
       return ret;
    }

    // Check and mount AMSS partition
    if (target_is_emmc()) {
        ret = get_amss_location(RADIO_IMAGE_LOCATION);
        if (ret != 0) {
            LOGE("get_amss_location returned error(%d)\n", ret);
            return ret;
        }
        /* Backup radio image before proceeding with the update */
        ret = get_amss_backup(RADIO_IMAGE_LOCATION, RADIO_IMAGE_LOCAL);
        if (ret != 0) {
            LOGI("Failed to get amss backup\n");
            return ret;
         }
    }

    // Execute modem update using delta update binary
    ret = run_modem_deltaupdate();
    LOGE("modem update result(%d)\n", ret);

    if(ret == 0) {
        if (target_is_emmc()) {
            if (remove(RADIO_IMAGE_LOCATION)) {
                LOGE("Failed to remove amss binary: %s\n",strerror(errno));
                return ret;
            }
            if (rename(RADIO_IMAGE_LOCAL, RADIO_IMAGE_LOCATION)) {
                LOGI("Failed to restore amss binary: %s\n",strerror(errno));
                return ret;
            }
        }
	return DELTA_UPDATE_SUCCESS_200;
    }
    else
       return ret;
}
