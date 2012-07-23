/*
**
** Copyright 2008, The Android Open Source Project
** Copyright 2010, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_SEC_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_SEC_H

#include "SecCamera.h"
#include <utils/threads.h>
#include <utils/RefBase.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <hardware/gralloc.h>
#include <camera/CameraParameters.h>
#ifdef BOARD_USE_V4L2_ION
#include <binder/MemoryHeapBaseIon.h>
#include "gralloc_priv.h"

#define  BUFFER_COUNT_FOR_GRALLOC (MAX_BUFFERS + 4)
#define  BUFFER_COUNT_FOR_ARRAY (MAX_BUFFERS)
#else
#define  BUFFER_COUNT_FOR_GRALLOC (MAX_BUFFERS)
#define  BUFFER_COUNT_FOR_ARRAY (1)
#endif

namespace android {
    class CameraHardwareSec : public virtual RefBase {
public:
    virtual void        setCallbacks(camera_notify_callback notify_cb,
                                     camera_data_callback data_cb,
                                     camera_data_timestamp_callback data_cb_timestamp,
                                     camera_request_memory get_memory,
                                     void *user);

    virtual void        enableMsgType(int32_t msgType);
    virtual void        disableMsgType(int32_t msgType);
    virtual bool        msgTypeEnabled(int32_t msgType);

    virtual status_t    startPreview();
    virtual void        stopPreview();
    virtual bool        previewEnabled();

    virtual status_t    startRecording();
    virtual void        stopRecording();
    virtual bool        recordingEnabled();
    virtual void        releaseRecordingFrame(const void *opaque);

    virtual status_t    autoFocus();
    virtual status_t    cancelAutoFocus();
    virtual status_t    takePicture();
    virtual status_t    cancelPicture();
    virtual status_t    dump(int fd) const;
    virtual status_t    setParameters(const CameraParameters& params);
    virtual CameraParameters  getParameters() const;
    virtual status_t    sendCommand(int32_t command, int32_t arg1, int32_t arg2);
    virtual status_t    setPreviewWindow(preview_stream_ops *w);
    virtual status_t    storeMetaDataInBuffers(bool enable);
    virtual void        release();

    inline  int         getCameraId() const;

    CameraHardwareSec(int cameraId, camera_device_t *dev);
    virtual             ~CameraHardwareSec();
private:
    status_t    startPreviewInternal();
    void stopPreviewInternal();

    class PreviewThread : public Thread {
        CameraHardwareSec *mHardware;
    public:
        PreviewThread(CameraHardwareSec *hw):
        Thread(false),
        mHardware(hw) { }
        virtual void onFirstRef() {
            run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);
        }
        virtual bool threadLoop() {
            mHardware->previewThreadWrapper();
            return false;
        }
    };

    class PictureThread : public Thread {
        CameraHardwareSec *mHardware;
    public:
        PictureThread(CameraHardwareSec *hw):
        Thread(false),
        mHardware(hw) { }
        virtual bool threadLoop() {
            mHardware->pictureThread();
            return false;
        }
    };

    class AutoFocusThread : public Thread {
        CameraHardwareSec *mHardware;
    public:
        AutoFocusThread(CameraHardwareSec *hw): Thread(false), mHardware(hw) { }
        virtual void onFirstRef() {
            run("CameraAutoFocusThread", PRIORITY_DEFAULT);
        }
        virtual bool threadLoop() {
            mHardware->autoFocusThread();
            return true;
        }
    };

#ifdef IS_FW_DEBUG
    class DebugThread : public Thread {
        CameraHardwareSec *mHardware;
    public:
        DebugThread(CameraHardwareSec *hw):
        Thread(false),
        mHardware(hw) { }
        virtual bool threadLoop() {
            return mHardware->debugThread();
        }
    };
#endif

            void        initDefaultParameters(int cameraId);
            void        initHeapLocked();

    sp<PreviewThread>   mPreviewThread;
            int         previewThread();
            int         previewThreadWrapper();

    sp<AutoFocusThread> mAutoFocusThread;
            int         autoFocusThread();

    sp<PictureThread>   mPictureThread;
            int         pictureThread();
            bool        mCaptureInProgress;

#ifdef IS_FW_DEBUG
    sp<DebugThread>     mDebugThread;
            bool        debugThread();
            bool        mDebugRunning;
            int         mPrevOffset;
            int         mCurrOffset;
            int         mPrevWp;
            int         mCurrWp;
   unsigned int         mDebugVaddr;
#endif

            int         save_jpeg(unsigned char *real_jpeg, int jpeg_size);
            void        save_postview(const char *fname, uint8_t *buf,
                                        uint32_t size);
            int         decodeInterleaveData(unsigned char *pInterleaveData,
                                                int interleaveDataSize,
                                                int yuvWidth,
                                                int yuvHeight,
                                                int *pJpegSize,
                                                void *pJpegData,
                                                void *pYuvData);
            bool        YUY2toNV21(void *srcBuf, void *dstBuf, uint32_t srcWidth, uint32_t srcHeight);
            bool        scaleDownYuv422(char *srcBuf, uint32_t srcWidth,
                                        uint32_t srcHight, char *dstBuf,
                                        uint32_t dstWidth, uint32_t dstHight);

            bool        CheckVideoStartMarker(unsigned char *pBuf);
            bool        CheckEOIMarker(unsigned char *pBuf);
            bool        FindEOIMarkerInJPEG(unsigned char *pBuf,
                                            int dwBufSize, int *pnJPEGsize);
            bool        SplitFrame(unsigned char *pFrame, int dwSize,
                                   int dwJPEGLineLength, int dwVideoLineLength,
                                   int dwVideoHeight, void *pJPEG,
                                   int *pdwJPEGSize, void *pVideo,
                                   int *pdwVideoSize);
            void        setSkipFrame(int frame);
            bool        isSupportedPreviewSize(const int width,
                                               const int height) const;
            bool        getVideosnapshotSize(int *width, int *height);

    /* used by auto focus thread to block until it's told to run */
    mutable Mutex       mFocusLock;
    mutable Condition   mFocusCondition;
            bool        mExitAutoFocusThread;
            int         mTouched;

    /* used by preview thread to block until it's told to run */
    mutable Mutex       mPreviewLock;
    mutable Condition   mPreviewCondition;
    mutable Condition   mPreviewStoppedCondition;
            bool        mPreviewRunning;
            bool        mPreviewStartDeferred;
            bool        mExitPreviewThread;

            preview_stream_ops *mPreviewWindow;

    /* used to guard threading state */
    mutable Mutex       mStateLock;

    enum PREVIEW_FMT {
        PREVIEW_FMT_1_PLANE = 0,
        PREVIEW_FMT_2_PLANE,
        PREVIEW_FMT_3_PLANE,
    };

            int         mPreviewFmtPlane;

    CameraParameters    mParameters;
    CameraParameters    mInternalParameters;

    int                 mFrameSizeDelta;
    camera_memory_t     *mPreviewHeap;
    camera_memory_t     *mRawHeap;
    sp<MemoryHeapBase>  mPostviewHeap[CAP_BUFFERS];
    sp<MemoryHeapBase>  mThumbnailHeap;
    camera_memory_t     *mRecordHeap[BUFFER_COUNT_FOR_ARRAY];

    camera_frame_metadata_t     *mFaceData;
    camera_memory_t     *mFaceDataHeap;

    buffer_handle_t *mBufferHandle[BUFFER_COUNT_FOR_ARRAY];
    int mStride[BUFFER_COUNT_FOR_ARRAY];


    SecCamera           *mSecCamera;
            const __u8  *mCameraSensorName;
            bool        mUseInternalISP;

    mutable Mutex       mSkipFrameLock;
            int         mSkipFrame;

    camera_notify_callback     mNotifyCb;
    camera_data_callback       mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    camera_request_memory      mGetMemoryCb;
    void   *mCallbackCookie;

            int32_t     mMsgEnabled;

            bool        mRecordRunning;
            bool        mRecordHint;
    mutable Mutex       mRecordLock;
            int         mPostViewWidth;
            int         mPostViewHeight;
            int         mPostViewSize;
     struct SecBuffer   mCapBuffer;
            int         mCapIndex;
            int         mCameraID;

            Vector<Size> mSupportedPreviewSizes;

    struct is_debug_control {
        unsigned int write_point;        /* 0 ~ 500KB boundary */
        unsigned int assert_flag;        /* 0: Not invoked, 1: Invoked */
        unsigned int pabort_flag;        /* 0: Not invoked, 1: Invoked */
        unsigned int dabort_flag;        /* 0: Not invoked, 1: Invoked */
    };
    struct is_debug_control mIs_debug_ctrl;

    camera_device_t *mHalDevice;
    static gralloc_module_t const* mGrallocHal;
};

}; // namespace android

#endif
