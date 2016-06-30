/*
 * Copyright (C) 2012 The CyanogenMod Project
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

package com.cyanogenmod.settings.device;

import android.content.Context;

import android.content.SharedPreferences;
import android.util.AttributeSet;
import android.preference.Preference;
import android.preference.ListPreference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceManager;

import org.cyanogenmod.internal.util.FileUtils;

public class mDNIeScenario extends ListPreference implements OnPreferenceChangeListener {

    private static String FILE = null;

    public mDNIeScenario(Context context, AttributeSet attrs) {
        super(context,attrs);
        this.setOnPreferenceChangeListener(this);
        FILE = context.getResources().getString(R.string.mdnie_scenario_sysfs_file);
    }

    public static boolean isSupported(String filePath) {
        return FileUtils.isFileWritable(filePath);
    }

    /**
     * Restore mdnie "camera" setting from SharedPreferences. (Write to kernel.)
     * @param context       The context to read the SharedPreferences from
     */
    public static void restore(Context context) {
        FILE = context.getResources().getString(R.string.mdnie_scenario_sysfs_file);
        if (!isSupported(FILE)) {
            return;
        }

        SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(context);
        FileUtils.writeLine(FILE, sharedPrefs.getString(DisplaySettings.KEY_MDNIE_SCENARIO, "0"));
    }

    public boolean onPreferenceChange(Preference preference, Object newValue) {
        FileUtils.writeLine(FILE, (String) newValue);
        return true;
    }

}
