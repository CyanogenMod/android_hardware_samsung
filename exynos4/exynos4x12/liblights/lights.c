/*
 * Copyright (C) 2012 The Android Open Source Project
 *                    The CyanogenMod Project
 *                    Daniel Hillenbrand <codeworkx@cyanogenmod.com>
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


#define LOG_TAG "lights"
//#define LOG_NDEBUG 0

#include <cutils/log.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

char const*const PANEL_FILE = "/sys/class/backlight/panel/brightness";
#ifndef EXYNOS4X12_TABLET
char const*const BUTTON_FILE = "/sys/class/sec/sec_touchkey/brightness";

char const*const LED_RED = "/sys/class/sec/led/led_r";
char const*const LED_GREEN = "/sys/class/sec/led/led_g";
char const*const LED_BLUE = "/sys/class/sec/led/led_b";
char const*const LED_BLINK = "/sys/class/sec/led/led_blink";
char const*const LED_BRIGHTNESS = "/sys/class/sec/led/led_br_lev";
#endif

#define MAX_WRITE_CMD 25

struct led_config {
    int red;
    int green;
    int blue;
    char blink[MAX_WRITE_CMD];
};

struct led_config g_BatteryStore;

void init_g_lock(void)
{
    pthread_mutex_init(&g_lock, NULL);
}

static int write_int(char const *path, int value)
{
    int fd;
    static int already_warned;

    already_warned = 0;

    ALOGV("write_int: path %s, value %d", path, value);
    fd = open(path, O_RDWR);

    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int read_int(char const *path)
{
    int fd;
    char buffer[2];

    fd = open(path, O_RDONLY);

    if (fd >= 0) {
        read(fd, buffer, 1);
    }
    close(fd);

    return atoi(buffer);
}

static int write_str(char const *path, const char* value)
{
    int fd;
    static int already_warned;

    already_warned = 0;

    ALOGV("write_str: path %s, value %s", path, value);
    fd = open(path, O_RDWR);

    if (fd >= 0) {
        char buffer[MAX_WRITE_CMD];
        int bytes = sprintf(buffer, "%s\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_str failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int rgb_to_brightness(struct light_state_t const *state)
{
    int color = state->color & 0x00ffffff;

    return ((77*((color>>16) & 0x00ff))
        + (150*((color>>8) & 0x00ff)) + (29*(color & 0x00ff))) >> 8;
}

static int get_dimmed_color(struct light_state_t const *state, int brightness)
{
    int red = (state->color >> 16) & 0xFF;
    int green = ((state->color >> 8) & 0xFF) * 0.7;
    int blue = (state->color & 0x00FF) * 0.7;

    return (((red * brightness) / 255) << 16) + (((green * brightness) / 255) << 8) + ((blue * brightness) / 255);
}

/* Panel backlight */
static int set_light_backlight(struct light_device_t *dev,
            struct light_state_t const *state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    int previous_brightness = read_int(PANEL_FILE);

    pthread_mutex_lock(&g_lock);
    err = write_int(PANEL_FILE, brightness);
    pthread_mutex_unlock(&g_lock);

    return err;
}

/* Touchkey backlight */
static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
#ifdef EXYNOS4X12_TABLET
    return 0;
#else
    int err = 0;
    int brightness = rgb_to_brightness(state);

    pthread_mutex_lock(&g_lock);
    ALOGD("set_light_buttons: %d\n", brightness > 0 ? 1 : 2);
    err = write_int(BUTTON_FILE, brightness > 0 ? 1 : 2);
    pthread_mutex_unlock(&g_lock);

    return err;
#endif
}

/* LEDs */
static int write_leds(struct led_config led)
{
    int err = 0;
#ifndef EXYNOS4X12_TABLET
    pthread_mutex_lock(&g_lock);
    err = write_int(LED_RED, led.red);
    err = write_int(LED_GREEN, led.green);
    err = write_int(LED_BLUE, led.blue);
    err = write_str(LED_BLINK, led.blink);
    pthread_mutex_unlock(&g_lock);
#endif
    return err;
}

static int set_light_leds(struct light_state_t const *state, int type)
{
#ifdef EXYNOS4X12_TABLET
    return 0;
#else

    struct led_config led;
    unsigned int colorRGB;

    colorRGB = get_dimmed_color(state, 200);

    switch (state->flashMode) {
    case LIGHT_FLASH_NONE:
            // use battery led state stored
            if (g_BatteryStore.blink == NULL) {
                led.red = 0;
                led.green = 0;
                led.blue = 0;
                snprintf(g_BatteryStore.blink, MAX_WRITE_CMD, "0x000000 0 0");
            }
            led = g_BatteryStore;
        break;
    case LIGHT_FLASH_TIMED:
    case LIGHT_FLASH_HARDWARE:
            led.red = (colorRGB >> 16) & 0xFF;
            led.green = (colorRGB >> 8) & 0xFF;
            led.blue = colorRGB & 0xFF;
            snprintf(led.blink, MAX_WRITE_CMD, "0x%x %d %d", colorRGB, state->flashOnMS, state->flashOffMS);
            ALOGD("set_light_leds 0x%x %d %d", colorRGB, state->flashOnMS, state->flashOffMS);
        break;
    default:
        return -EINVAL;
    }

    return write_leds(led);
#endif
}

static int set_light_leds_notifications(struct light_device_t *dev,
            struct light_state_t const *state)
{
    return set_light_leds(state, 0);
}

static int set_light_battery(struct light_device_t *dev,
            struct light_state_t const *state)
{
#ifdef EXYNOS4X12_TABLET
    return 0;
#else
    struct led_config led;
    int brightness = rgb_to_brightness(state);
    unsigned int colorRGB;

    colorRGB = get_dimmed_color(state, 20);

    if (brightness == 0) {
        led.red = 0;
        led.green = 0;
        led.blue = 0;
        snprintf(led.blink, MAX_WRITE_CMD, "0x000000 0 0");
    } else {
        led.red = (colorRGB >> 16) & 0xFF;
        led.green = (colorRGB >> 8) & 0xFF;
        led.blue = colorRGB & 0xFF;
        snprintf(led.blink, MAX_WRITE_CMD, "0x%x %d %d", colorRGB, state->flashOnMS, state->flashOffMS);
        ALOGD("set_light_battery 0x%x %d %d", colorRGB, state->flashOnMS, state->flashOffMS);
    }

    g_BatteryStore = led;
    return write_leds(led);
#endif
}

static int set_light_leds_attention(struct light_device_t *dev,
            struct light_state_t const *state)
{
    return set_light_leds(state, 1);
}

static int close_lights(struct light_device_t *dev)
{
    ALOGV("close_light is called");
    if (dev)
        free(dev);

    return 0;
}

static int open_lights(const struct hw_module_t *module, char const *name,
                        struct hw_device_t **device)
{
    int (*set_light)(struct light_device_t *dev,
        struct light_state_t const *state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_leds_notifications;
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_leds_attention;
    else if (0 == strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else
        return -EINVAL;

    pthread_once(&g_init, init_g_lock);

    g_BatteryStore.red = 0;
    g_BatteryStore.green = 0;
    g_BatteryStore.blue = 0;

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *)module;
    dev->common.close = (int (*)(struct hw_device_t *))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t *)dev;

    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Exynos4x12 lights Module",
    .author = "The CyanogenMod Project",
    .methods = &lights_module_methods,
};
