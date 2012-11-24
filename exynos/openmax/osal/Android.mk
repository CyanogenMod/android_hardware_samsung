LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	Exynos_OSAL_Android.cpp \
	Exynos_OSAL_Event.c \
	Exynos_OSAL_Queue.c \
	Exynos_OSAL_ETC.c \
	Exynos_OSAL_Mutex.c \
	Exynos_OSAL_Thread.c \
	Exynos_OSAL_Memory.c \
	Exynos_OSAL_Semaphore.c \
	Exynos_OSAL_Library.c \
	Exynos_OSAL_Log.c \
	Exynos_OSAL_SharedMemory.c

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libExynosOMX_OSAL

LOCAL_CFLAGS :=

ifeq ($(BOARD_USE_ANB_OUTBUF_SHARE), true)
LOCAL_CFLAGS += -DUSE_ANB_OUTBUF_SHARE
endif

ifeq ($(BOARD_USE_DMA_BUF), true)
LOCAL_CFLAGS += -DUSE_DMA_BUF
endif

LOCAL_STATIC_LIBRARIES := liblog libcutils libExynosVideoApi

LOCAL_C_INCLUDES := \
	$(EXYNOS_OMX_INC)/exynos \
	$(EXYNOS_OMX_TOP)/osal \
	$(EXYNOS_OMX_COMPONENT)/common \
	$(EXYNOS_OMX_COMPONENT)/video/dec \
	$(EXYNOS_VIDEO_CODEC)/v4l2/include \
	$(TOP)/hardware/samsung/exynos/include \
	$(TOP)/hardware/samsung/exynos4/include

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), true)
LOCAL_CFLAGS += -DUSE_KHRONOS_OMX_HEADER
LOCAL_C_INCLUDES += $(EXYNOS_OMX_INC)/khronos
else
LOCAL_C_INCLUDES += $(ANDROID_MEDIA_INC)/hardware
LOCAL_C_INCLUDES += $(ANDROID_MEDIA_INC)/openmax
endif

include $(BUILD_STATIC_LIBRARY)
