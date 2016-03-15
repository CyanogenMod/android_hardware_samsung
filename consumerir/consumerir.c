/*
 * Copyright (C) 2014 The Android Open Source Project
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
#define LOG_TAG "ConsumerIrHal"

#include <stdlib.h>
#include <malloc.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/consumerir.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 30000},
    {.min = 33000, .max = 33000},
    {.min = 36000, .max = 36000},
    {.min = 38000, .max = 38000},
    {.min = 40000, .max = 40000},
    {.min = 56000, .max = 56000},
};

static bool
try_append_number(char *buffer, int *len, int size, int number)
{
    int stored;

    stored = snprintf(&buffer[*len], size - *len, "%d,", number);

    if (stored < 0 || stored >= size - *len) {
        return false;
    }

    *len += stored;
    return true;
}

static bool
grow_buffer(char **buffer, int *size)
{
    char *new_buffer;

    *size *= 2;
    if ((new_buffer = realloc(*buffer, *size)) == NULL) {
        return false;
    }
    *buffer = new_buffer;
    return true;
}

static bool
append_number(char **buffer, int *len, int *size, int number)
{
    if (! try_append_number(*buffer, len, *size, number)) {
        if (! grow_buffer(buffer, size)) return false;
        return try_append_number(*buffer, len, *size, number);
    } else {
        return true;
    }
}

int fd = 0;
static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int buffer_len = 0;
    int buffer_size = 128;
    int i;
    char *buffer;

    if ((buffer = malloc(buffer_size)) == NULL) {
        return -ENOMEM;
    }

    /* write the header */
    if (! append_number(&buffer, &buffer_len, &buffer_size, carrier_freq)) {
        goto error;
    }

    /* calculate factor of conversion from microseconds to pulses */
    float factor = 1000000 / carrier_freq;

    /* write out the timing pattern */
    for (i = 0; i < pattern_len; i++)
    {
        if (! append_number(&buffer, &buffer_len, &buffer_size, (int) (pattern[i]/factor))) {
            goto error;
        }
    }

    buffer[buffer_len - 1] = 0;
    write(fd, buffer, buffer_len - 1);

    free(buffer);

    return 0;

error:
    free(buffer);
    return -ENOMEM;
}

static int consumerir_get_num_carrier_freqs(struct consumerir_device *dev __unused)
{
    return ARRAY_SIZE(consumerir_freqs);
}

static int consumerir_get_carrier_freqs(struct consumerir_device *dev __unused,
    size_t len, consumerir_freq_range_t *ranges)
{
    size_t to_copy = ARRAY_SIZE(consumerir_freqs);

    to_copy = len < to_copy ? len : to_copy;
    memcpy(ranges, consumerir_freqs, to_copy * sizeof(consumerir_freq_range_t));
    return to_copy;
}

static int consumerir_close(hw_device_t *dev)
{
    free(dev);
    close(fd);
    return 0;
}

/*
 * Generic device handling
 */
static int consumerir_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    if (strcmp(name, CONSUMERIR_TRANSMITTER) != 0) {
        return -EINVAL;
    }
    if (device == NULL) {
        ALOGE("NULL device on open");
        return -EINVAL;
    }

    consumerir_device_t *dev = malloc(sizeof(consumerir_device_t));
    memset(dev, 0, sizeof(consumerir_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = consumerir_close;

    dev->transmit = consumerir_transmit;
    dev->get_num_carrier_freqs = consumerir_get_num_carrier_freqs;
    dev->get_carrier_freqs = consumerir_get_carrier_freqs;

    *device = (hw_device_t*) dev;
    fd = open("/sys/class/sec/sec_ir/ir_send", O_RDWR);
    return 0;
}

static struct hw_module_methods_t consumerir_module_methods = {
    .open = consumerir_open,
};

consumerir_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = CONSUMERIR_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = CONSUMERIR_HARDWARE_MODULE_ID,
        .name               = "Consumer IR Module",
        .author             = "The CyanogenMod Project",
        .methods            = &consumerir_module_methods,
    },
};
