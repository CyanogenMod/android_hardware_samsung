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

LOCAL_CFLAGS := -fno-short-enums
LOCAL_CFLAGS += -DLOG_TAG=\"hdmi.$(TARGET_BOARD_PLATFORM)\"

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := \
    fimc.c \
    fimd.c \
    SecHDMI.cpp \
    hal_module.cpp

LOCAL_MODULE := hdmi.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := liblog libutils
 
include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
