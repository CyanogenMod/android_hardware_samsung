/*
 * Copyright (C) 2015 The CyanogenMod Project
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

import org.cyanogenmod.hardware.util.FileUtils;

import android.os.SystemProperties;
import android.text.TextUtils;

import java.io.File;

/**
 * Disable capacitive keypad.
 *
 * This is intended for use on devices in which the capacitive keys
 * can be fully disabled for replacement with a soft navbar. You
 * really should not be using this on a device with mechanical or
 * otherwise visible-when-inactive keys
 */
public class KeyDisabler {

    private static int enable_keydisabler = SystemProperties.getInt("ro.cm.hardware.keydisabler", 0);
    private static String KEYDISABLER_PATH = SystemProperties.get("ro.cm.hardware.keydisabler.path", "/sys/class/sec/sec_touchkey/keypad_enable");

    /**
     * Wether the device has capacitive keys.
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() {
        if (enable_keydisabler == 0) {
            return false;
        }

        File f = new File(KEYDISABLER_PATH);
        return f.exists();
    }

    /**
     * This method returns the current status.
     *
     * @return boolean Retruns true if the keypad is enabled, false otherwise.
     */
    public static boolean isActive() {
        if (TextUtils.equals(FileUtils.readOneLine(KEYDISABLER_PATH), "1")) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * This method disables the capacitive keys.
     *
     * @param status The new capacitive keypad state
     */
    public static boolean setActive(boolean state) {
        if (state == true) {
            return FileUtils.writeLine(KEYDISABLER_PATH, "1");
        }

        return FileUtils.writeLine(KEYDISABLER_PATH, "0");
    }
}
