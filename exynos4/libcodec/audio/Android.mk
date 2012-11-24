LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(BOARD_USE_ALP_AUDIO), true)
include $(LOCAL_PATH)/alp/Android.mk
endif
