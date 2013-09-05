/*
 * Copyright (C) 2013 The CyanogenMod Project
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

import java.io.File;

public class VibratorHW {

    private static String LEVEL_PATH = "/sys/class/timed_output/vibrator/pwm_value";
    private static String LEVEL_MAX_PATH = "/sys/class/timed_output/vibrator/pwm_max";
    private static String LEVEL_MIN_PATH = "/sys/class/timed_output/vibrator/pwm_min";
    private static String LEVEL_DEFAULT_PATH = "/sys/class/timed_output/vibrator/pwm_default";
    private static String LEVEL_THRESHOLD_PATH = "/sys/class/timed_output/vibrator/pwm_threshold";

    public static boolean isSupported() {
        File f = new File(LEVEL_PATH);

        if(f.exists()) {
            return true;
        } else {
            return false;
        }
    }

    public static int getMaxIntensity()  {
        File f = new File(LEVEL_MAX_PATH);

        if(f.exists()) {
            return Integer.parseInt(FileUtils.readOneLine(LEVEL_MAX_PATH));
        } else {
            return 100;
        }
    }

    public static int getMinIntensity()  {
        File f = new File(LEVEL_MIN_PATH);

        if(f.exists()) {
            return Integer.parseInt(FileUtils.readOneLine(LEVEL_MIN_PATH));
        } else {
            return 0;
        }
    }

    public static int getWarningThreshold()  {
        File f = new File(LEVEL_THRESHOLD_PATH);

        if(f.exists()) {
            return Integer.parseInt(FileUtils.readOneLine(LEVEL_THRESHOLD_PATH));
        } else {
            return 75;
        }
    }

    public static int getCurIntensity()  {
        File f = new File(LEVEL_PATH);

        if(f.exists()) {
            return Integer.parseInt(FileUtils.readOneLine(LEVEL_PATH));
        } else {
            return 0;
        }
    }

    public static int getDefaultIntensity()  {
        File f = new File(LEVEL_DEFAULT_PATH);

        if(f.exists()) {
            return Integer.parseInt(FileUtils.readOneLine(LEVEL_DEFAULT_PATH));
        } else {
            return 50;
        }
    }

    public static boolean setIntensity(int intensity)  {
        File f = new File(LEVEL_PATH);

        if(f.exists()) {
            return FileUtils.writeLine(LEVEL_PATH, String.valueOf(intensity));
        } else {
            return false;
        }
    }
}
