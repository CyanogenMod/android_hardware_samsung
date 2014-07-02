# Copyright 2006 The Android Open Source Project

ifeq ($(BOARD_PROVIDES_LIBRIL),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ril.cpp \
    ril_event.cpp\
    RilSocket.cpp \
    RilSapSocket.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy \
    librilutils \

LOCAL_STATIC_LIBRARIES := \
    libnanopb-c-2.8.0-enable_malloc \

ifneq ($(filter xmm6262 xmm6360,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS := -DMODEM_TYPE_XMM6262
endif
ifeq ($(BOARD_MODEM_TYPE),xmm6260)
LOCAL_CFLAGS := -DMODEM_TYPE_XMM6260
endif
ifeq ($(BOARD_MODEM_TYPE),xmm7260)
LOCAL_CFLAGS := -DMODEM_TYPE_XMM7260
endif
ifeq ($(BOARD_MODEM_TYPE),m7450)
LOCAL_CFLAGS := -DMODEM_TYPE_M7450
endif

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADER)/librilutils
LOCAL_C_INCLUDES += external/nanopb-c

LOCAL_MODULE:= libril

LOCAL_COPY_HEADERS_TO := libril
LOCAL_COPY_HEADERS := ril_ex.h

include $(BUILD_SHARED_LIBRARY)

# For RdoServD which needs a static library
# =========================================
ifneq ($(ANDROID_BIONIC_TRANSITION),)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ril.cpp

LOCAL_STATIC_LIBRARIES := \
    libutils_static \
    libcutils \
    librilutils_static

ifneq ($(filter xmm6262 xmm6360,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS := -DMODEM_TYPE_XMM6262
endif
ifeq ($(BOARD_MODEM_TYPE),xmm6260)
LOCAL_CFLAGS := -DMODEM_TYPE_XMM6260
endif

LOCAL_MODULE:= libril_static

include $(BUILD_STATIC_LIBRARY)
endif # ANDROID_BIONIC_TRANSITION
endif # BOARD_PROVIDES_LIBRIL
