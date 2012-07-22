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

#ifndef ANDROID_HARDWARE_CAMERA_SEC_H
#define ANDROID_HARDWARE_CAMERA_SEC_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <utils/RefBase.h>
#include <videodev2.h>
#include <videodev2_samsung.h>
#include <linux/vt.h>

#include <media.h>
#include <v4l2-mediabus.h>
#include <v4l2-subdev.h>

#include <sec_utils_v4l2.h>

#include "SecBuffer.h"

#include <mediactl.h>
#include <v4l2subdev.h>

#include <utils/String8.h>

#include "SecJpegEncoder.h"

#include "Exif.h"
namespace android {

//#define ENABLE_ESD_PREVIEW_CHECK

#if defined(ALOG_NDEBUG) && (ALOG_NDEBUG == 0)
#define LOG_CAMERA ALOGD
#define LOG_CAMERA_PREVIEW ALOGD

#define LOG_TIME_DEFINE(n) \
    struct timeval time_start_##n, time_stop_##n; unsigned long log_time_##n = 0;

#define LOG_TIME_START(n) \
    gettimeofday(&time_start_##n, NULL);

#define LOG_TIME_END(n) \
    gettimeofday(&time_stop_##n, NULL); log_time_##n = measure_time_camera(&time_start_##n, &time_stop_##n);

#define LOG_TIME(n) \
    log_time_##n

#else
#define LOG_CAMERA(...)
#define LOG_CAMERA_PREVIEW(...)
#define LOG_TIME_DEFINE(n)
#define LOG_TIME_START(n)
#define LOG_TIME_END(n)
#define LOG_TIME(n)
#endif

#define JOIN(x, y) JOIN_AGAIN(x, y)
#define JOIN_AGAIN(x, y) x ## y

#define FRONT_CAM M5MO
#define BACK_CAM S5K4E5

#if !defined (FRONT_CAM) || !defined(BACK_CAM)
#error "Please define the Camera module"
#endif

#define M5MO_PREVIEW_WIDTH                  1920
#define M5MO_PREVIEW_HEIGHT                 1080
#define M5MO_SNAPSHOT_WIDTH                 3264
#define M5MO_SNAPSHOT_HEIGHT                2448

#define M5MO_THUMBNAIL_WIDTH                320
#define M5MO_THUMBNAIL_HEIGHT               240
#define M5MO_THUMBNAIL_BPP                  16

#define M5MO_FPS                            30

/* focal length of 3.43mm */
#define M5MO_FOCAL_LENGTH                   343

#define S5K4E5_PREVIEW_WIDTH                1920
#define S5K4E5_PREVIEW_HEIGHT               1080
#define S5K4E5_SNAPSHOT_WIDTH               1920
#define S5K4E5_SNAPSHOT_HEIGHT              1080

#define S5K4E5_THUMBNAIL_WIDTH              320
#define S5K4E5_THUMBNAIL_HEIGHT             240
#define S5K4E5_THUMBNAIL_BPP                16

#define S5K4E5_FPS                          30

/* focal length of 0.9mm */
#define S5K4E5_FOCAL_LENGTH                 90

#define MAX_BACK_CAMERA_PREVIEW_WIDTH       JOIN(BACK_CAM,_PREVIEW_WIDTH)
#define MAX_BACK_CAMERA_PREVIEW_HEIGHT      JOIN(BACK_CAM,_PREVIEW_HEIGHT)
#define MAX_BACK_CAMERA_SNAPSHOT_WIDTH      JOIN(BACK_CAM,_SNAPSHOT_WIDTH)
#define MAX_BACK_CAMERA_SNAPSHOT_HEIGHT     JOIN(BACK_CAM,_SNAPSHOT_HEIGHT)

#define BACK_CAMERA_THUMBNAIL_WIDTH         JOIN(BACK_CAM,_THUMBNAIL_WIDTH)
#define BACK_CAMERA_THUMBNAIL_HEIGHT        JOIN(BACK_CAM,_THUMBNAIL_HEIGHT)
#define BACK_CAMERA_THUMBNAIL_BPP           JOIN(BACK_CAM,_THUMBNAIL_BPP)

#define BACK_CAMERA_FPS                     JOIN(BACK_CAM,_FPS)

#define BACK_CAMERA_FOCAL_LENGTH            JOIN(BACK_CAM,_FOCAL_LENGTH)

#define MAX_FRONT_CAMERA_PREVIEW_WIDTH      JOIN(FRONT_CAM,_PREVIEW_WIDTH)
#define MAX_FRONT_CAMERA_PREVIEW_HEIGHT     JOIN(FRONT_CAM,_PREVIEW_HEIGHT)
#define MAX_FRONT_CAMERA_SNAPSHOT_WIDTH     JOIN(FRONT_CAM,_SNAPSHOT_WIDTH)
#define MAX_FRONT_CAMERA_SNAPSHOT_HEIGHT    JOIN(FRONT_CAM,_SNAPSHOT_HEIGHT)

#define FRONT_CAMERA_THUMBNAIL_WIDTH        JOIN(FRONT_CAM,_THUMBNAIL_WIDTH)
#define FRONT_CAMERA_THUMBNAIL_HEIGHT       JOIN(FRONT_CAM,_THUMBNAIL_HEIGHT)
#define FRONT_CAMERA_THUMBNAIL_BPP          JOIN(FRONT_CAM,_THUMBNAIL_BPP)

#define FRONT_CAMERA_FPS                    JOIN(FRONT_CAM,_FPS)

#define FRONT_CAMERA_FOCAL_LENGTH           JOIN(FRONT_CAM,_FOCAL_LENGTH)

#define DEFAULT_JPEG_THUMBNAIL_WIDTH        256
#define DEFAULT_JPEG_THUMBNAIL_HEIGHT       192

#define PFX_NODE_GSC                        "/dev/video"

#define M5MOLS_ENTITY_NAME                  "M5MOLS 5-001f"
#define PFX_SUBDEV_ENTITY_MIPI_CSIS         "s5p-mipi-csis"
#define PFX_SUBDEV_ENTITY_FLITE             "exynos-fimc-lite"
#define PFX_SUBDEV_ENTITY_GSC_CAP           "gsc-cap-subdev"
#define PFX_VIDEODEV_ENTITY_GSC_CAP         "exynos-gsc"
#define FIMD1_ENTITY_NAME                   "s5p-fimd1"

#define GAIA_FW_BETA                        1

#ifndef GAIA_FW_BETA
#define GSC_VD_NODE_OFFSET                  25         //GSCALER 0 (0:25, 1:28, 2:31, 3:34)
#else
#define GSC_VD_NODE_OFFSET                  41         //INTERNAL_ISP 4E5

#define ISP_SENSOR_MAX_ENTITIES             1
#define ISP_SENSOR_PAD_SOURCE_FRONT         0
#define ISP_SENSOR_PADS_NUM                 1

#define ISP_FRONT_MAX_ENTITIES              1
#define ISP_FRONT_PAD_SINK                  0
#define ISP_FRONT_PAD_SOURCE_BACK           1
#define ISP_FRONT_PAD_SOURCE_BAYER          2
#define ISP_FRONT_PAD_SOURCE_SCALERC        3
#define ISP_FRONT_PADS_NUM                  4

#define ISP_BACK_MAX_ENTITIES               1
#define ISP_BACK_PAD_SINK                   0
#define ISP_BACK_PAD_SOURCE_3DNR            1
#define ISP_BACK_PAD_SOURCE_SCALERP         2
#define ISP_BACK_PADS_NUM                   3

#define ISP_MODULE_NAME                     "exynos5-fimc-is"
#define ISP_SENSOR_ENTITY_NAME              "exynos5-fimc-is-sensor"
#define ISP_FRONT_ENTITY_NAME               "exynos5-fimc-is-front"
#define ISP_BACK_ENTITY_NAME                "exynos5-fimc-is-back"
#define ISP_VIDEO_BAYER_NAME                "exynos5-fimc-is-bayer"
#define ISP_VIDEO_SCALERC_NAME              "exynos5-fimc-is-scalerc"
#define ISP_VIDEO_3DNR_NAME                 "exynos5-fimc-is-3dnr"
#define ISP_VIDEO_SCALERP_NAME              "exynos5-fimc-is-scalerp"

#endif
#define MIPI_NUM                            1
#define FLITE_NUM                           1
#define GSC_NUM                             0

#define PFX_SUBDEV_NODE                     "/dev/v4l-subdev"

#define BPP             2
#define MIN(x, y)       (((x) < (y)) ? (x) : (y))
#ifndef GAIA_FW_BETA
#define MAX_BUFFERS     8
#else
#define MAX_BUFFERS     4  //external : 8,  internal : 4
#endif

#define MAX_PLANES      (3)
#define V4L2_BUF_TYPE V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE

#define V4L2_MEMORY_TYPE V4L2_MEMORY_USERPTR
#define RECORD_PIX_FMT V4L2_PIX_FMT_NV12M
#define PREVIEW_NUM_PLANE (3)
#define RECORD_NUM_PLANE (2)

/*
 * V 4 L 2   F I M C   E X T E N S I O N S
 *
 */
#define V4L2_CID_ROTATION                   (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PADDR_Y                    (V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_PADDR_CB                   (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_PADDR_CR                   (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_PADDR_CBCR                 (V4L2_CID_PRIVATE_BASE + 4)
#define V4L2_CID_STREAM_PAUSE               (V4L2_CID_PRIVATE_BASE + 53)

#define V4L2_CID_CAM_JPEG_MAIN_SIZE         (V4L2_CID_PRIVATE_BASE + 32)
#define V4L2_CID_CAM_JPEG_MAIN_OFFSET       (V4L2_CID_PRIVATE_BASE + 33)
#define V4L2_CID_CAM_JPEG_THUMB_SIZE        (V4L2_CID_PRIVATE_BASE + 34)
#define V4L2_CID_CAM_JPEG_THUMB_OFFSET      (V4L2_CID_PRIVATE_BASE + 35)
#define V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET   (V4L2_CID_PRIVATE_BASE + 36)
#define V4L2_CID_CAM_JPEG_QUALITY           (V4L2_CID_PRIVATE_BASE + 37)

#define TPATTERN_COLORBAR           1
#define TPATTERN_HORIZONTAL         2
#define TPATTERN_VERTICAL           3

#define V4L2_PIX_FMT_YVYU           v4l2_fourcc('Y', 'V', 'Y', 'U')

/* FOURCC for FIMC specific */
#define V4L2_PIX_FMT_VYUY           v4l2_fourcc('V', 'Y', 'U', 'Y')
#define V4L2_PIX_FMT_NV16           v4l2_fourcc('N', 'V', '1', '6')
#define V4L2_PIX_FMT_NV61           v4l2_fourcc('N', 'V', '6', '1')
#define V4L2_PIX_FMT_NV12T          v4l2_fourcc('T', 'V', '1', '2')
/*
 * U S E R   D E F I N E D   T Y P E S
 *
 */
#define PREVIEW_MODE 1
#define RECORD_MODE 2

/* We use this struct as the v4l2_streamparm raw_data for
 * VIDIOC_G_PARM and VIDIOC_S_PARM
 */
struct sec_cam_parm {
    struct v4l2_captureparm capture;
    int contrast;
    int effects;
    int brightness;
    int exposure;
    int flash_mode;
    int focus_mode;
    int iso;
    int metering;
    int saturation;
    int scene_mode;
    int sharpness;
    int hue;
    int white_balance;
};

struct yuv_fmt_list {
    const char  *name;
    const char  *desc;
    unsigned int    fmt;
    int     depth;
    int     planes;
};

struct camsensor_date_info {
    unsigned int year;
    unsigned int month;
    unsigned int date;
};

class SecCamera : public virtual RefBase {
public:

    enum CAMERA_ID {
        CAMERA_ID_BACK  = 0,
        CAMERA_ID_FRONT = 1,
    };

    enum JPEG_QUALITY {
        JPEG_QUALITY_ECONOMY    = 0,
        JPEG_QUALITY_NORMAL     = 50,
        JPEG_QUALITY_SUPERFINE  = 100,
        JPEG_QUALITY_MAX,
    };

    enum OBJECT_TRACKING {
        OBJECT_TRACKING_OFF,
        OBJECT_TRACKING_ON,
        OBJECT_TRACKING_MAX,
    };

    /*VT call*/
    enum VT_MODE {
        VT_MODE_OFF,
        VT_MODE_ON,
        VT_MODE_MAX,
    };

    /*Camera sensor mode - Camcorder fix fps*/
    enum SENSOR_MODE {
        SENSOR_MODE_CAMERA,
        SENSOR_MODE_MOVIE,
    };

    /*Camera Shot mode*/
    enum SHOT_MODE {
        SHOT_MODE_SINGLE        = 0,
        SHOT_MODE_CONTINUOUS    = 1,
        SHOT_MODE_PANORAMA      = 2,
        SHOT_MODE_SMILE         = 3,
        SHOT_MODE_SELF          = 6,
    };

    enum CHK_DATALINE {
        CHK_DATALINE_OFF,
        CHK_DATALINE_ON,
        CHK_DATALINE_MAX,
    };

    int m_touch_af_start_stop;

    SecCamera();
    virtual ~SecCamera();

    static SecCamera* createInstance(void)
    {
        static SecCamera singleton;
        return &singleton;
    }
    status_t dump(int fd);

    bool            CreateCamera(int index);
    bool            DestroyCamera(void);
    int             getCameraId(void);

    int             startPreview(void);
    int             stopPreview(void);
    int             getPreviewState(void)
    {
        return m_preview_state;
    }
    void            clearPreviewState(void)
    {
        m_preview_state = 0;
    }

    int             startRecord(void);
    int             stopRecord(void);
    int             setPreviewFrame(int index);
    int             getRecordFrame(void);
    int             releaseRecordFrame(int index);
    int             getRecordAddr(int index, SecBuffer *buffer);

    int             getPreview(void);
    int             setPreviewSize(int width, int height, int pixel_format);
    int             getPreviewSize(int *width, int *height, int *frame_size);
    int             getPreviewMaxSize(int *width, int *height);
    int             getPreviewPixelFormat(void);
    int             setPreviewImage(int index, unsigned char *buffer, int size);

    int             setSnapshotSize(int width, int height);
    int             getSnapshotSize(int *width, int *height, int *frame_size);
    int             getSnapshotMaxSize(int *width, int *height);
    int             setSnapshotPixelFormat(int pixel_format);
    int             getSnapshotPixelFormat(void);

    unsigned char*  getJpeg(unsigned char *snapshot_data, int snapshot_size, int *size);
    unsigned char*  yuv2Jpeg(unsigned char *raw_data, int raw_size,
                                int *jpeg_size,
                                int width, int height, int pixel_format);

    int             setJpegThumbnailSize(int width, int height);
    int             getJpegThumbnailSize(int *width, int *height);

    int             setJpegThumbnailQuality(int jpeg_thumbnail_quality);
    int             getJpegThumbnailQuality(void);

    int             setAutofocus(void);

    int             SetRotate(int angle);
    int             getRotate(void);

    int             setVerticalMirror(void);
    int             setHorizontalMirror(void);

    int             setWhiteBalance(int white_balance);
    int             getWhiteBalance(void);

    int             setBrightness(int brightness);
    int             getBrightness(void);

    int             setExposure(int exposure);
    int             getExposure(void);

    int             setImageEffect(int image_effect);
    int             getImageEffect(void);

    int             setSceneMode(int scene_mode);
    int             getSceneMode(void);

    int             setFlashMode(int flash_mode);
    int             getFlashMode(void);

    int             setMetering(int metering_value);
    int             getMetering(void);

    int             setISO(int iso_value);
    int             getISO(void);

    int             setContrast(int contrast_value);
    int             getContrast(void);

    int             setSaturation(int saturation_value);
    int             getSaturation(void);

    int             setSharpness(int sharpness_value);
    int             getSharpness(void);

    int             setHue(int hue_value);
    int             getHue(void);

    int             setWDR(int wdr_value);
    int             getWDR(void);

    int             setAntiShake(int anti_shake);
    int             getAntiShake(void);

    int             setJpegQuality(int jpeg_qality);
    int             getJpegQuality(void);

    int             setZoom(int zoom_level);
    int             getZoom(void);

    int             setObjectTracking(int object_tracking);
    int             getObjectTracking(void);
    int             getObjectTrackingStatus(void);

    int             setSmartAuto(int smart_auto);
    int             getSmartAuto(void);
    int             getAutosceneStatus(void);

    int             setBeautyShot(int beauty_shot);
    int             getBeautyShot(void);

    int             setVintageMode(int vintage_mode);
    int             getVintageMode(void);

    int             setFocusMode(int focus_mode);
    int             getFocusMode(void);

    int             setFaceDetect(int face_detect);
    int             getFaceDetect(void);

    int             setGPSLatitude(const char *gps_latitude);
    int             setGPSLongitude(const char *gps_longitude);
    int             setGPSAltitude(const char *gps_altitude);
    int             setGPSTimeStamp(const char *gps_timestamp);
    int             setGPSProcessingMethod(const char *gps_timestamp);
    int             cancelAutofocus(void);
    int             setFaceDetectLockUnlock(int facedetect_lockunlock);
    int             setObjectPosition(int x, int y);
    int             setObjectTrackingStartStop(int start_stop);
    int             setTouchAFStartStop(int start_stop);
    int             setCAFStatus(int on_off);
    int             getAutoFocusResult(void);
    int             setAntiBanding(int anti_banding);
    int             getPostview(void);
    int             setRecording(int recording_en);
    int             setRecordingSize(int width, int height);
    int             getRecordingSize(int *width, int *height);
    int             setGamma(int gamma);
    int             setSlowAE(int slow_ae);
    int             setExifOrientationInfo(int orientationInfo);
    int             setBatchReflection(void);
    int             setSnapshotCmd(void);
    int             endSnapshot(void);
    int             setCameraSensorReset(void);
    int             setSensorMode(int sensor_mode); /* Camcorder fix fps */
    int             setShotMode(int shot_mode);     /* Shot mode */
    int             setDataLineCheck(int chk_dataline);
    int             getDataLineCheck(void);
    int             setDataLineCheckStop(void);
    int             setDefultIMEI(int imei);
    int             getDefultIMEI(void);
    const __u8*     getCameraSensorName(void);
#ifdef ENABLE_ESD_PREVIEW_CHECK
    int             getCameraSensorESDStatus(void);
#endif // ENABLE_ESD_PREVIEW_CHECK

    int setFrameRate(int frame_rate);
    unsigned char*  getJpeg(int*, unsigned int*);
    int             getSnapshotAndJpeg(unsigned char *yuv_buf, unsigned char *jpeg_buf,
                                        unsigned int *output_size);
    int             getExif(unsigned char *pExifDst, unsigned char *pThumbSrc);

    void            getPostViewConfig(int*, int*, int*);
    void            getThumbnailConfig(int *width, int *height, int *size);

    int             getPostViewOffset(void);
    int             getCameraFd(void);
    unsigned char*  getPictureVaddr(void);
    int             getJpegFd(void);
    void            SetJpgAddr(unsigned char *addr);
    int             getPreviewAddr(int index, SecBuffer *buffer);
    void            setUserBufferAddr(void *ptr, int index, int mode);
    static void     setJpegRatio(double ratio)
    {
        if((ratio < 0) || (ratio > 1))
            return;

        jpeg_ratio = ratio;
    }

    static double   getJpegRatio()
    {
        return jpeg_ratio;
    }

    static void     setInterleaveDataSize(int x)
    {
        interleaveDataSize = x;
    }

    static int      getInterleaveDataSize()
    {
        return interleaveDataSize;
    }

    static void     setJpegLineLength(int x)
    {
        jpegLineLength = x;
    }

    static int      getJpegLineLength()
    {
        return jpegLineLength;
    }

private:
    v4l2_streamparm m_streamparm;
    struct sec_cam_parm   *m_params;
    int             m_flagCreate;
    int             m_preview_state;
    int             m_camera_id;

    /* v4l2 sub-dev file description */
    int             m_cam_sd_fd;
    int             m_mipi_sd_fd;
    int             m_flite_sd_fd;
    int             m_gsc_sd_fd;
    int             m_gsc_vd_fd;

    /* media controller variable */
    struct media_device *media;
    struct media_link   *links;
    struct media_pad    *pads;

    struct media_entity *camera_sd_entity;
    struct media_entity *mipi_sd_entity;
    struct media_entity *flite_sd_entity;
    struct media_entity *gsc_cap_sd_entity;
    struct media_entity *gsc_cap_vd_entity;
    struct media_entity *isp_sensor_entity;
    struct media_entity *isp_front_entity;
    struct media_entity *isp_back_entity;
    struct media_entity *isp_scalerc_entity;
    struct media_entity *isp_scalerp_entity;
    struct media_entity *isp_3dnr_entity;

    int             m_cam_fd;

    int             m_cam_fd2;
    struct pollfd   m_events_c2;
    int             m_flag_record_start;

    int             m_preview_v4lformat;
    int             m_preview_width;
    int             m_preview_height;
    int             m_preview_max_width;
    int             m_preview_max_height;

    int             m_snapshot_v4lformat;
    int             m_snapshot_width;
    int             m_snapshot_height;
    int             m_snapshot_max_width;
    int             m_snapshot_max_height;
    unsigned char*  m_picture_vaddr;

    int             m_angle;
    int             m_anti_banding;
    int             m_wdr;
    int             m_anti_shake;
    int             m_zoom_level;
    int             m_object_tracking;
    int             m_smart_auto;
    int             m_beauty_shot;
    int             m_vintage_mode;
    int             m_face_detect;
    int             m_object_tracking_start_stop;
    int             m_recording_en;
    int             m_recording_width;
    int             m_recording_height;
    long            m_gps_latitude;
    long            m_gps_longitude;
    long            m_gps_altitude;
    long            m_gps_timestamp;
    int             m_sensor_mode; /*Camcorder fix fps */
    int             m_shot_mode; /* Shot mode */
    int             m_exif_orientation;
    int             m_chk_dataline;
    int             m_video_gamma;
    int             m_slow_ae;
    int             m_camera_af_flag;

    int             m_flag_camera_start;

    int             m_jpeg_fd;
    int             m_jpeg_thumbnail_width;
    int             m_jpeg_thumbnail_height;
    int             m_jpeg_thumbnail_quality;
    int             m_jpeg_quality;

    int             m_postview_offset;

#ifdef ENABLE_ESD_PREVIEW_CHECK
    int             m_esd_check_count;
#endif // ENABLE_ESD_PREVIEW_CHECK

    exif_attribute_t mExifInfo;

    struct SecBuffer m_capture_buf;
    struct SecBuffer m_buffers_preview[MAX_BUFFERS];
    struct SecBuffer m_buffers_record[MAX_BUFFERS];
    struct pollfd   m_events_c;

    inline void     writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 uint32_t value);
    inline void     writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 unsigned char *pValue);
    inline void     writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 rational_t *pValue,
                                 unsigned int *offset,
                                 unsigned char *start);
    inline void     writeExifIfd(unsigned char **pCur,
                                 unsigned short tag,
                                 unsigned short type,
                                 unsigned int count,
                                 unsigned char *pValue,
                                 unsigned int *offset,
                                 unsigned char *start);

    void            setExifChangedAttribute();
    void            setExifFixedAttribute();
    int             makeExif (unsigned char *exifOut,
                                        unsigned char *thumb_buf,
                                        unsigned int thumb_size,
                                        exif_attribute_t *exifInfo,
                                        unsigned int *size,
                                        bool useMainbufForThumb);
    void            resetCamera();

    static double   jpeg_ratio;
    static int      interleaveDataSize;
    static int      jpegLineLength;
};

extern unsigned long measure_time_camera(struct timeval *start, struct timeval *stop);

}; // namespace android

#endif // ANDROID_HARDWARE_CAMERA_SEC_H
