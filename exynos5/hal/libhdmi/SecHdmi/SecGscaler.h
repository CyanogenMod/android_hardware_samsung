/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SEC_GSC_OUT_H__
#define __SEC_GSC_OUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/fb.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <asm/sizes.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <hardware/hardware.h>

#include "utils/Timers.h"

#include "s5p_fimc_v4l2.h"
#include "sec_utils_v4l2.h"
#include "media.h"
#include "v4l2-subdev.h"

#include "sec_format.h"

#include "SecBuffer.h"

#define PFX_NODE_MEDIADEV          "/dev/media"
#define PFX_NODE_SUBDEV            "/dev/v4l-subdev"
#define PFX_NODE_VIDEODEV          "/dev/video"
#define PFX_ENTITY_SUBDEV_GSC      "exynos-gsc-sd.%d"
#define PFX_ENTITY_OUTPUTDEV_GSC   "exynos-gsc.%d.output"

#define GSC_SUBDEV_PAD_SINK      (0)
#define GSC_SUBDEV_PAD_SOURCE    (1)
#define GSC_VIDEODEV_PAD_SOURCE  (0)
#define MAX_BUFFERS_GSCALER      (2)
#define MAX_PLANES_GSCALER       (3)

#ifdef __cplusplus
}

class SecGscaler
{
public:
    enum DEV {
        DEV_0 = 0,
        DEV_1,
        DEV_2,
        DEV_3,
        DEV_MAX,
    };

    enum MODE {
        MODE_NONE = 0,
        MODE_SINGLE_BUF,
        MODE_MULTI_BUF,
        MODE_DMA_AUTO,
        MODE_MAX,
    };

private:
    bool                        mFlagCreate;
    int                         mDev;
    int                         mVideoNodeNum;
    int                         mSubdevNodeNum;
    int                         mVideodevFd;
    int                         mMediadevFd;
    int                         mSubdevFd;
    unsigned int                mNumOfBuf;
    unsigned int                mSrcIndex;
    int                         mRotVal;
    bool                        mFlagGlobalAlpha;
    int                         mGlobalAlpha;
    bool                        mFlagLocalAlpha;
    bool                        mFlagColorKey;
    int                         mColorKey;
    bool                        mFlagSetSrcParam;
    bool                        mFlagSetDstParam;
    bool                        mFlagStreamOn;

    s5p_fimc_t                  mS5pFimc;
    SecBuffer                   mSrcBuffer[MAX_BUFFERS_GSCALER];

    __u32                       mSubdevEntity;
    __u32                       mVideodevEntity;
    //struct media_link_desc      mlink_desc;

public:
    SecGscaler();
    virtual ~SecGscaler();

    bool create(enum DEV dev, enum MODE mode, unsigned int numOfBuf);
    bool create(enum DEV dev, unsigned int numOfBuf);

    bool destroy(void);
    bool flagCreate(void);

    int   getFd(void);
    int   getVideodevFd(void);
    bool  openVideodevFd(void);
    bool  closeVideodevFd(void);

    int   getSubdevFd(void);
    __u32 getSubdevEntity(void);
    __u32 getVideodevEntity(void);
    bool  getFlagSteamOn(void);

    SecBuffer * getSrcBufferAddr(int index);

    bool setSrcParams(unsigned int width, unsigned int height,
                      unsigned int cropX, unsigned int cropY,
                      unsigned int *cropWidth, unsigned int *cropHeight,
                      int colorFormat,
                      bool forceChange = true);

    bool getSrcParams(unsigned int *width, unsigned int *height,
                      unsigned int *cropX, unsigned int *cropY,
                      unsigned int *cropWidth, unsigned int *cropHeight,
                      int *v4l2colorFormat);

    bool setSrcAddr(unsigned int YAddr,
                    unsigned int CbAddr = 0,
                    unsigned int CrAddr = 0,
                    int colorFormat = 0);

    bool setDstParams(unsigned int width, unsigned int height,
                      unsigned int cropX, unsigned int cropY,
                      unsigned int *cropWidth, unsigned int *cropHeight,
                      int colorFormat,
                      bool forceChange = true);

    bool getDstParams(unsigned int *width, unsigned int *height,
                      unsigned int *cropX, unsigned int *cropY,
                      unsigned int *cropWidth, unsigned int *cropHeight,
                      int *mbusColorFormat);

    bool setDstAddr(unsigned int YAddr,
                    unsigned int CbAddr = 0,
                    unsigned int CrAddr = 0,
                    int buf_index = 0);

    bool setRotVal(unsigned int rotVal);
    bool setGlobalAlpha(bool enable = true, int alpha = 0xff);
    bool setLocalAlpha(bool enable);
    bool setColorKey(bool enable = true, int colorKey = 0xff);

    bool run(void);
    bool streamOn(void);
    bool streamOff(void);

private:
    bool m_checkSrcSize(unsigned int width, unsigned int height,
                        unsigned int cropX, unsigned int cropY,
                        unsigned int *cropWidth, unsigned int *cropHeight,
                        int colorFormat,
                        bool forceChange = false);

    bool m_checkDstSize(unsigned int width, unsigned int height,
                        unsigned int cropX, unsigned int cropY,
                        unsigned int *cropWidth, unsigned int *cropHeight,
                        int colorFormat,
                        int rotVal,
                        bool forceChange = false);
    int  m_widthOfFimc(int v4l2ColorFormat, int width);
    int  m_heightOfFimc(int v4l2ColorFormat, int height);
    int  m_getYuvBpp(unsigned int fmt);
    int  m_getYuvPlanes(unsigned int fmt);
};
#endif

#endif //__SEC_GSC_OUT_H__
