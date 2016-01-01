/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2015 Christopher N. Hesse <raymanfx@gmail.com>
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cutils/properties.h>
#include <cutils/log.h>

#define LOG_TAG "modemloader"
#define LOG_NDEBUG 0

void parse_hardware_revision(unsigned int *revision)
{
    const char *cpuinfo = "/proc/cpuinfo";
    char *data = NULL;
    size_t len = 0, limit = 1024;
    int fd, n;
    char *x, *rev;

    fd = open(cpuinfo, O_RDONLY);
    if (fd < 0) return;

    for (;;) {
        x = (char*)realloc(data, limit);
        if (!x) {
            ALOGE("Failed to allocate memory to read %s\n", cpuinfo);
            goto done;
        }
        data = x;

        n = read(fd, data + len, limit - len);
        if (n < 0) {
            ALOGE("Failed reading %s: %s (%d)\n", cpuinfo, strerror(errno), errno);
            goto done;
        }
        len += n;

        if (len < limit)
            break;

        /* We filled the buffer, so increase size and loop to read more */
        limit *= 2;
    }

    data[len] = 0;
    rev = strstr(data, "\nRevision");

    if (rev) {
        x = strstr(rev, ": ");
        if (x) {
            *revision = strtoul(x + 2, 0, 16);
        }
    }

done:
    close(fd);
    free(data);
}

int main(void)
{
    unsigned int revision = 0;
    char ro_revision[PROP_VALUE_MAX];

    parse_hardware_revision(&revision);
    snprintf(ro_revision, PROP_VALUE_MAX, "%d", revision);
    property_set("ro.revision", ro_revision);

    return 0;
}
