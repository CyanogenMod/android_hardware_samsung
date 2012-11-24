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

//#define LOG_NDEBUG 0
//#define LOG_TAG "libhdmi"
#include <cutils/log.h>

#include "SecHdmi.h"
#include "SecHdmiV4L2Utils.h"

#define CHECK_GRAPHIC_LAYER_TIME (0)

namespace android {

extern unsigned int output_type;
extern v4l2_std_id t_std_id;
extern int g_hpd_state;
extern unsigned int g_hdcp_en;

extern int fp_tvout;
extern int fp_tvout_v;
extern int fp_tvout_g0;
extern int fp_tvout_g1;

#if defined(BOARD_USES_FIMGAPI)
extern unsigned int g2d_reserved_memory[HDMI_G2D_OUTPUT_BUF_NUM];
extern unsigned int g2d_reserved_memory_size;
extern unsigned int cur_g2d_address;
extern unsigned int g2d_buf_index;
#endif

#if defined(BOARD_USES_CEC)
SecHdmi::CECThread::~CECThread()
{
    LOG_LIB_HDMI("%s", __func__);

    mFlagRunning = false;
}

bool SecHdmi::CECThread::threadLoop()
{
    unsigned char buffer[CEC_MAX_FRAME_SIZE];
    int size;
    unsigned char lsrc, ldst, opcode;

    {
        Mutex::Autolock lock(mThreadLoopLock);
        mFlagRunning = true;

        size = CECReceiveMessage(buffer, CEC_MAX_FRAME_SIZE, 100000);

        if (!size) // no data available or ctrl-c
            return true;

        if (size == 1)
            return true; // "Polling Message"

        lsrc = buffer[0] >> 4;

        /* ignore messages with src address == mLaddr*/
        if (lsrc == mLaddr)
            return true;

        opcode = buffer[1];

        if (CECIgnoreMessage(opcode, lsrc)) {
            LOGE("### ignore message coming from address 15 (unregistered)\n");
            return true;
        }

        if (!CECCheckMessageSize(opcode, size)) {
            LOGE("### invalid message size: %d(opcode: 0x%x) ###\n", size, opcode);
            return true;
        }

        /* check if message broadcasted/directly addressed */
        if (!CECCheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
            LOGE("### invalid message mode (directly addressed/broadcast) ###\n");
            return true;
        }

        ldst = lsrc;

        //TODO: macroses to extract src and dst logical addresses
        //TODO: macros to extract opcode

        switch (opcode) {
        case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
            /* responce with "Report Physical Address" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            buffer[4] = mDevtype;
            size = 5;
            break;

        case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
            LOGD("[CEC_OPCODE_REQUEST_ACTIVE_SOURCE]\n");
            /* responce with "Active Source" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            size = 4;
            LOGD("Tx : [CEC_OPCODE_ACTIVE_SOURCE]\n");
            break;

        case CEC_OPCODE_ABORT:
        case CEC_OPCODE_FEATURE_ABORT:
        default:
            /* send "Feature Abort" */
            buffer[0] = (mLaddr << 4) | ldst;
            buffer[1] = CEC_OPCODE_FEATURE_ABORT;
            buffer[2] = CEC_OPCODE_ABORT;
            buffer[3] = 0x04; // "refused"
            size = 4;
            break;
        }

        if (CECSendMessage(buffer, size) != size)
            LOGE("CECSendMessage() failed!!!\n");

    }
    return true;
}

bool SecHdmi::CECThread::start()
{
    LOG_LIB_HDMI("%s", __func__);

    Mutex::Autolock lock(mThreadControlLock);
    if (exitPending()) {
        if (requestExitAndWait() == WOULD_BLOCK) {
            LOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
            return false;
        }
    }

    LOG_LIB_HDMI("EDIDGetCECPhysicalAddress");

    /* set to not valid physical address */
    mPaddr = CEC_NOT_VALID_PHYSICAL_ADDRESS;

    if (!EDIDGetCECPhysicalAddress(&mPaddr)) {
        LOGE("Error: EDIDGetCECPhysicalAddress() failed.\n");
        return false;
    }

    LOG_LIB_HDMI("CECOpen");

    if (!CECOpen()) {
        LOGE("CECOpen() failed!!!\n");
        return false;
    }

    /* a logical address should only be allocated when a device \
       has a valid physical address, at all other times a device \
       should take the 'Unregistered' logical address (15)
       */

    /* if physical address is not valid device should take \
       the 'Unregistered' logical address (15)
       */

    LOG_LIB_HDMI("CECAllocLogicalAddress");

    mLaddr = CECAllocLogicalAddress(mPaddr, mDevtype);

    if (!mLaddr) {
        LOGE("CECAllocLogicalAddress() failed!!!\n");
        if (!CECClose())
            LOGE("CECClose() failed!\n");
        return false;
    }

    LOG_LIB_HDMI("request to run CECThread");

    status_t ret = run("SecHdmi::CECThread", PRIORITY_DISPLAY);
    if (ret != NO_ERROR) {
        LOGE("%s fail to run thread", __func__);
        return false;
    }
    return true;
}

bool SecHdmi::CECThread::stop()
{
    LOG_LIB_HDMI("%s request Exit", __func__);

    Mutex::Autolock lock(mThreadControlLock);
    if (requestExitAndWait() == WOULD_BLOCK) {
        LOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
        return false;
    }

    if (!CECClose())
        LOGE("CECClose() failed!\n");

    mFlagRunning = false;
    return true;
}
#endif

SecHdmi::SecHdmi():
#if defined(BOARD_USES_CEC)
    mCECThread(NULL),
#endif
    mFlagCreate(false),
    mFlagConnected(false),
    mHdmiDstWidth(0),
    mHdmiDstHeight(0),
    mHdmiSrcYAddr(0),
    mHdmiSrcCbCrAddr(0),
    mHdmiOutputMode(DEFAULT_OUPUT_MODE),
    mHdmiResolutionValue(DEFAULT_HDMI_RESOLUTION_VALUE), // V4L2_STD_480P_60_4_3
    mHdmiS3DMode(0),    // 2D mode
    mCompositeStd(DEFAULT_COMPOSITE_STD),
    mHdcpMode(false),
    mAudioMode(2),
    mUIRotVal(0),
    mG2DUIRotVal(0),
    mCurrentHdmiOutputMode(-1),
    mCurrentHdmiResolutionValue(0), // 1080960
    mCurrentHdmiS3DMode(HDMI_2D), // 2D mode
    mCurrentHdcpMode(false),
    mCurrentAudioMode(-1),
    mHdmiInfoChange(true),
    mFimcDstColorFormat(0),
    mFimcCurrentOutBufIndex(0),
    mFBaddr(NULL),
    mFBsize(0),
    mFBionfd(-1),
    mDefaultFBFd(-1),
    mDisplayWidth(DEFALULT_DISPLAY_WIDTH),
    mDisplayHeight(DEFALULT_DISPLAY_HEIGHT)
{
    LOG_LIB_HDMI("%s", __func__);

    for (int i = 0; i < HDMI_LAYER_MAX; i++) {
        mFlagLayerEnable[i] = false;
        mFlagHdmiStart[i] = false;

        mSrcWidth      [i] = 0;
        mSrcHeight     [i] = 0;
        mSrcColorFormat[i] = 0;
        mHdmiResolutionWidth  [i] = 0;
        mHdmiResolutionHeight [i] = 0;
        mHdmiFd[i] = -1;
        mDstWidth  [i] = 0;
        mDstHeight [i] = 0;
        mPrevDstWidth  [i] = 0;
        mPrevDstHeight [i] = 0;
    }

    mHdmiPresetId = DEFAULT_HDMI_PRESET_ID;
    mHdmiStdId = DEFAULT_HDMI_STD_ID;

    //All layer is on
    mFlagLayerEnable[HDMI_LAYER_VIDEO] = true;
    mFlagLayerEnable[HDMI_LAYER_GRAPHIC_0] = true;
    mFlagLayerEnable[HDMI_LAYER_GRAPHIC_1] = true;

    mHdmiResolutionValueList[0]  = 1080960;
    mHdmiResolutionValueList[1]  = 1080950;
    mHdmiResolutionValueList[2]  = 1080930;
    mHdmiResolutionValueList[3]  = 1080160;
    mHdmiResolutionValueList[4]  = 1080150;
    mHdmiResolutionValueList[5]  = 720960;
    mHdmiResolutionValueList[6]  = 720950;
    mHdmiResolutionValueList[7]  = 5769501;
    mHdmiResolutionValueList[8]  = 5769502;
    mHdmiResolutionValueList[9]  = 4809601;
    mHdmiResolutionValueList[10] = 4809602;

    mHdmiS3dTbResolutionValueList[0] = 1080924;
    mHdmiS3dTbResolutionValueList[1] = 720950;

    mHdmiS3dSbsResolutionValueList[0] = 720960;

#if defined(BOARD_USES_CEC)
    mCECThread = new CECThread(this);
#endif

    SecBuffer zeroBuf;
    for (int i = 0; i < HDMI_FIMC_OUTPUT_BUF_NUM; i++)
        mFimcReservedMem[i] = zeroBuf;

    memset(&mDstRect, 0 , sizeof(struct v4l2_rect));
}

SecHdmi::~SecHdmi()
{
    LOG_LIB_HDMI("%s", __func__);

    if (mFlagCreate == true)
        LOGE("%s::this is not Destroyed fail", __func__);
    else
        disconnect();
}

bool SecHdmi::create(int width, int height)
{
    Mutex::Autolock lock(mLock);
    unsigned int fimc_buf_size = 0;
    unsigned int gralloc_buf_size = 0;
    mFimcCurrentOutBufIndex = 0;
    int stride;
    int vstride;
    int BufNum = 0;

/*
 * Video plaback (I420): output buffer size of FIMC3 is (1920 x 1088 x 1.5)
 * Video plaback (NV12): FIMC3 is not used.
 * Camera preview (YV12): output buffer size of FIMC3 is (640 x 480 x 1.5)
 * UI mode (ARGB8888) : output buffer size of FIMC3 is (480 x 800 x 1.5)
 */
#ifndef SUPPORT_1080P_FIMC_OUT
    setDisplaySize(width, height);
#endif

    stride = ALIGN(HDMI_MAX_WIDTH, 16);
    vstride = ALIGN(HDMI_MAX_HEIGHT, 16);

    fimc_buf_size = stride * vstride * HDMI_FIMC_BUFFER_BPP_SIZE;
    gralloc_buf_size = GRALLOC_BUF_SIZE * SIZE_1K;
#if defined(BOARD_USES_FIMGAPI)
    g2d_reserved_memory_size = stride * vstride * HDMI_G2D_BUFFER_BPP_SIZE;
#endif

    LOG_LIB_HDMI("%s", __func__);

    if (mFlagCreate == true) {
        LOGE("%s::Already Created fail", __func__);
        goto CREATE_FAIL;
    }

    if (mDefaultFBFd <= 0) {
        if ((mDefaultFBFd = fb_open(DEFAULT_FB)) < 0) {
            LOGE("%s:Failed to open default FB", __func__);
            return false;
        }
    }

    BufNum = 1;

    if (mSecFimc.create(SecFimc::DEV_3, SecFimc::MODE_SINGLE_BUF, BufNum) == false) {
        LOGE("%s::SecFimc create() fail", __func__);
        goto CREATE_FAIL;
    }

    for (int i = 0; i < HDMI_FIMC_OUTPUT_BUF_NUM; i++)
        mFimcReservedMem[i].phys.p = mSecFimc.getMemAddr()->phys.p + gralloc_buf_size + (fimc_buf_size * i);

#if defined(BOARD_USES_FIMGAPI)
#if defined(BOARD_USES_HDMI_SUBTITLES)
    for (int i = 0; i < HDMI_G2D_OUTPUT_BUF_NUM; i++)
        g2d_reserved_memory[i] = mFimcReservedMem[HDMI_FIMC_OUTPUT_BUF_NUM - 1].phys.p + fimc_buf_size + (g2d_reserved_memory_size * i);
#else
    for (int i = 0; i < HDMI_G2D_OUTPUT_BUF_NUM; i++)
        g2d_reserved_memory[i] = mSecFimc.getMemAddr()->phys.p + gralloc_buf_size + (g2d_reserved_memory_size * i);
#endif
#endif

    v4l2_std_id std_id;

    LOG_LIB_HDMI("%s::mHdmiOutputMode(%d) \n", __func__, mHdmiOutputMode);

    if (mHdmiOutputMode == COMPOSITE_OUTPUT_MODE) {
        std_id = composite_std_2_v4l2_std_id(mCompositeStd);
        if ((int)std_id < 0) {
            LOGE("%s::composite_std_2_v4l2_std_id(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
        if (m_setCompositeResolution(mCompositeStd) == false) {
            LOGE("%s::m_setCompositeResolution(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
    } else if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
        if (hdmi_resolution_2_std_id(mHdmiResolutionValue, mHdmiS3DMode, &mHdmiDstWidth, &mHdmiDstHeight, &std_id) < 0) {
            LOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, mHdmiResolutionValue);
            goto CREATE_FAIL;
        }
    }

    mFlagCreate = true;

    return true;

CREATE_FAIL :

    if (mSecFimc.flagCreate() == true &&
       mSecFimc.destroy()    == false)
        LOGE("%s::fimc destory fail", __func__);

    return false;
}

bool SecHdmi::destroy(void)
{
    LOG_LIB_HDMI("%s", __func__);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Already Destroyed fail \n", __func__);
        goto DESTROY_FAIL;
    }

    for (int layer = HDMI_LAYER_BASE + 1; layer <= HDMI_LAYER_GRAPHIC_0; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            LOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, layer);
            goto DESTROY_FAIL;
        }

        if (hdmi_deinit_layer(layer) < 0) {
            LOGE("%s::hdmi_deinit_layer(%d) fail \n", __func__, layer);
            goto DESTROY_FAIL;
        }
    }

