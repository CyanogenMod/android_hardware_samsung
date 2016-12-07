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

#ifndef LIGHTS_HELPER_H
#define LIGHTS_HELPER_H

/*
 * Data structures for lights HAL
 *
 */
struct backlight_config {
    int cur_brightness, max_brightness;
};

struct led_config {
    unsigned int color;
    int delay_on, delay_off;
};

/*
 * Allow other modules to access our static data
 *
 * Currently we expose panel cur/max brightness.
 *
 */
int get_cur_panel_brightness();
int get_max_panel_brightness();
void set_cur_panel_brightness(int brightness);
void set_max_panel_brightness(int brightness);

#endif // LIGHTS_HELPER_H
