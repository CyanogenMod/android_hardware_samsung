/*
 * Copyright (C) 2014, 2016 The CyanogenMod Project
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

import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.util.Slog;

import org.cyanogenmod.internal.util.FileUtils;

public class DisplayColorCalibration {

    private static final String TAG = "DisplayColorCalibration";

    private static final String RED_FILE = "/sys/class/mdnie/mdnie/r_adj";
    private static final String GREEN_FILE = "/sys/class/mdnie/mdnie/g_adj";
    private static final String BLUE_FILE = "/sys/class/mdnie/mdnie/b_adj";

    private static final boolean sUseGPUMode;

    private static final int MIN = 1;
    private static final int MAX = 255;

    private static final int[] sCurColors = new int[] { MAX, MAX, MAX };

    static {
        // We can also support GPU transform using RenderEngine. This is not
        // preferred though, as it has a high power cost.
        sUseGPUMode = !FileUtils.isFileWritable(RED_FILE) ||
                SystemProperties.getBoolean("debug.livedisplay.force_gpu", false);
    }

    public static boolean isSupported() {
        // Always true, use GPU mode if no HW support
        return true;
    }

    public static int getMaxValue()  {
        return MAX;
    }

    public static int getMinValue()  {
        return MIN;
    }

    public static int getDefValue() {
        return getMaxValue();
    }

    public static String getCurColors()  {
        if (!sUseGPUMode) {
            return String.format("%s %s %s",
                    FileUtils.readOneLine(RED_FILE),
                    FileUtils.readOneLine(GREEN_FILE),
                    FileUtils.readOneLine(BLUE_FILE));
        }

        return String.format("%d %d %d", sCurColors[0],
                sCurColors[1], sCurColors[2]);
    }

    public static boolean setColors(String colors) {
        if (!sUseGPUMode) {
            String[] array = colors.split(" ");
            return FileUtils.writeLine(RED_FILE, array[0]) &&
                FileUtils.writeLine(GREEN_FILE, array[1]) &&
                FileUtils.writeLine(BLUE_FILE, array[2]);
        }

        float[] mat = toColorMatrix(colors);

        // set to null if identity
        if (mat == null ||
                (mat[0] == 1.0f && mat[5] == 1.0f &&
                 mat[10] == 1.0f && mat[15] == 1.0f)) {
            return setColorTransform(null);
        }
        return setColorTransform(mat);
    }

    private static float[] toColorMatrix(String rgbString) {
        String[] adj = rgbString == null ? null : rgbString.split(" ");

        if (adj == null || adj.length != 3) {
            return null;
        }

        float[] mat = new float[16];

        // sanity check
        for (int i = 0; i < 3; i++) {
            int v = Integer.parseInt(adj[i]);

            if (v >= MAX) {
                v = MAX;
            } else if (v < MIN) {
                v = MIN;
            }

            mat[i * 5] = (float)v / (float)MAX;
            sCurColors[i] = v;
        }

        mat[15] = 1.0f;
        return mat;
    }

    /**
     * Sets the surface flinger's color transformation as a 4x4 matrix. If the
     * matrix is null, color transformations are disabled.
     *
     * @param m the float array that holds the transformation matrix, or null to
     *            disable transformation
     */
    private static boolean setColorTransform(float[] m) {
        try {
            final IBinder flinger = ServiceManager.getService("SurfaceFlinger");
            if (flinger != null) {
                final Parcel data = Parcel.obtain();
                data.writeInterfaceToken("android.ui.ISurfaceComposer");
                if (m != null) {
                    data.writeInt(1);
                    for (int i = 0; i < 16; i++) {
                        data.writeFloat(m[i]);
                    }
                } else {
                    data.writeInt(0);
                }
                flinger.transact(1030, data, null, 0);
                data.recycle();
            }
        } catch (RemoteException ex) {
            Slog.e(TAG, "Failed to set color transform", ex);
            return false;
        }
        return true;
    }

}