    tvout_deinit();

    if (mSecFimc.flagCreate() == true && mSecFimc.destroy() == false) {
        LOGE("%s::fimc destory fail \n", __func__);
        goto DESTROY_FAIL;
    }

#ifdef USE_LCD_ADDR_IN_HERE
    {
        if (0 < mDefaultFBFd) {
            close(mDefaultFBFd);
            mDefaultFBFd = -1;
        }
    }
#endif //USE_LCD_ADDR_IN_HERE

    mFlagCreate = false;

    return true;

DESTROY_FAIL :

    return false;
}

bool SecHdmi::connect(void)
{
    LOG_LIB_HDMI("%s", __func__);

    {
        Mutex::Autolock lock(mLock);

        if (mFlagCreate == false) {
            LOGE("%s::Not Yet Created \n", __func__);
            return false;
        }

        if (mFlagConnected == true) {
            LOGD("%s::Already Connected.. \n", __func__);
            return true;
        }

        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (m_flagHWConnected() == false) {
                LOGD("%s::m_flagHWConnected() fail \n", __func__);
                return false;
            }

#if defined(BOARD_USES_EDID)
            if (!EDIDOpen())
                LOGE("EDIDInit() failed!\n");

            if (!EDIDRead()) {
                LOGE("EDIDRead() failed!\n");
                if (!EDIDClose())
                    LOGE("EDIDClose() failed!\n");
            }
#endif

#if defined(BOARD_USES_CEC)
            if (!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif
        }
    }

    if (this->setHdmiOutputMode(mHdmiOutputMode, true) == false)
        LOGE("%s::setHdmiOutputMode(%d) fail \n", __func__, mHdmiOutputMode);

    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
        if (this->setHdmiResolution(mHdmiResolutionValue, HDMI_2D, true) == false)
            LOGE("%s::setHdmiResolution(%d), mHdmiS3DMode(%d) fail \n", __func__, mHdmiResolutionValue, mHdmiS3DMode);

        if (this->setHdcpMode(mHdcpMode, false) == false)
            LOGE("%s::setHdcpMode(%d) fail \n", __func__, mHdcpMode);

        mHdmiInfoChange = true;
        mFlagConnected = true;

#if defined(BOARD_USES_EDID)
        // show display..
        display_menu();
#endif
    }

    return true;
}

