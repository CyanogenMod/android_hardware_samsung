/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#ifndef SAMSUNG_LIGHTS_HELPER_H
#define SAMSUNG_LIGHTS_HELPER_H

#include <samsung_lights.h>

/*
 * Prototypes
 */
int read_int(char const *path);
int write_int(char const *path, const int value);

/*
 * Interface for other modules accessing panel brightness data
 */
static inline int get_cur_panel_brightness()
{
    return read_int(PANEL_BRIGHTNESS_NODE);
}

static inline int get_max_panel_brightness()
{
    return read_int(PANEL_MAX_BRIGHTNESS_NODE);
}

static inline void set_cur_panel_brightness(const int brightness)
{
    write_int(PANEL_BRIGHTNESS_NODE, brightness);
}

static inline void set_max_panel_brightness(const int brightness)
{
    write_int(PANEL_MAX_BRIGHTNESS_NODE, brightness);
}

#endif // SAMSUNG_LIGHTS_HELPER_H
