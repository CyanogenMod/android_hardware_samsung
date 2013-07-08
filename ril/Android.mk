#
# Copyright (C) 2013 The Android Open-Source Project
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
#

LOCAL_PATH := $(call my-dir)

ifeq ($(BOARD_VENDOR),samsung)

# libril
ifeq ($(BOARD_PROVIDES_LIBRIL),true)
ifeq ($(BOARD_MODEM_TYPE),xmm6260)
include $(LOCAL_PATH)/xmm6260/libril/Android.mk
endif
ifeq ($(BOARD_MODEM_TYPE),xmm6262)
include $(LOCAL_PATH)/xmm6262/libril/Android.mk
endif
endif

# ril client
client_dirs := libsecril-client libsecril-client-sap
include $(call all-named-subdir-makefiles,$(client_dirs))

endif