bool SecHdmi::disconnect(void)
{
    LOG_LIB_HDMI("%s", __func__);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (mFlagConnected == false) {
        LOGE("%s::Already Disconnected.. \n", __func__);
        return true;
    }

    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
        mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
#if defined(BOARD_USES_CEC)
        if (mCECThread->mFlagRunning)
            mCECThread->stop();
#endif

#if defined(BOARD_USES_EDID)
        if (!EDIDClose()) {
            LOGE("EDIDClose() failed!\n");
            return false;
        }
#endif
    }

    for (int layer = SecHdmi::HDMI_LAYER_BASE + 1; layer <= SecHdmi::HDMI_LAYER_GRAPHIC_0; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            LOGE("%s::hdmiLayer(%d) layer fail \n", __func__, layer);
            return false;
        }
    }

    tvout_deinit();

    mFlagConnected = false;

    mHdmiOutputMode = DEFAULT_OUPUT_MODE;
    mHdmiResolutionValue = DEFAULT_HDMI_RESOLUTION_VALUE;
    mHdmiStdId = DEFAULT_HDMI_STD_ID;
    mCompositeStd = DEFAULT_COMPOSITE_STD;
    mAudioMode = 2;
    mCurrentHdmiOutputMode = -1;
    mCurrentHdmiResolutionValue = 0;
    mCurrentHdmiS3DMode = HDMI_2D;
    mCurrentAudioMode = -1;
    mFimcCurrentOutBufIndex = 0;

    return true;
}

bool SecHdmi::flagConnected(void)
{
    LOG_LIB_HDMI("%s", __func__);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    return mFlagConnected;
}

