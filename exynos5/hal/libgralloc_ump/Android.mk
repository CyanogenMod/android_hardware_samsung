#
# Copyright (C) 2010 ARM Limited. All rights reserved.
#
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


LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libGLESv1_CM libGLES_mali libion
LOCAL_MODULE_TAGS := eng optional

# Include the UMP header files
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := \
	gralloc_module.cpp \
	alloc_device.cpp \
	framebuffer_device.cpp

# For now we override arch and ABI values to allow us to work with a
# libmali that has been forced to armv7, eventually we can let android
# pick for us

ifeq ($(TARGET_PRODUCT), armboard_v7a)
# Support for ARM platforms
LOCAL_CFLAGS:= -DALOG_TAG=\"gralloc\" -DGRALLOC_16_BITS -DSTANDARD_LINUX_SCREEN \
	-march=armv7-a \
	-mfloat-abi=softfp
LOCAL_MODULE := gralloc.default

else
#Default to goldfish
LOCAL_CFLAGS:= -DALOG_TAG=\"gralloc\" -DSTANDARD_LINUX_SCREEN \
	-march=armv7-a \
	-mfloat-abi=softfp \
	-DVITHAR_HACK

LOCAL_MODULE := gralloc.smdk5250
endif


include $(BUILD_SHARED_LIBRARY)
