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
    libprotobuf-c-nano-enable_malloc \

ifneq ($(filter xmm6262 xmm6360,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS := -DMODEM_TYPE_XMM6262
endif
ifeq ($(BOARD_MODEM_TYPE),xmm6260)
LOCAL_CFLAGS := -DMODEM_TYPE_XMM6260
endif
ifneq ($(filter m7450 mdm9x35 ss333 xmm7260,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS := -DSAMSUNG_NEXT_GEN_MODEM
endif

ifneq ($(filter m7450 xmm7260,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS += -DNEEDS_VIDEO_CALL_FIELD
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
    librilutils_static \
    libprotobuf-c-nano-enable_malloc

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