bool SecHdmi::flush(int srcW, int srcH, int srcColorFormat,
        unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
        int dstX, int dstY,
        int hdmiLayer,
        int num_of_hwc_layer)
{
    LOG_LIB_HDMI("%s [srcW=%d, srcH=%d, srcColorFormat=0x%x, srcYAddr=0x%x, srcCbAddr=0x%x, srcCrAddr=0x%x, dstX=%d, dstY=%d, hdmiLayer=%d]",
            __func__, srcW, srcH, srcColorFormat, srcYAddr, srcCbAddr, srcCrAddr, dstX, dstY, hdmiLayer);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (srcW != mSrcWidth[hdmiLayer] ||
        srcH != mSrcHeight[hdmiLayer] ||
        srcColorFormat != mSrcColorFormat[hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
        mHdmiInfoChange == true) {
        LOG_LIB_HDMI("m_reset param(%d, %d, %d, %d, %d, %d, %d)",
            srcW, mSrcWidth[hdmiLayer], \
            srcH, mSrcHeight[hdmiLayer], \
            srcColorFormat,mSrcColorFormat[hdmiLayer], \
            hdmiLayer);

        if (m_reset(srcW, srcH, srcColorFormat, hdmiLayer, num_of_hwc_layer) == false) {
            LOGE("%s::m_reset(%d, %d, %d, %d, %d) fail", __func__, srcW, srcH, srcColorFormat, hdmiLayer, num_of_hwc_layer);
            return false;
        }
    }

    if (srcYAddr == 0) {
        unsigned int phyFBAddr = 0;

        // get physical framebuffer address for LCD
        if (ioctl(mDefaultFBFd, S3CFB_GET_FB_PHY_ADDR, &phyFBAddr) == -1) {
            LOGE("%s:ioctl(S3CFB_GET_FB_PHY__ADDR) fail", __func__);
            return false;
        }

        /*
         * when early suspend, FIMD IP off.
         * so physical framebuffer address for LCD is 0x00000000
         * so JUST RETURN.
         */
        if (phyFBAddr == 0) {
            LOGE("%s::S3CFB_GET_FB_PHY_ADDR fail", __func__);
            return true;
        }
        srcYAddr = phyFBAddr;
        srcCbAddr = srcYAddr;
    }

    if (hdmiLayer == HDMI_LAYER_VIDEO) {
        if (srcColorFormat == HAL_PIXEL_FORMAT_YCbCr_420_SP ||
            srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP) {
            hdmi_set_v_param(hdmiLayer,
                    srcW, srcH, V4L2_PIX_FMT_NV12,
                    srcYAddr, srcCbAddr,
                    mHdmiDstWidth, mHdmiDstHeight);
        } else if (srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED) {
            hdmi_set_v_param(hdmiLayer,
                            srcW, srcH, V4L2_PIX_FMT_NV12T,
                            srcYAddr, srcCbAddr,
                            mHdmiDstWidth, mHdmiDstHeight);
        } else if (srcColorFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                 srcColorFormat == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP) {
            hdmi_set_v_param(hdmiLayer,
                    srcW, srcH, V4L2_PIX_FMT_NV21,
                    srcYAddr, srcCbAddr,
                    mHdmiDstWidth, mHdmiDstHeight);
        } else {
            if (mSecFimc.setSrcAddr(srcYAddr, srcCbAddr, srcCrAddr, srcColorFormat) == false) {
                LOGE("%s::setSrcAddr(%d, %d, %d) fail",
                        __func__, srcYAddr, srcCbAddr, srcCrAddr);
                return false;
            }

            int  y_size = 0;
            if (mUIRotVal == 0 || mUIRotVal == 180)
                y_size =  ALIGN(ALIGN(srcW,128) * ALIGN(srcH, 32), SZ_8K);
            else
                y_size =  ALIGN(ALIGN(srcH,128) * ALIGN(srcW, 32), SZ_8K);

            mHdmiSrcYAddr    = mFimcReservedMem[mFimcCurrentOutBufIndex].phys.extP[0];
            mHdmiSrcCbCrAddr = mFimcReservedMem[mFimcCurrentOutBufIndex].phys.extP[0] + y_size;
            if (mSecFimc.setDstAddr(mHdmiSrcYAddr, mHdmiSrcCbCrAddr, 0, mFimcCurrentOutBufIndex) == false) {
                LOGE("%s::mSecFimc.setDstAddr(%d, %d) fail \n",
                        __func__, mHdmiSrcYAddr, mHdmiSrcCbCrAddr);
                return false;
            }

            if (mSecFimc.draw(0, mFimcCurrentOutBufIndex) == false) {
                LOGE("%s::mSecFimc.draw() fail \n", __func__);
                return false;
            }
            if (mUIRotVal == 0 || mUIRotVal == 180)
                hdmi_set_v_param(hdmiLayer,
                        srcW, srcH, V4L2_PIX_FMT_NV12T,
                        mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                        mHdmiDstWidth, mHdmiDstHeight);
            else
                hdmi_set_v_param(hdmiLayer,
                        srcH, srcW, V4L2_PIX_FMT_NV12T,
                        mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                        mHdmiDstWidth, mHdmiDstHeight);
            mFimcCurrentOutBufIndex++;
            if (mFimcCurrentOutBufIndex >= HDMI_FIMC_OUTPUT_BUF_NUM)
                mFimcCurrentOutBufIndex = 0;
        }

    } else {
        if (srcColorFormat != HAL_PIXEL_FORMAT_BGRA_8888 &&
            srcColorFormat != HAL_PIXEL_FORMAT_RGBA_8888 &&
            srcColorFormat != HAL_PIXEL_FORMAT_RGB_565) {
            if (mSecFimc.setSrcAddr(srcYAddr, srcCbAddr, srcCrAddr, srcColorFormat) == false) {
                LOGE("%s::setSrcAddr(%d, %d, %d) fail",
                     __func__, srcYAddr, srcCbAddr, srcCrAddr);
                return false;
            }

            if (mSecFimc.draw(0, mFimcCurrentOutBufIndex) == false) {
                LOGE("%s::mSecFimc.draw() failed", __func__);
                return false;
            }
            if (hdmi_gl_set_param(hdmiLayer,
                            HAL_PIXEL_FORMAT_BGRA_8888,
                            mDstRect.width, mDstRect.height,
                            mHdmiSrcYAddr, mHdmiSrcCbCrAddr,
                            mDstRect.left , mDstRect.top,
                            mHdmiDstWidth, mHdmiDstHeight,
                            mG2DUIRotVal) < 0)
                return false;
        } else {
#if CHECK_GRAPHIC_LAYER_TIME
            nsecs_t start, end;
            start = systemTime();
#endif
            if (num_of_hwc_layer == 0) { /* UI only mode */
                struct v4l2_rect rect;

                if (mG2DUIRotVal == 0 || mG2DUIRotVal == 180)
                    hdmi_cal_rect(srcW, srcH, mHdmiDstWidth, mHdmiDstHeight, &rect);
                else
                    hdmi_cal_rect(srcH, srcW, mHdmiDstWidth, mHdmiDstHeight, &rect);

                rect.left = ALIGN(rect.left, 16);

                if (hdmi_gl_set_param(hdmiLayer,
                                srcColorFormat,
                                srcW, srcH,
                                srcYAddr, srcCbAddr,
                                rect.left, rect.top,
                                rect.width, rect.height,
                                mG2DUIRotVal) < 0)
                    return false;
            } else { /* Video Playback Mode */
                if (hdmi_gl_set_param(hdmiLayer,
                                srcColorFormat,
                                srcW, srcH,
                                srcYAddr, srcCbAddr,
                                dstX, dstY,
                                mHdmiDstWidth, mHdmiDstHeight,
                                mG2DUIRotVal) < 0)
                    return false;
            }
#if CHECK_GRAPHIC_LAYER_TIME
            end = systemTime();
            LOGD("[UI] hdmi_gl_set_param[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
        }
    }

    if (mFlagConnected) {
        if (mFlagHdmiStart[hdmiLayer] == false && m_startHdmi(hdmiLayer) == false) {
            LOGE("%s::hdmiLayer(%d) fail", __func__, hdmiLayer);
            return false;
        }
    }

    return true;
}

bool SecHdmi::clear(int hdmiLayer)
{
    LOG_LIB_HDMI("%s || hdmiLayer = %d", __func__, hdmiLayer);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }
    if (mFlagHdmiStart[hdmiLayer] == true && m_stopHdmi(hdmiLayer) == false) {
        LOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, hdmiLayer);
        return false;
    }
    return true;
}

