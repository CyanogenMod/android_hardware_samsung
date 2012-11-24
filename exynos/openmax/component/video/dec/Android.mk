LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	Exynos_OMX_VdecControl.c \
	Exynos_OMX_Vdec.c

LOCAL_MODULE := libExynosOMX_Vdec
LOCAL_ARM_MODE := arm
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES := \
	$(EXYNOS_OMX_INC)/exynos \
	$(EXYNOS_OMX_TOP)/osal \
	$(EXYNOS_OMX_TOP)/core \
	$(EXYNOS_OMX_COMPONENT)/common \
	$(EXYNOS_OMX_COMPONENT)/video/dec \
	$(EXYNOS_VIDEO_CODEC)/v4l2/include \
	$(TOP)/hardware/samsung/exynos/include \
	$(TOP)/hardware/samsung/exynos4/include

LOCAL_STATIC_LIBRARIES := libExynosVideoApi

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), true)
LOCAL_CFLAGS += -DUSE_KHRONOS_OMX_HEADER
LOCAL_C_INCLUDES += $(EXYNOS_OMX_INC)/khronos
else
LOCAL_C_INCLUDES += $(ANDROID_MEDIA_INC)/openmax
endif

ifeq ($(BOARD_USE_ANB), true)
LOCAL_STATIC_LIBRARIES += libExynosOMX_OSAL libcsc_helper
LOCAL_CFLAGS += -DUSE_ANB
endif

ifeq ($(BOARD_USE_DMA_BUF), true)
LOCAL_CFLAGS += -DUSE_DMA_BUF
endif

include $(BUILD_STATIC_LIBRARY)
