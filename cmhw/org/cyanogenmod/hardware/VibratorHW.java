/*
 * Copyright (C) 2013-2016 The CyanogenMod Project
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

import org.cyanogenmod.internal.util.FileUtils;

public class VibratorHW {

    private static final String LEVEL_PATH = "/sys/class/timed_output/vibrator/pwm_value";
    private static final String LEVEL_MAX_PATH = "/sys/class/timed_output/vibrator/pwm_max";
    private static final String LEVEL_MIN_PATH = "/sys/class/timed_output/vibrator/pwm_min";
    private static final String LEVEL_DEFAULT_PATH = "/sys/class/timed_output/vibrator/pwm_default";
    private static final String LEVEL_THRESHOLD_PATH = "/sys/class/timed_output/vibrator/pwm_threshold";

    public static boolean isSupported() {
        return FileUtils.isFileWritable(LEVEL_PATH) &&
                FileUtils.isFileReadable(LEVEL_PATH) &&
                FileUtils.isFileReadable(LEVEL_DEFAULT_PATH) &&
                FileUtils.isFileReadable(LEVEL_MAX_PATH) &&
                FileUtils.isFileReadable(LEVEL_MIN_PATH) &&
                FileUtils.isFileReadable(LEVEL_THRESHOLD_PATH);
    }

    public static int getMaxIntensity()  {
        return Integer.parseInt(FileUtils.readOneLine(LEVEL_MAX_PATH));
    }

    public static int getMinIntensity()  {
        return Integer.parseInt(FileUtils.readOneLine(LEVEL_MIN_PATH));
    }

    public static int getWarningThreshold()  {
        return Integer.parseInt(FileUtils.readOneLine(LEVEL_THRESHOLD_PATH));
    }

    public static int getCurIntensity()  {
        return Integer.parseInt(FileUtils.readOneLine(LEVEL_PATH));
    }

    public static int getDefaultIntensity()  {
        return Integer.parseInt(FileUtils.readOneLine(LEVEL_DEFAULT_PATH));
    }

    public static boolean setIntensity(int intensity)  {
        return FileUtils.writeLine(LEVEL_PATH, String.valueOf(intensity));
    }
}