bool SecHdmi::setHdmiOutputMode(int hdmiOutputMode, bool forceRun)
{
    LOG_LIB_HDMI("%s::hdmiOutputMode = %d, forceRun = %d", __func__, hdmiOutputMode, forceRun);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdmiOutputMode == hdmiOutputMode) {
        LOG_LIB_HDMI("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
        return true;
    }

    int newHdmiOutputMode = hdmiOutputMode;

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        LOGD("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

#if defined(BOARD_USES_EDID)
    int newV4l2OutputType = hdmi_check_output_mode(v4l2OutputType);
    if (newV4l2OutputType != v4l2OutputType) {
        newHdmiOutputMode = hdmi_v4l2_output_type_2_outputmode(newV4l2OutputType);
        if (newHdmiOutputMode < 0) {
            LOGD("%s::hdmi_v4l2_output_type_2_outputmode(%d) fail\n", __func__, newV4l2OutputType);
            return false;
        }

        LOGD("%s::calibration mode(%d -> %d)... \n", __func__, hdmiOutputMode, newHdmiOutputMode);
        mHdmiInfoChange = true;
    }
#endif

    if (mHdmiOutputMode != newHdmiOutputMode) {
        mHdmiOutputMode = newHdmiOutputMode;
        mHdmiInfoChange = true;
    }

    return true;
}

bool SecHdmi::setHdmiResolution(unsigned int hdmiResolutionValue, unsigned int s3dMode, bool forceRun)
{
    LOG_LIB_HDMI("%s:: hdmiResolutionValue = %d, s3dMode, =%d, forceRun = %d", __func__, hdmiResolutionValue, s3dMode, forceRun);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if ((forceRun == false) && (mHdmiS3DMode == s3dMode) && (mHdmiResolutionValue == hdmiResolutionValue)) {
        LOG_LIB_HDMI("%s::same hdmiResolutionValue(%d) s3dMode(%d) \n", __func__, hdmiResolutionValue, s3dMode);
        return true;
    }

    unsigned int newHdmiResolutionValue = hdmiResolutionValue;
    unsigned int newHdmiS3DMode = s3dMode;
    int w = 0;
    int h = 0;

#if defined(BOARD_USES_EDID)
    // find perfect resolutions..
    v4l2_std_id std_id;
    if (hdmi_resolution_2_std_id(newHdmiResolutionValue, newHdmiS3DMode, &w, &h, &std_id) < 0 ||
        hdmi_check_resolution(std_id) < 0) {
        bool flagFoundIndex = false;
        int resolutionValueIndex = 0;

        if(s3dMode == HDMI_S3D_TB) {
            resolutionValueIndex = m_resolutionValueIndex(DEFAULT_HDMI_S3D_TB_RESOLUTION_VALUE, newHdmiS3DMode);
            if (resolutionValueIndex < 0) {
                LOGE("%s::Cannot find matched resolution(%d) index\n", __func__, DEFAULT_HDMI_S3D_TB_RESOLUTION_VALUE);
                return false;
            }
            for (int i = resolutionValueIndex; i < HDMI_TB_RESOLUTION_NUM; i++) {
                if (hdmi_resolution_2_std_id(mHdmiS3dTbResolutionValueList[i], newHdmiS3DMode, &w, &h, &std_id) == 0 &&
                    hdmi_check_resolution(std_id) == 0) {
                    newHdmiResolutionValue = mHdmiS3dTbResolutionValueList[i];
                    flagFoundIndex = true;
                    break;
                }
            }
        } else if (s3dMode == HDMI_S3D_SBS) {
            resolutionValueIndex = m_resolutionValueIndex(DEFAULT_HDMI_S3D_SBS_RESOLUTION_VALUE, newHdmiS3DMode);
            if (resolutionValueIndex < 0) {
                LOGE("%s::Cannot find matched resolution(%d) index\n", __func__, DEFAULT_HDMI_S3D_SBS_RESOLUTION_VALUE);
                return false;
            }
            for (int i = resolutionValueIndex; i < HDMI_SBS_RESOLUTION_NUM; i++) {
                if (hdmi_resolution_2_std_id(mHdmiS3dSbsResolutionValueList[i], newHdmiS3DMode, &w, &h, &std_id) == 0 &&
                    hdmi_check_resolution(std_id) == 0) {
                    newHdmiResolutionValue = mHdmiS3dSbsResolutionValueList[i];
                    flagFoundIndex = true;
                    break;
                }
            }
        }

        if (flagFoundIndex == false) {
            newHdmiS3DMode = HDMI_2D;
            resolutionValueIndex = m_resolutionValueIndex(DEFAULT_HDMI_RESOLUTION_VALUE, newHdmiS3DMode);
            if (resolutionValueIndex < 0) {
                LOGE("%s::Cannot find matched resolution(%d) index\n", __func__, DEFAULT_HDMI_RESOLUTION_VALUE);
                return false;
            }
            for (int i = resolutionValueIndex; i < HDMI_2D_RESOLUTION_NUM; i++) {
                if (hdmi_resolution_2_std_id(mHdmiResolutionValueList[i], newHdmiS3DMode, &w, &h, &std_id) == 0 &&
                    hdmi_check_resolution(std_id) == 0) {
                    newHdmiResolutionValue = mHdmiResolutionValueList[i];
                    flagFoundIndex = true;
                    break;
                }
            }
        }

        if (flagFoundIndex == false) {
            newHdmiS3DMode = HDMI_2D;
            LOGE("%s::hdmi cannot control this resolution(%d) fail \n", __func__, hdmiResolutionValue);
            // Set resolution to 480P
            newHdmiResolutionValue = mHdmiResolutionValueList[HDMI_2D_RESOLUTION_NUM-2];
        } else {
            LOGD("%s::HDMI resolutions size is calibrated(%d -> %d)..\n", __func__, hdmiResolutionValue, newHdmiResolutionValue);
        }
    }
    else {
        LOG_LIB_HDMI("%s::find resolutions(%d) at once\n", __func__, hdmiResolutionValue);
    }
#endif

    if (mHdmiResolutionValue != newHdmiResolutionValue) {
        mHdmiResolutionValue = newHdmiResolutionValue;
        mHdmiInfoChange = true;
    }

    if (mHdmiS3DMode != newHdmiS3DMode) {
        mHdmiS3DMode = newHdmiS3DMode;
        mHdmiInfoChange = true;
    }

    return true;
}

bool SecHdmi::setHdcpMode(bool hdcpMode, bool forceRun)
{
    LOG_LIB_HDMI("%s", __func__);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdcpMode == hdcpMode) {
        LOG_LIB_HDMI("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
        return true;
    }

    mHdcpMode = hdcpMode;
    mHdmiInfoChange = true;

    return true;
}

bool SecHdmi::setUIRotation(unsigned int rotVal, unsigned int hwcLayer)
{
    LOG_LIB_HDMI("%s", __func__);

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        LOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (rotVal % 90 != 0) {
        LOGE("%s::Invalid rotation value(%d)", __func__, rotVal);
        return false;
    }

    /* G2D rotation */
    if (rotVal != mG2DUIRotVal) {
        mG2DUIRotVal = rotVal;
        mHdmiInfoChange = true;
    }

    /* FIMC rotation */
    if (hwcLayer == 0) { /* Rotate in UI only mode */
        if (rotVal != mUIRotVal) {
            mSecFimc.setRotVal(rotVal);
            mUIRotVal = rotVal;
            mHdmiInfoChange = true;
        }
    } else { /* Don't rotate video layer when video is played. */
        rotVal = 0;
        if (rotVal != mUIRotVal) {
            mSecFimc.setRotVal(rotVal);
            mUIRotVal = rotVal;
            mHdmiInfoChange = true;
        }
    }

    return true;
}

bool SecHdmi::setDisplaySize(int width, int height)
{
    mDisplayWidth = width;
    mDisplayHeight = height;

    return true;
}

bool SecHdmi::m_reset(int w, int h, int colorFormat, int hdmiLayer, int hwcLayer)
{
    LOG_LIB_HDMI("%s", __func__);

    v4l2_std_id std_id = 0;
    mFimcCurrentOutBufIndex = 0;

    int srcW = w;
    int srcH = h;

    // stop all..
    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            LOGE("%s::m_stopHdmi: layer[%d] fail", __func__, layer);
            return false;
        }
    }

    if (w != mSrcWidth [hdmiLayer] ||
        h != mSrcHeight [hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
        colorFormat != mSrcColorFormat[hdmiLayer]) {
        int preVideoSrcColorFormat = mSrcColorFormat[hdmiLayer];
        int videoSrcColorFormat = colorFormat;

        if (preVideoSrcColorFormat != HAL_PIXEL_FORMAT_YCbCr_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED) {
                LOGI("%s: Unsupported preVideoSrcColorFormat = 0x%x\n", __func__, preVideoSrcColorFormat);
                preVideoSrcColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED;
        }

        if (hdmiLayer == HDMI_LAYER_VIDEO) {
            if (colorFormat != HAL_PIXEL_FORMAT_YCbCr_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP &&
                colorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED) {
                LOG_LIB_HDMI("### %s  call mSecFimc.setSrcParams\n", __func__);
                unsigned int full_wdith = ALIGN(w, 16);
                unsigned int full_height = ALIGN(h, 2);

                if (mSecFimc.setSrcParams(full_wdith, full_height, 0, 0,
                            (unsigned int*)&w, (unsigned int*)&h, colorFormat, true) == false) {
                    LOGE("%s::mSecFimc.setSrcParams(%d, %d, %d) fail \n",
                            __func__, w, h, colorFormat);
                    return false;
                }

                mFimcDstColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED;

                LOG_LIB_HDMI("### %s  call mSecFimc.setDstParams\n", __func__);

                if (mUIRotVal == 0 || mUIRotVal == 180) {
                    if (mSecFimc.setDstParams((unsigned int)w, (unsigned int)h, 0, 0,
                                (unsigned int*)&w, (unsigned int*)&h, mFimcDstColorFormat, true) == false) {
                        LOGE("%s::mSecFimc.setDstParams(%d, %d, %d) fail \n",
                                __func__, w, h, mFimcDstColorFormat);
                        return false;
                    }
                } else {
                    if (mSecFimc.setDstParams((unsigned int)h, (unsigned int)w, 0, 0,
                                (unsigned int*)&h, (unsigned int*)&w, mFimcDstColorFormat, true) == false) {
                        LOGE("%s::mSecFimc.setDstParams(%d, %d, %d) fail \n",
                                __func__, w, h, mFimcDstColorFormat);
                        return false;
                    }
                }
            }
            mPrevDstWidth[hdmiLayer] = mHdmiDstWidth;
            mPrevDstHeight[hdmiLayer] = mHdmiDstHeight;
        } else {
        }

        if (preVideoSrcColorFormat != videoSrcColorFormat)
            mHdmiInfoChange = true;

        mSrcWidth[hdmiLayer] = srcW;
        mSrcHeight[hdmiLayer] = srcH;
        mSrcColorFormat[hdmiLayer] = colorFormat;

        mHdmiResolutionWidth[hdmiLayer] = mHdmiDstWidth;
        mHdmiResolutionHeight[hdmiLayer] = mHdmiDstHeight;

        LOG_LIB_HDMI("m_reset saved param(%d, %d, %d, %d, %d, %d, %d) \n",
            srcW, mSrcWidth[hdmiLayer], \
            srcH, mSrcHeight[hdmiLayer], \
            colorFormat,mSrcColorFormat[hdmiLayer], \
            hdmiLayer);
    }

    if (mHdmiInfoChange == true) {
        LOG_LIB_HDMI("mHdmiInfoChange: %d\n", mHdmiInfoChange);

        // stop all..
#if defined(BOARD_USES_CEC)
        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (mCECThread->mFlagRunning)
                mCECThread->stop();
        }
#endif

        if (m_setHdmiOutputMode(mHdmiOutputMode) == false) {
            LOGE("%s::m_setHdmiOutputMode() fail \n", __func__);
            return false;
        }
        if (mHdmiOutputMode == COMPOSITE_OUTPUT_MODE) {
            std_id = composite_std_2_v4l2_std_id(mCompositeStd);
            if ((int)std_id < 0) {
                LOGE("%s::composite_std_2_v4l2_std_id(%d) fail\n", __func__, mCompositeStd);
                return false;
            }
            if (m_setCompositeResolution(mCompositeStd) == false) {
                LOGE("%s::m_setCompositeRsolution() fail \n", __func__);
                return false;
            }
        } else if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                   mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (m_setHdmiResolution(mHdmiResolutionValue, mHdmiS3DMode) == false) {
                LOGE("%s::m_setHdmiResolution() fail \n", __func__);
                return false;
            }

            if (m_setHdcpMode(mHdcpMode) == false) {
                LOGE("%s::m_setHdcpMode() fail \n", __func__);
                return false;
            }
            std_id = mHdmiStdId;
        }

        fp_tvout = tvout_init(std_id);

        for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
            if (hdmi_deinit_layer(layer) < 0)
                LOGE("%s::hdmi_init_layer(%d) fail \n", __func__, layer);
        }

        for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
            if (hdmi_init_layer(layer) < 0)
                LOGE("%s::hdmi_init_layer(%d) fail \n", __func__, layer);
        }

        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
