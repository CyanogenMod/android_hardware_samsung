# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng

LOCAL_SRC_FILES:= \
    secril-client.cpp

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy

LOCAL_CFLAGS := 

ifeq ($(BOARD_MODEM_TYPE),xmm7260)
LOCAL_CFLAGS += -DMODEM_TYPE_XMM7260
endif

LOCAL_MODULE:= libsecril-client
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
