/*
 * Copyright (C) 2013 Xiao-Long Chen <chenxiaolong@cxl.epac.to>
 * Copyright (C) 2015 Franco Rapetti <frapeti@gmail.com>
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

/**
 * Touchscreen Hovering
 */
public class TouchscreenHovering {

    /**
     * Whether device supports touchscreen hovering.
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() { return false; }

    /**
     * This method return the current activation status of touchscreen hovering
     *
     * @return boolean Must be false if touchscreen hovering is not supported or not activated,
     * or the operation failed while reading the status; true in any other case.
     */
    public static boolean isEnabled() { return false; }

    /**
     * This method allows to setup touchscreen hovering status.
     *
     * @param status The new touchscreen hovering status
     * @return boolean Must be false if touchscreen hovering is not supported or the operation
     * failed; true in any other case.
     */
    public static boolean setEnabled(boolean status) { return false; }

}