#if defined(BOARD_USES_CEC)
            if (!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif

            if (m_setAudioMode(mAudioMode) == false)
                LOGE("%s::m_setAudioMode() fail \n", __func__);
        }

        mHdmiInfoChange = false;
    }

    return true;
}

bool SecHdmi::m_startHdmi(int hdmiLayer)
{
    LOG_LIB_HDMI("%s", __func__);

    bool ret = true;
    int buf_index = 0;

    LOG_LIB_HDMI("### %s: hdmiLayer(%d) called\n", __func__, hdmiLayer);

    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
        tvout_v4l2_start_overlay(fp_tvout_v);
        mFlagHdmiStart[hdmiLayer] = true;
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g0, FBIOBLANK, (void *)FB_BLANK_UNBLANK) != -1)
                mFlagHdmiStart[hdmiLayer] = true;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g1, FBIOBLANK, (void *)FB_BLANK_UNBLANK) != -1)
                mFlagHdmiStart[hdmiLayer] = true;
        }
        break;
    default :
        LOGE("%s::unmathced layer(%d) fail", __func__, hdmiLayer);
        ret = false;
        break;
    }

    return true;
}

bool SecHdmi::m_stopHdmi(int hdmiLayer)
{
    LOG_LIB_HDMI("%s", __func__);

    bool ret = true;
    if (mFlagHdmiStart[hdmiLayer] == false) {
        LOGD("%s::already HDMI(%d layer) stopped.. \n", __func__, hdmiLayer);
        return true;
    }

    LOG_LIB_HDMI("### %s : layer[%d] called\n", __func__, hdmiLayer);

    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
        tvout_v4l2_stop_overlay(fp_tvout_v);
        mFlagHdmiStart[hdmiLayer] = false;
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g0, FBIOBLANK, (void *)FB_BLANK_POWERDOWN) != -1)
                mFlagHdmiStart[hdmiLayer] = false;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
