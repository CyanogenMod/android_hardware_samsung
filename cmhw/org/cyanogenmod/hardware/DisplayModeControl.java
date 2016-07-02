/*
 * Copyright (C) 2015-2016 The CyanogenMod Project
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

package org.cyanogenmod.hardware;

import cyanogenmod.hardware.DisplayMode;
import org.cyanogenmod.internal.util.FileUtils;

/*
 * Display Modes API
 *
 * A device may implement a list of preset display modes for different
 * viewing intents, such as movies, photos, or extra vibrance. These
 * modes may have multiple components such as gamma correction, white
 * point adjustment, etc, but are activated by a single control point.
 *
 * This API provides support for enumerating and selecting the
 * modes supported by the hardware.
 */

public class DisplayModeControl {

    private static final String MODE_PATH = "/sys/class/mdnie/mdnie/mode";
    private static final String DEFAULT_PATH = "/data/misc/.displaymodedefault";

    private static final DisplayMode[] DISPLAY_MODES = {
        new DisplayMode(0, "Dynamic"),
        new DisplayMode(1, "Standard"),
        new DisplayMode(2, "Natural"),
        new DisplayMode(3, "Cinema"),
        new DisplayMode(4, "Adaptive"),
    };

    static {
        if (FileUtils.isFileReadable(DEFAULT_PATH)) {
            setMode(getDefaultMode(), false);
        } else if (FileUtils.isFileReadable(MODE_PATH)) {
            /* If default mode is not set yet, set current mode as default */
            setMode(getCurrentMode(), true);
        }
    }

    /*
     * All HAF classes should export this boolean.
     * Real implementations must, of course, return true
     */
    public static boolean isSupported() {
        return FileUtils.isFileWritable(MODE_PATH) &&
                FileUtils.isFileReadable(MODE_PATH) &&
                FileUtils.isFileWritable(DEFAULT_PATH) &&
                FileUtils.isFileReadable(DEFAULT_PATH);
    }

    /*
     * Get the list of available modes. A mode has an integer
     * identifier and a string name.
     *
     * It is the responsibility of the upper layers to
     * map the name to a human-readable format or perform translation.
     */
    public static DisplayMode[] getAvailableModes() {
        return DISPLAY_MODES;
    }

    /*
     * Get the name of the currently selected mode. This can return
     * null if no mode is selected.
     */
    public static DisplayMode getCurrentMode() {
        try {
            int curMode = Integer.parseInt(FileUtils.readOneLine(MODE_PATH));
            return DISPLAY_MODES[curMode];
        } catch (NumberFormatException e) {
            return null;
        }
    }

    /*
     * Selects a mode from the list of available modes by it's
     * string identifier. Returns true on success, false for
     * failure. It is up to the implementation to determine
     * if this mode is valid.
     */
    public static boolean setMode(DisplayMode mode, boolean makeDefault) {
        boolean success = FileUtils.writeLine(MODE_PATH, String.valueOf(mode.id));
        if (success && makeDefault) {
            return FileUtils.writeLine(DEFAULT_PATH, String.valueOf(mode.id));
        }

        return success;
    }

    /*
     * Gets the preferred default mode for this device by it's
     * string identifier. Can return null if there is no default.
     */
    public static DisplayMode getDefaultMode() {
        try {
            int defMode = Integer.parseInt(FileUtils.readOneLine(DEFAULT_PATH));
            return DISPLAY_MODES[defMode];
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
