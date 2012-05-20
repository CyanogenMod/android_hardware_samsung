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

ifeq ($(filter-out exynos4,$(TARGET_BOARD_PLATFORM)),)
ifeq ($(BOARD_USES_HDMI),true)

LOCAL_PATH:= $(call my-dir)

#
# libTVOut
#

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
	SecTVOutService.cpp \
	ISecTVOut.cpp \
	MessageQueue.cpp

LOCAL_C_INCLUDES := \

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libutils \
	libcutils

LOCAL_C_INCLUDES += $(TARGET_HAL_PATH)/include
LOCAL_C_INCLUDES += $(TARGET_HAL_PATH)/libhdmi
LOCAL_C_INCLUDES += $(TARGET_HAL_PATH)/libfimc
LOCAL_SHARED_LIBRARIES += libhdmi libfimc

ifeq ($(BOARD_USES_HDMI_SUBTITLES),true)
	LOCAL_CFLAGS  += -DBOARD_USES_HDMI_SUBTITLES
endif

ifeq ($(TARGET_SOC),exynos4210)
	LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4210
endif

ifeq ($(TARGET_SOC),exynos4x12)
	LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4x12
endif

LOCAL_CFLAGS     += -DBOARD_USES_HDMI

ifeq ($(BOARD_USE_V4L2),true)
	LOCAL_CFLAGS += -DBOARD_USE_V4L2
endif

ifeq ($(BOARD_USE_V4L2_ION),true)
	LOCAL_CFLAGS += -DBOARD_USE_V4L2_ION
endif

LOCAL_MODULE := libTVOut

include $(BUILD_SHARED_LIBRARY)

#
# libhdmiclient
#

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
    SecHdmiClient.cpp

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE)

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libutils \
	libTVOut

ifeq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_OS),linux)
ifeq ($(TARGET_ARCH),x86)
LOCAL_LDLIBS += -lpthread -ldl -lrt
endif
endif
endif

ifeq ($(WITH_MALLOC_LEAK_CHECK),true)
	LOCAL_CFLAGS += -DMALLOC_LEAK_CHECK
endif

ifeq ($(TARGET_SOC),exynos4210)
	LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4210
endif

ifeq ($(TARGET_SOC),exynos4x12)
	LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4x12
endif

LOCAL_CFLAGS     += -DBOARD_USES_HDMI

ifeq ($(BOARD_USE_V4L2),true)
	LOCAL_CFLAGS += -DBOARD_USE_V4L2
endif

ifeq ($(BOARD_USE_V4L2_ION),true)
	LOCAL_CFLAGS += -DBOARD_USE_V4L2_ION
endif

LOCAL_MODULE:= libhdmiclient

include $(BUILD_SHARED_LIBRARY)

endif
endif