#if defined(BOARD_USES_FIMGAPI)
        cur_g2d_address = 0;
        g2d_buf_index = 0;
#endif
        if (mFlagLayerEnable[hdmiLayer]) {
            if (ioctl(fp_tvout_g1, FBIOBLANK, (void *)FB_BLANK_POWERDOWN) != -1)
                mFlagHdmiStart[hdmiLayer] = false;
        }
        break;
    default :
        LOGE("%s::unmathced layer(%d) fail", __func__, hdmiLayer);
        ret = false;
        break;
    }

    return true;
}

bool SecHdmi::m_setHdmiOutputMode(int hdmiOutputMode)
{
    LOG_LIB_HDMI("%s", __func__);

    if (hdmiOutputMode == mCurrentHdmiOutputMode) {
        LOG_LIB_HDMI("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
        return true;
    }

    LOG_LIB_HDMI("### %s called\n", __func__);

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        LOGE("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

    output_type = v4l2OutputType;

    mCurrentHdmiOutputMode = hdmiOutputMode;

    return true;
}

bool SecHdmi::m_setCompositeResolution(unsigned int compositeStdId)
{
    LOG_LIB_HDMI("%s", __func__);

    int w = 0;
    int h = 0;

    if (mHdmiOutputMode != COMPOSITE_OUTPUT_MODE) {
        LOGE("%s:: not supported output type \n", __func__);
        return false;
    }

    switch (compositeStdId) {
    case COMPOSITE_STD_NTSC_M:
    case COMPOSITE_STD_NTSC_443:
        w = 704;
        h = 480;
        break;
    case COMPOSITE_STD_PAL_BDGHI:
    case COMPOSITE_STD_PAL_M:
    case COMPOSITE_STD_PAL_N:
    case COMPOSITE_STD_PAL_Nc:
    case COMPOSITE_STD_PAL_60:
        w = 704;
        h = 576;
        break;
    default:
        LOGE("%s::unmathced composite_std(%d)", __func__, compositeStdId);
        return false;
    }

    t_std_id      = composite_std_2_v4l2_std_id(mCompositeStd);

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = -1;
    return true;
}

bool SecHdmi::m_setHdmiResolution(unsigned int hdmiResolutionValue, unsigned int s3dMode)
{
    LOG_LIB_HDMI("%s", __func__);

    if ((hdmiResolutionValue == mCurrentHdmiResolutionValue) && (s3dMode == mCurrentHdmiS3DMode)) {
        LOG_LIB_HDMI("%s::same hdmiResolutionValue(%d), s3dMode(%d)\n", __func__, hdmiResolutionValue, s3dMode);
        return true;
    }

    int w = 0;
    int h = 0;

    v4l2_std_id std_id;

    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
        mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
        if (hdmi_resolution_2_std_id(hdmiResolutionValue, s3dMode, &w, &h, &std_id) < 0) {
            LOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, hdmiResolutionValue);
            return false;
        }
        mHdmiStdId    = std_id;
    } else {
        LOGE("%s:: not supported output type \n", __func__);
        return false;
    }

    t_std_id      = std_id;

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = hdmiResolutionValue;
    mCurrentHdmiS3DMode = s3dMode;

        LOG_LIB_HDMI("%s:: mHdmiDstWidth = %d, mHdmiDstHeight = %d, mHdmiStdId = 0x%x, hdmiResolutionValue = 0x%x, s3dMode = 0x%x\n",
                __func__,
                mHdmiDstWidth,
                mHdmiDstHeight,
                mHdmiStdId,
                hdmiResolutionValue,
                s3dMode);

    return true;
}

