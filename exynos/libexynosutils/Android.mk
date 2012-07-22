# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils

LOCAL_CFLAGS += -DEXYNOS_PLATFORM_ON_ANDROID

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += frameworks/native/include
LOCAL_C_INCLUDES += frameworks/av/include
LOCAL_C_INCLUDES += $(TARGET_HAL_PATH)/include

LOCAL_SRC_FILES := ExynosMutex.cpp

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libexynosutils

ifeq ($(TARGET_BOARD_PLATFORM), exynos4)
ifeq ($(BOARD_USE_V4L2), true)
LOCAL_SRC_FILES += exynos4_format_v4l2.c
else
LOCAL_SRC_FILES += exynos4_format.c
endif
endif

ifeq ($(TARGET_BOARD_PLATFORM), exynos5)
LOCAL_SRC_FILES += exynos5_format_v4l2.c
endif

include $(BUILD_SHARED_LIBRARY)
