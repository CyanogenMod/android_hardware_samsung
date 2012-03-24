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
include $(CLEAR_VARS)

UMP_SRCS := \
	arch_011_udd/ump_frontend.c \
	arch_011_udd/ump_ref_drv.c \
	arch_011_udd/ump_arch.c \
	os/linux/ump_uku.c \
	os/linux/ump_osu_memory.c \
	os/linux/ump_osu_locks.c

# Shared and static library for target
# ========================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libUMP
LOCAL_SRC_FILES := $(UMP_SRCS)

LOCAL_C_INCLUDES:= \
	$(BOARD_HAL_PATH)/libump/ \
	$(BOARD_HAL_PATH)/libump/include \

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libUMP
LOCAL_MODULE_TAGS := optional
LOCAL_WHOLE_STATIC_LIBRARIES := libUMP
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/
include $(BUILD_SHARED_LIBRARY)
