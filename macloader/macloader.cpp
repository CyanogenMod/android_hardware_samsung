/*
 * Copyright (C) 2012, The CyanogenMod Project
 *                     Daniel Hillenbrand <codeworkx@cyanogenmod.com>
 *                     Marco Hillenbrand <marco.hillenbrand@googlemail.com>
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <cutils/log.h>

#define LOG_TAG "macloader"
#define LOG_NDEBUG 0

#define MACADDR_PATH "/efs/wifi/.mac.info"
#define CID_PATH "/data/.cid.info"

enum Type {
    NONE,
    MURATA,
    SEMCOSH,
    SEMCOVE
};

int main() {
    FILE* file;
    FILE* cidfile;
    char* str;
    char mac_addr_half[9];
    int ret = -1;
    int amode;
    enum Type type = NONE;

    /* open mac addr file */
    file = fopen(MACADDR_PATH, "r");
    if (file == 0) {
        fprintf(stderr, "open(%s) failed\n", MACADDR_PATH);
        ALOGE("Can't open %s\n", MACADDR_PATH);
        return -1;
    }

    /* get and compare mac addr */
    str = fgets(mac_addr_half, 9, file);
    if (str == 0) {
        fprintf(stderr, "fgets() from file %s failed\n", MACADDR_PATH);
        ALOGE("Can't read from %s\n", MACADDR_PATH);
        return -1;
    }

    /* murata */
    if (strncasecmp(mac_addr_half, "00:37:6d", 9) == 0 ||
        strncasecmp(mac_addr_half, "88:30:8a", 9) == 0 ||
        strncasecmp(mac_addr_half, "20:02:af", 9) == 0 ||
        strncasecmp(mac_addr_half, "5c:f8:a1", 9) == 0 ||
        strncasecmp(mac_addr_half, "40:f3:08", 9) == 0 ||
        strncasecmp(mac_addr_half, "f0:27:65", 9) == 0 ||
        strncasecmp(mac_addr_half, "1c:99:4c", 9) == 0 ||
        strncasecmp(mac_addr_half, "60:21:c0", 9) == 0) {
        type = MURATA;
    }

    /* semcosh */
    if (strncasecmp(mac_addr_half, "5c:0a:5b", 9) == 0 ||
        strncasecmp(mac_addr_half, "cc:3a:61", 9) == 0) {
        type = SEMCOSH;
    }

    if (type != NONE) {
        /* open cid file */
        cidfile = fopen(CID_PATH, "w");
        if(cidfile == 0) {
            fprintf(stderr, "open(%s) failed\n", CID_PATH);
            ALOGE("Can't open %s\n", CID_PATH);
            return -1;
        }

        switch(type) {
            case NONE:
                return -1;
            break;
            case MURATA:
                /* write murata to cid file */
                ALOGI("Writing murata to %s\n", CID_PATH);
                ret = fputs("murata", cidfile);
            break;
            case SEMCOSH:
                /* write semcosh to cid file */
                ALOGI("Writing semcosh to %s\n", CID_PATH);
                ret = fputs("semcosh", cidfile);
            break;
            case SEMCOVE:
                /* write semcove to cid file */
                ALOGI("Writing semcove to %s\n", CID_PATH);
                ret = fputs("semcove", cidfile);
            break;
         }

        if (ret != 0) {
            fprintf(stderr, "fputs() to file %s failed\n", CID_PATH);
            ALOGE("Can't write to %s\n", CID_PATH);
            return -1;
        }
        fclose(cidfile);

        /* set permissions on cid file */
        ALOGD("Setting permissions on %s\n", CID_PATH);
        amode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        ret = chmod(CID_PATH, amode);

        char* chown_cmd = (char*) malloc(strlen("chown system ") + strlen(CID_PATH) + 1);
        char* chgrp_cmd = (char*) malloc(strlen("chgrp system ") + strlen(CID_PATH) + 1);
        sprintf(chown_cmd, "chown system %s", CID_PATH);
        sprintf(chgrp_cmd, "chgrp system %s", CID_PATH);
        system(chown_cmd);
        system(chgrp_cmd);

        if (ret != 0) {
            fprintf(stderr, "chmod() on file %s failed\n", CID_PATH);
            ALOGE("Can't set permissions on %s\n", CID_PATH);
            return ret;
        }

    } else {
        /* delete cid file if no specific type */
        ALOGD("Deleting file %s\n", CID_PATH);
        remove(CID_PATH);
    }
    fclose(file);
    return 0;
}