bool SecHdmi::m_setHdcpMode(bool hdcpMode)
{
    LOG_LIB_HDMI("%s", __func__);

    if (hdcpMode == mCurrentHdcpMode) {
        LOG_LIB_HDMI("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
        return true;
    }

    if (hdcpMode == true)
        g_hdcp_en = 1;
    else
        g_hdcp_en = 0;

    mCurrentHdcpMode = hdcpMode;

    return true;
}

bool SecHdmi::m_setAudioMode(int audioMode)
{
    LOG_LIB_HDMI("%s", __func__);

    if (audioMode == mCurrentAudioMode) {
        LOG_LIB_HDMI("%s::same audioMode(%d) \n", __func__, audioMode);
        return true;
    }

    if (hdmi_check_audio() < 0) {
        LOGE("%s::hdmi_check_audio() fail \n", __func__);
        return false;
    }

    mCurrentAudioMode = audioMode;

    return true;
}

int SecHdmi::m_resolutionValueIndex(unsigned int ResolutionValue, unsigned int s3dMode)
{
    LOG_LIB_HDMI("%s", __func__);

    int index = -1;

    if (s3dMode == HDMI_2D) {
        for (int i = 0; i < HDMI_2D_RESOLUTION_NUM; i++) {
            if (mHdmiResolutionValueList[i] == ResolutionValue) {
                index = i;
                break;
            }
        }
    } else if (s3dMode == HDMI_S3D_TB) {
        for (int i = 0; i < HDMI_TB_RESOLUTION_NUM; i++) {
            if (mHdmiS3dTbResolutionValueList[i] == ResolutionValue) {
                index = i;
                break;
            }
        }
    } else if (s3dMode == HDMI_S3D_SBS) {
        for (int i = 0; i < HDMI_SBS_RESOLUTION_NUM; i++) {
            if (mHdmiS3dSbsResolutionValueList[i] == ResolutionValue) {
                index = i;
                break;
            }
        }
    } else {
        LOGE("%s::Unsupported S3D mode(%d)\n", __func__, s3dMode);
    }

    return index;
}

bool SecHdmi::m_flagHWConnected(void)
{
    LOG_LIB_HDMI("%s", __func__);

    bool ret = true;
    int hdmiStatus = hdmi_cable_status();

    if (hdmiStatus <= 0) {
        LOG_LIB_HDMI("%s::hdmi_cable_status() fail \n", __func__);
        ret = false;
    } else {
        ret = true;
    }

    return ret;
}

}; // namespace android
