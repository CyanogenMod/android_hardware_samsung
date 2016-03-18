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

import android.app.Activity;
import android.app.ActionBar;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.content.Context;
import android.os.Bundle;
import android.view.MenuItem;
import android.widget.FrameLayout;

import com.cyanogenmod.settings.device.R;

import java.util.ArrayList;

public class DisplaySettings extends Activity {

    public static final String SHARED_PREFERENCES_BASENAME = "com.cyanogenmod.settings.device";
    public static final String ACTION_UPDATE_PREFERENCES = "com.cyanogenmod.settings.device.UPDATE";
    public static final String KEY_MDNIE_SCENARIO = "mdnie_scenario";
    public static final String KEY_MDNIE_MODE = "mdnie_mode";
    public static final String KEY_MDNIE_NEGATIVE = "mdnie_negative";

    ScreenFragmentActivity mFragment;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mFragment = new ScreenFragmentActivity();
        setContentView(R.layout.framelayout);
        getFragmentManager().beginTransaction().add(R.id.frameLayout, mFragment).commit();

        final ActionBar bar = getActionBar();
        bar.setTitle(R.string.app_name);
        bar.setDisplayHomeAsUpEnabled(true);

    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case android.R.id.home:
            DisplaySettings.this.onBackPressed();
        default:
            return super.onOptionsItemSelected(item);
        }
    }
}
