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

//#define DEBUG_MSG_ENABLE
//#define LOG_NDEBUG 0
//#define LOG_TAG "libhdmi"
#include <cutils/log.h>
#include "ion.h"
#include "SecHdmi.h"
#include "SecHdmiV4L2Utils.h"

#define CHECK_GRAPHIC_LAYER_TIME (0)

namespace android {

extern unsigned int output_type;
extern v4l2_std_id t_std_id;
extern int g_hpd_state;
extern unsigned int g_hdcp_en;

#if defined(BOARD_USES_HDMI_FIMGAPI)
extern unsigned int g2d_reserved_memory0;
extern unsigned int g2d_reserved_memory1;
extern unsigned int g2d_reserved_memory_size;
extern unsigned int cur_g2d_address;
#endif

#if defined(BOARD_USES_CEC)
SecHdmi::CECThread::~CECThread()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
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
            ALOGE("### ignore message coming from address 15 (unregistered)\n");
            return true;
        }

        if (!CECCheckMessageSize(opcode, size)) {
            ALOGE("### invalid message size: %d(opcode: 0x%x) ###\n", size, opcode);
            return true;
        }

        /* check if message broadcasted/directly addressed */
        if (!CECCheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
            ALOGE("### invalid message mode (directly addressed/broadcast) ###\n");
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
            ALOGD("[CEC_OPCODE_REQUEST_ACTIVE_SOURCE]\n");
            /* responce with "Active Source" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            size = 4;
            ALOGD("Tx : [CEC_OPCODE_ACTIVE_SOURCE]\n");
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
            ALOGE("CECSendMessage() failed!!!\n");

    }
    return true;
}

bool SecHdmi::CECThread::start()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mThreadControlLock);
    if (exitPending()) {
        if (requestExitAndWait() == WOULD_BLOCK) {
            ALOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
            return false;
        }
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("EDIDGetCECPhysicalAddress");
#endif
    /* set to not valid physical address */
    mPaddr = CEC_NOT_VALID_PHYSICAL_ADDRESS;

    if (!EDIDGetCECPhysicalAddress(&mPaddr)) {
        ALOGE("Error: EDIDGetCECPhysicalAddress() failed.\n");
        return false;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("CECOpen");
#endif
    if (!CECOpen()) {
        ALOGE("CECOpen() failed!!!\n");
        return false;
    }

    /* a logical address should only be allocated when a device \
       has a valid physical address, at all other times a device \
       should take the 'Unregistered' logical address (15)
       */

    /* if physical address is not valid device should take \
       the 'Unregistered' logical address (15)
       */

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("CECAllocLogicalAddress");
#endif
    mLaddr = CECAllocLogicalAddress(mPaddr, mDevtype);

    if (!mLaddr) {
        ALOGE("CECAllocLogicalAddress() failed!!!\n");
        if (!CECClose())
            ALOGE("CECClose() failed!\n");
        return false;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("request to run CECThread");
#endif

    status_t ret = run("SecHdmi::CECThread", PRIORITY_DISPLAY);
    if (ret != NO_ERROR) {
        ALOGE("%s fail to run thread", __func__);
        return false;
    }
    return true;
}

bool SecHdmi::CECThread::stop()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s request Exit", __func__);
#endif
    Mutex::Autolock lock(mThreadControlLock);
    if (requestExitAndWait() == WOULD_BLOCK) {
        ALOGE("mCECThread.requestExitAndWait() == WOULD_BLOCK");
        return false;
    }

    if (!CECClose())
        ALOGE("CECClose() failed!\n");

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
    mPreviousHdmiPresetId(V4L2_DV_1080P60),
    mHdmiDstWidth(0),
    mHdmiDstHeight(0),
    mHdmiSrcYAddr(0),
    mHdmiSrcCbCrAddr(0),
    mFBaddr(0),
    mFBsize(0),
    mFBionfd(-1),
    mFBoffset(0),
    mHdmiOutputMode(DEFAULT_OUPUT_MODE),
    mHdmiResolutionValue(DEFAULT_HDMI_RESOLUTION_VALUE), // V4L2_STD_480P_60_4_3
    mHdmiStdId(DEFAULT_HDMI_STD_ID), // V4L2_STD_480P_60_4_3
    mCompositeStd(DEFAULT_COMPOSITE_STD),
    mHdcpMode(false),
    mAudioMode(2),
    mUIRotVal(0),
    mG2DUIRotVal(0),
    mCurrentHdmiOutputMode(-1),
    mCurrentHdmiResolutionValue(0), // 1080960
    mCurrentHdcpMode(false),
    mCurrentAudioMode(-1),
    mHdmiInfoChange(true),
    mFlagGscalerStart(false),
    mGscalerDstColorFormat(0),
    mDefaultFBFd(-1),
    mCurrentsrcW(0),
    mCurrentsrcH(0),
    mCurrentsrcColorFormat(0),
    mCurrentsrcYAddr(0),
    mCurrentsrcCbAddr(0),
    mCurrentdstX(0),
    mCurrentdstY(0),
    mCurrenthdmiLayer(0),
    mCurrentNumOfHWCLayer(0),
    mDisplayWidth(DEFALULT_DISPLAY_WIDTH),
    mDisplayHeight(DEFALULT_DISPLAY_HEIGHT)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    for (int i = 0; i < HDMI_LAYER_MAX; i++) {
        mFlagLayerEnable[i] = false;
        mFlagHdmiStart[i] = false;

        mSrcWidth      [i] = 0;
        mSrcHeight     [i] = 0;
        mSrcColorFormat[i] = 0;
        mHdmiResolutionWidth  [i] = 0;
        mHdmiResolutionHeight [i] = 0;
        mPreviousNumofHwLayer [i] = 0;
        mSrcIndex[i] = 0;
    }

    //All layer is on
    mFlagLayerEnable[HDMI_LAYER_VIDEO] = true;
    mFlagLayerEnable[HDMI_LAYER_GRAPHIC_0] = true;
    mFlagLayerEnable[HDMI_LAYER_GRAPHIC_1] = true;

    mHdmiSizeOfResolutionValueList = 14;

    mHdmiResolutionValueList[0]  = 1080960;
    mHdmiResolutionValueList[1]  = 1080950;
    mHdmiResolutionValueList[2]  = 1080930;
    mHdmiResolutionValueList[3]  = 1080924;
    mHdmiResolutionValueList[4]  = 1080160;
    mHdmiResolutionValueList[5]  = 1080150;
    mHdmiResolutionValueList[6]  = 720960;
    mHdmiResolutionValueList[7]  = 7209601;
    mHdmiResolutionValueList[8]  = 720950;
    mHdmiResolutionValueList[9]  = 7209501;
    mHdmiResolutionValueList[10] = 5769501;
    mHdmiResolutionValueList[11] = 5769502;
    mHdmiResolutionValueList[12] = 4809601;
    mHdmiResolutionValueList[13] = 4809602;

#if defined(BOARD_USES_CEC)
    mCECThread = new CECThread(this);
#endif

    mGscalerForceUpdate = false;
}

SecHdmi::~SecHdmi()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (mFlagCreate == true)
        ALOGE("%s::this is not Destroyed fail", __func__);
    else
        disconnect();
}

bool SecHdmi::create(int width, int height)
{
    Mutex::Autolock lock(mLock);
    unsigned int fimc_buf_size = 0;
    int stride;
    int vstride;

    int ionfd_G2D = 0;
    void * ion_base_addr = NULL;

    struct s3c_fb_user_ion_client ion_handle;
    unsigned int FB_size = ALIGN(width, 16) * ALIGN(height, 16) * HDMI_G2D_BUFFER_BPP_SIZE * 2;
    int ionfd_FB = 0;

/*
 * Video plaback (I420): output buffer size of FIMC3 is (1920 x 1088 x 1.5)
 * Video plaback (NV12): FIMC3 is not used.
 * Camera preview (YV12): output buffer size of FIMC3 is (640 x 480 x 1.5)
 * UI mode (ARGB8888) : output buffer size of FIMC3 is (480 x 800 x 1.5)
 */
#ifndef SUPPORT_1080P_FIMC_OUT
    setDisplaySize(width, height);
#endif

    stride  = ALIGN(HDMI_MAX_WIDTH, 16);
    vstride = ALIGN(HDMI_MAX_HEIGHT, 16);

    fimc_buf_size = stride * vstride * HDMI_FIMC_BUFFER_BPP_SIZE;
#if defined(BOARD_USES_HDMI_FIMGAPI)
    g2d_reserved_memory_size = stride * vstride * HDMI_G2D_BUFFER_BPP_SIZE;
#endif

#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == true) {
        ALOGE("%s::Already Created", __func__);
        return true;
    }

    if (mDefaultFBFd <= 0) {
        if ((mDefaultFBFd = fb_open(DEFAULT_FB)) < 0) {
            ALOGE("%s:Failed to open default FB", __func__);
            return false;
        }
    }

    if (mSecGscaler.create(SecGscaler::DEV_3, MAX_BUFFERS_GSCALER) == false) {
        ALOGE("%s::SecGscaler create() failed", __func__);
        goto CREATE_FAIL;
    }

    mIonClient = ion_client_create();
    if (mIonClient < 0) {
        mIonClient = -1;
        ALOGE("%s::ion_client_create() failed", __func__);
        goto CREATE_FAIL;
    }

    // get framebuffer virtual address for LCD
    if (ioctl(mDefaultFBFd, S3CFB_GET_ION_USER_HANDLE, &ion_handle) == -1) {
        ALOGE("%s:ioctl(S3CFB_GET_ION_USER_HANDLE) failed", __func__);
        return false;
    }

    mFBaddr  = (unsigned int)ion_map(ion_handle.fd, ALIGN(FB_size, PAGE_SIZE), 0);
    mFBsize  = FB_size;
    mFBionfd = ion_handle.fd;

#if defined(BOARD_USES_HDMI_FIMGAPI)
    ionfd_G2D = ion_alloc(mIonClient, ALIGN(g2d_reserved_memory_size * 2, PAGE_SIZE), 0, ION_HEAP_EXYNOS_MASK);

    if (ionfd_G2D < 0) {
        ALOGE("%s::ION memory allocation failed", __func__);
    } else {
        ion_base_addr = ion_map(ionfd_G2D, ALIGN(g2d_reserved_memory_size * 2, PAGE_SIZE), 0);
        if (ion_base_addr == MAP_FAILED)
            ALOGE("%s::ION mmap failed", __func__);
    }

    g2d_reserved_memory0 = (unsigned int)ion_base_addr;
    g2d_reserved_memory1 = g2d_reserved_memory0 + g2d_reserved_memory_size;
#endif

    v4l2_std_id std_id;
    __u32 preset_id;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::mHdmiOutputMode(%d) \n", __func__, mHdmiOutputMode);
#endif
    if (mHdmiOutputMode == COMPOSITE_OUTPUT_MODE) {
        std_id = composite_std_2_v4l2_std_id(mCompositeStd);
        if ((int)std_id < 0) {
            ALOGE("%s::composite_std_2_v4l2_std_id(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
        if (m_setCompositeResolution(mCompositeStd) == false) {
            ALOGE("%s::m_setCompositeResolution(%d) fail\n", __func__, mCompositeStd);
            goto CREATE_FAIL;
        }
    } else if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
        if (hdmi_resolution_2_std_id(mHdmiResolutionValue, &mHdmiDstWidth, &mHdmiDstHeight, &std_id, &preset_id) < 0) {
            ALOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, mHdmiResolutionValue);
            goto CREATE_FAIL;
        }
    }

    if (m_setupLink() == false) {
        ALOGE("%s:Enable the link failed", __func__);
        return false;
    }

    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (m_openLayer(layer) == false)
            ALOGE("%s::hdmi_init_layer(%d) failed", __func__, layer);
    }

    for (int layer = HDMI_LAYER_BASE + 2; layer < HDMI_LAYER_MAX; layer++) {
        if (tvout_std_v4l2_s_ctrl(mVideodevFd[layer], V4L2_CID_TV_LAYER_BLEND_ENABLE, 1) < 0) {
            ALOGE("%s::tvout_std_v4l2_s_ctrl [layer=%d] (V4L2_CID_TV_LAYER_BLEND_ENABLE) failed", __func__, layer);
            return false;
        }

        if (tvout_std_v4l2_s_ctrl(mVideodevFd[layer], V4L2_CID_TV_PIXEL_BLEND_ENABLE, 1) < 0) {
            ALOGE("%s::tvout_std_v4l2_s_ctrl [layer=%d] (V4L2_CID_TV_LAYER_BLEND_ENABLE) failed", __func__, layer);
            return false;
        }

        if (tvout_std_v4l2_s_ctrl(mVideodevFd[layer], V4L2_CID_TV_LAYER_BLEND_ALPHA, 255) < 0) {
            ALOGE("%s::tvout_std_v4l2_s_ctrl [layer=%d] (V4L2_CID_TV_LAYER_BLEND_ALPHA) failed", __func__, layer);
            return false;
        }
    }

    mFlagCreate = true;

    return true;

CREATE_FAIL :

    if (mSecGscaler.flagCreate() == true &&
        mSecGscaler.destroy()    == false)
        ALOGE("%s::Gscaler destory failed", __func__);

    return false;
}

bool SecHdmi::destroy(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    char node[32];
    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Already Destroyed fail \n", __func__);
        goto DESTROY_FAIL;
    }

    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            ALOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, layer);
            goto DESTROY_FAIL;
        }
    }

    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (m_closeLayer(layer) == false)
            ALOGE("%s::hdmi_deinit_layer(%d) failed", __func__, layer);
    }

    if (mSecGscaler.flagCreate() == true && mSecGscaler.destroy() == false) {
        ALOGE("%s::fimc destory fail \n", __func__);
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

#if defined(BOARD_USES_HDMI_FIMGAPI)
    ion_unmap((void *)g2d_reserved_memory0, ALIGN(g2d_reserved_memory_size * 2, PAGE_SIZE));
#endif

    if (0 < mFBaddr)
        ion_unmap((void *)mFBaddr, ALIGN(mFBsize, PAGE_SIZE));

    if (0 < mFBionfd)
        ion_free(mFBionfd);

    sprintf(node, "%s%d", PFX_NODE_MEDIADEV, 0);
    mMediadevFd = open(node, O_RDONLY);

    if (mMediadevFd < 0) {
        ALOGE("%s::open(%s) failed", __func__, node);
        goto DESTROY_FAIL;
    }

    mlink_desc.flags = 0;
    if (ioctl(mMediadevFd, MEDIA_IOC_SETUP_LINK, &mlink_desc) < 0) {
        ALOGE("%s::MEDIA_IOC_SETUP_UNLINK failed", __func__);
        goto DESTROY_FAIL;
    }

    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (m_closeLayer(layer) == false)
            ALOGE("%s::hdmi_deinit_layer(%d) failed", __func__, layer);
    }

    if (0 < mMediadevFd)
        close(mMediadevFd);

    if (0 < mSubdevMixerFd)
        close(mSubdevMixerFd);

    mMediadevFd = -1;
    mSubdevMixerFd = -1;

    mFlagCreate = false;

    return true;

DESTROY_FAIL :

    return false;
}

bool SecHdmi::connect(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    {
        Mutex::Autolock lock(mLock);

        if (mFlagCreate == false) {
            ALOGE("%s::Not Yet Created \n", __func__);
            return false;
        }

        if (mFlagConnected == true) {
            ALOGD("%s::Already Connected.. \n", __func__);
            return true;
        }

        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (m_flagHWConnected() == false) {
                ALOGD("%s::m_flagHWConnected() fail \n", __func__);
                return false;
            }

#if defined(BOARD_USES_EDID)
            if (!EDIDOpen())
                ALOGE("EDIDInit() failed!\n");

            if (!EDIDRead()) {
                ALOGE("EDIDRead() failed!\n");
                if (!EDIDClose())
                    ALOGE("EDIDClose() failed!\n");
            }
#endif

#if defined(BOARD_USES_CEC)
            if (!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif
        }
    }

    if (this->setHdmiOutputMode(mHdmiOutputMode, true) == false)
        ALOGE("%s::setHdmiOutputMode(%d) fail \n", __func__, mHdmiOutputMode);

    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
        if (this->setHdmiResolution(mHdmiResolutionValue, true) == false)
            ALOGE("%s::setHdmiResolution(%d) fail \n", __func__, mHdmiResolutionValue);

        if (this->setHdcpMode(mHdcpMode, false) == false)
            ALOGE("%s::setHdcpMode(%d) fail \n", __func__, mHdcpMode);

/*        if (this->m_setAudioMode(mAudioMode) == false)
            ALOGE("%s::m_setAudioMode(%d) fail \n", __func__, mAudioMode);
*/
        mHdmiInfoChange = true;
        mFlagConnected = true;

#if defined(BOARD_USES_EDID)
        display_menu();
#endif
    }

    return true;
}

bool SecHdmi::disconnect(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (mFlagConnected == false) {
        ALOGE("%s::Already Disconnected.. \n", __func__);
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
            ALOGE("EDIDClose() failed!\n");
            return false;
        }
#endif
    }

    for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
        if (mFlagHdmiStart[layer] == true && m_stopHdmi(layer) == false) {
            ALOGE("%s::hdmiLayer(%d) layer fail \n", __func__, layer);
            return false;
        }
    }

    mFlagConnected = false;
    mPreviousHdmiPresetId = V4L2_DV_1080P60;

    mHdmiOutputMode = DEFAULT_OUPUT_MODE;
    mHdmiResolutionValue = DEFAULT_HDMI_RESOLUTION_VALUE;
    mHdmiStdId = DEFAULT_HDMI_STD_ID;
    mCompositeStd = DEFAULT_COMPOSITE_STD;
    mAudioMode = 2;
    mCurrentHdmiOutputMode = -1;
    mCurrentHdmiResolutionValue = 0;
    mCurrentAudioMode = -1;

    return true;
}

bool SecHdmi::startHdmi(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);
    if (mFlagHdmiStart[hdmiLayer] == false &&
            m_startHdmi(hdmiLayer) == false) {
        ALOGE("%s::hdmiLayer(%d) fail \n", __func__, hdmiLayer);
        return false;
    }
    return true;
}

bool SecHdmi::stopHdmi(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);
    if (mFlagHdmiStart[hdmiLayer] == true &&
            m_stopHdmi(hdmiLayer) == false) {
        ALOGE("%s::hdmiLayer(%d) layer fail \n", __func__, hdmiLayer);
        return false;
    }
    tvout_deinit();
    return true;
}

bool SecHdmi::flagConnected(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
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
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::hdmiLayer=%d", __func__, hdmiLayer);
#endif

    Mutex::Autolock lock(mLock);

#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s [srcW = %d, srcH = %d, srcColorFormat = 0x%x, srcYAddr= 0x%x, srcCbAddr = 0x%x, srcCrAddr = 0x%x, dstX = %d, dstY = %d, hdmiLayer = %d, num_of_hwc_layer=%d",
         __func__, srcW, srcH, srcColorFormat,
                   srcYAddr, srcCbAddr, srcCrAddr,
                   dstX, dstY, hdmiLayer, num_of_hwc_layer);
    ALOGD("saved param(%d, %d, %d)",
            mSrcWidth[hdmiLayer], mSrcHeight[hdmiLayer], mSrcColorFormat[hdmiLayer]);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created", __func__);
        return false;
    }

    if (srcW != mSrcWidth[hdmiLayer] ||
        srcH != mSrcHeight[hdmiLayer] ||
        srcColorFormat != mSrcColorFormat[hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
        num_of_hwc_layer != mPreviousNumofHwLayer[hdmiLayer] ||
        mHdmiInfoChange == true) {
#ifdef DEBUG_MSG_ENABLE
        ALOGD("m_reset param(%d, %d, %d, %d, %d, %d, %d)",
            srcW, mSrcWidth[hdmiLayer],
            srcH, mSrcHeight[hdmiLayer],
            srcColorFormat, mSrcColorFormat[hdmiLayer],
            hdmiLayer);
#endif
        if (m_reset(srcW, srcH, dstX, dstY, srcColorFormat, hdmiLayer, num_of_hwc_layer) == false) {
            ALOGE("%s::m_reset(%d, %d, %d, %d) failed", __func__, srcW, srcH, srcColorFormat, hdmiLayer);
            return false;
        }
    }

    if (srcYAddr == 0) {
        unsigned int FB_size = ALIGN(srcW, 16) * ALIGN(srcH, 16) * HDMI_G2D_BUFFER_BPP_SIZE;

        srcYAddr = (unsigned int)mFBaddr + mFBoffset;
        srcCbAddr = srcYAddr;

        mFBoffset += FB_size;
        if (FB_size < mFBoffset)
            mFBoffset = 0;

#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::mFBaddr=0x%08x, srcYAddr=0x%08x, mFBoffset=0x%08x", __func__, mFBaddr, srcYAddr, mFBoffset);
#endif
    }

    if (hdmiLayer == HDMI_LAYER_VIDEO) {
        if (mSecGscaler.setSrcAddr(srcYAddr, srcCbAddr, srcCrAddr, srcColorFormat) == false) {
            ALOGE("%s::setSrcAddr(0x%08x, 0x%08x, 0x%08x)", __func__, srcYAddr, srcCbAddr, srcCrAddr);
            return false;
        }
    } else {
#if CHECK_GRAPHIC_LAYER_TIME
        nsecs_t start, end;
        start = systemTime();
#endif

        if (num_of_hwc_layer == 0) { /* UI only mode */
            struct v4l2_rect rect;

            if (mG2DUIRotVal == 0 || mG2DUIRotVal == 180) {
                hdmi_cal_rect(srcW, srcH, mHdmiDstWidth, mHdmiDstHeight, &rect);
            } else {
                hdmi_cal_rect(srcH, srcW, mHdmiDstWidth, mHdmiDstHeight, &rect);
            }

            rect.left = ALIGN(rect.left, 16);

            if (hdmi_set_graphiclayer(mSubdevMixerFd, mVideodevFd[hdmiLayer], hdmiLayer,
                            srcColorFormat,
                            srcW, srcH,
                            srcYAddr, &mSrcBuffer[hdmiLayer][mSrcIndex[hdmiLayer]],
                            rect.left, rect.top,
                            rect.width, rect.height,
                            mG2DUIRotVal) < 0)
            return false;

        } else { // Video Playback + UI Mode
            if (hdmi_set_graphiclayer(mSubdevMixerFd, mVideodevFd[hdmiLayer], hdmiLayer,
                            srcColorFormat,
                            srcW, srcH,
                            srcYAddr, &mSrcBuffer[hdmiLayer][mSrcIndex[hdmiLayer]],
                            dstX, dstY,
                            mHdmiDstWidth, mHdmiDstHeight,
                            mG2DUIRotVal) < 0)
            return false;
        }

#if CHECK_GRAPHIC_LAYER_TIME
        end = systemTime();
        ALOGD("[UI] hdmi_gl_set_param[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
    }

    if (mFlagConnected) {
        if (mFlagHdmiStart[hdmiLayer] == true) {
            if (m_run(hdmiLayer) == false) {
                ALOGE("%s::m_run(%d) failed", __func__, hdmiLayer);
                return false;
            }
        }

        if (mFlagHdmiStart[hdmiLayer] == false && m_startHdmi(hdmiLayer) == false) {
            ALOGE("%s::start hdmiLayer(%d) failed", __func__, hdmiLayer);
            return false;
        }
    }
    return true;
}

bool SecHdmi::clear(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::hdmiLayer = %d", __func__, hdmiLayer);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }
    if (mFlagHdmiStart[hdmiLayer] == true && m_stopHdmi(hdmiLayer) == false) {
        ALOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, hdmiLayer);
        return false;
    }
    return true;
}

void SecHdmi::clearGraphicLayer(int hdmiLayer)
{
    mSrcWidth[hdmiLayer] = 0;
    mSrcHeight[hdmiLayer] = 0;
    mSrcColorFormat[hdmiLayer] = 0;
}

bool SecHdmi::enableGraphicLayer(int hdmiLayer)
{
    Mutex::Autolock lock(mLock);
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::hdmiLayer(%d)",__func__, hdmiLayer);
#endif
    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
    case HDMI_LAYER_GRAPHIC_0:
    case HDMI_LAYER_GRAPHIC_1:
        mFlagLayerEnable[hdmiLayer] = true;
        if (mFlagConnected == true)
            m_startHdmi(hdmiLayer);
        break;
    default:
        return false;
    }
    return true;
}

bool SecHdmi::disableGraphicLayer(int hdmiLayer)
{
    Mutex::Autolock lock(mLock);
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::hdmiLayer(%d)",__func__, hdmiLayer);
#endif
    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
    case HDMI_LAYER_GRAPHIC_0:
    case HDMI_LAYER_GRAPHIC_1:
        if (mFlagConnected == true && mFlagLayerEnable[hdmiLayer])
            if (m_stopHdmi(hdmiLayer) == false )
                ALOGE("%s::m_stopHdmi: layer[%d] fail \n", __func__, hdmiLayer);

        mFlagLayerEnable[hdmiLayer] = false;
        break;
    default:
        return false;
    }
    return true;
}

bool SecHdmi::setHdmiOutputMode(int hdmiOutputMode, bool forceRun)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::hdmiOutputMode = %d, forceRun = %d", __func__, hdmiOutputMode, forceRun);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdmiOutputMode == hdmiOutputMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
#endif
        return true;
    }

    int newHdmiOutputMode = hdmiOutputMode;

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        ALOGD("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

#if defined(BOARD_USES_EDID)
    int newV4l2OutputType = hdmi_check_output_mode(v4l2OutputType);
    if (newV4l2OutputType != v4l2OutputType) {
        newHdmiOutputMode = hdmi_v4l2_output_type_2_outputmode(newV4l2OutputType);
        if (newHdmiOutputMode < 0) {
            ALOGD("%s::hdmi_v4l2_output_type_2_outputmode(%d) fail\n", __func__, newV4l2OutputType);
            return false;
        }

        ALOGD("%s::calibration mode(%d -> %d)... \n", __func__, hdmiOutputMode, newHdmiOutputMode);
        mHdmiInfoChange = true;
    }
#endif

    if (mHdmiOutputMode != newHdmiOutputMode) {
        mHdmiOutputMode = newHdmiOutputMode;
        mHdmiInfoChange = true;
    }

    return true;
}

bool SecHdmi::setHdmiResolution(unsigned int hdmiResolutionValue, bool forceRun)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::hdmiResolutionValue = %d, forceRun = %d", __func__, hdmiResolutionValue, forceRun);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdmiResolutionValue == hdmiResolutionValue) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiResolutionValue(%d) \n", __func__, hdmiResolutionValue);
#endif
        return true;
    }

    unsigned int newHdmiResolutionValue = hdmiResolutionValue;
    int w = 0;
    int h = 0;
    v4l2_std_id std_id;
    __u32 preset_id;

#if defined(BOARD_USES_EDID)
    // find perfect resolutions..
    if (hdmi_resolution_2_std_id(newHdmiResolutionValue, &w, &h, &std_id, &preset_id) < 0 ||
        hdmi_check_resolution(std_id) < 0) {
        bool flagFoundIndex = false;
        int resolutionValueIndex = m_resolutionValueIndex(newHdmiResolutionValue);

        for (int i = resolutionValueIndex + 1; i < mHdmiSizeOfResolutionValueList; i++) {
            if (hdmi_resolution_2_std_id(mHdmiResolutionValueList[i], &w, &h, &std_id, &preset_id) == 0 &&
                hdmi_check_resolution(std_id) == 0) {
                newHdmiResolutionValue = mHdmiResolutionValueList[i];
                flagFoundIndex = true;
                break;
            }
        }

        if (flagFoundIndex == false) {
            ALOGE("%s::hdmi cannot control this resolution(%d) fail \n", __func__, hdmiResolutionValue);
            // Set resolution to 480P
            newHdmiResolutionValue = mHdmiResolutionValueList[mHdmiSizeOfResolutionValueList-2];
        } else {
            ALOGD("%s::HDMI resolutions size is calibrated(%d -> %d)..\n", __func__, hdmiResolutionValue, newHdmiResolutionValue);
        }
    } else {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::find resolutions(%d) at once\n", __func__, hdmiResolutionValue);
#endif
    }
#endif

    if (mHdmiResolutionValue != newHdmiResolutionValue) {
        mHdmiResolutionValue = newHdmiResolutionValue;
        mHdmiInfoChange = true;
    }

    return true;
}

bool SecHdmi::setHdcpMode(bool hdcpMode, bool forceRun)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdcpMode == hdcpMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
#endif
        return true;
    }

    mHdcpMode = hdcpMode;
    mHdmiInfoChange = true;

    return true;
}

bool SecHdmi::setUIRotation(unsigned int rotVal, unsigned int hwcLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    Mutex::Autolock lock(mLock);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (rotVal % 90 != 0) {
        ALOGE("%s::Invalid rotation value(%d)", __func__, rotVal);
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
            mSecGscaler.setRotVal(rotVal);
            mUIRotVal = rotVal;
            mHdmiInfoChange = true;
        }
    } else { /* Don't rotate video layer when video is played. */
        rotVal = 0;
        if (rotVal != mUIRotVal) {
            mSecGscaler.setRotVal(rotVal);
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

void SecHdmi::setDisplayInfo(int srcW, int srcH, int srcColorFormat,
        unsigned int srcYAddr, unsigned int srcCbAddr,
        int dstX, int dstY,
        int hdmiLayer,
        int num_of_hwc_layer)
{
#ifdef DEBUG_MSG_ENABLE
ALOGD("%s [srcW = %d, srcH = %d, srcColorFormat = 0x%x, srcYAddr= 0x%x, srcCbAddr = 0x%x, dstX = %d, dstY = %d, hdmiLayer = %d",
            __func__, srcW, srcH, srcColorFormat, srcYAddr, srcCbAddr, dstX, dstY, hdmiLayer);
#endif

    mCurrentsrcW = srcW;
    mCurrentsrcH = srcH;
    mCurrentsrcColorFormat = srcColorFormat;
    mCurrentsrcYAddr = srcYAddr;
    mCurrentsrcCbAddr = srcCbAddr,
    mCurrentdstX = dstX;
    mCurrentdstY = dstY;
    mCurrenthdmiLayer = hdmiLayer;
    mCurrentNumOfHWCLayer = num_of_hwc_layer;

    return;
}

bool SecHdmi::m_setupLink(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif
    int  ret;
    char node[32];
    char subdevname[32];
    char videodevname[32];

    struct v4l2_capability   v4l2cap;
    struct media_entity_desc entity_desc;

    sprintf(node, "%s%d", PFX_NODE_MEDIADEV, 0);
    mMediadevFd = open(node, O_RDWR);
    if (mMediadevFd < 0) {
        ALOGE("%s::open(%s) failed", __func__, node);
        goto err;
    }

    /* open subdev fd */
    sprintf(subdevname, PFX_ENTITY_SUBDEV_MIXER, 0);
    for (__u32 id = 0; ; id = entity_desc.id) {
        entity_desc.id = id | MEDIA_ENT_ID_FLAG_NEXT;

        if (ioctl(mMediadevFd, MEDIA_IOC_ENUM_ENTITIES, &entity_desc) < 0) {
            if (errno == EINVAL) {
                ALOGD("%s::MEDIA_IOC_ENUM_ENTITIES ended", __func__);
                break;
            }
            ALOGE("%s::MEDIA_IOC_ENUM_ENTITIES failed", __func__);
            goto err;
        }
        ALOGD("%s::entity_desc.id=%d, .minor=%d .name=%s", __func__, entity_desc.id, entity_desc.v4l.minor, entity_desc.name);

        if (strncmp(entity_desc.name, subdevname, strlen(subdevname)) == 0)
            mMixerSubdevEntity = entity_desc.id;
    }

    mlink_desc.source.entity = mSecGscaler.getSubdevEntity();
    mlink_desc.source.index  = GSCALER_SUBDEV_PAD_SOURCE;
    mlink_desc.source.flags  = MEDIA_PAD_FL_SOURCE;

    mlink_desc.sink.entity   = mMixerSubdevEntity;
    mlink_desc.sink.index    = MIXER_V_SUBDEV_PAD_SINK;
    mlink_desc.sink.flags    = MEDIA_PAD_FL_SINK;
    mlink_desc.flags         = MEDIA_LNK_FL_ENABLED;

#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::mlink_desc.source.entity=%02d, .pad=%d", __func__, mlink_desc.source.entity, mlink_desc.source.index);
    ALOGD("%s::mlink_desc.sink.entity  =%02d, .pad=%d", __func__, mlink_desc.sink.entity, mlink_desc.sink.index);
#endif

    if (ioctl(mMediadevFd, MEDIA_IOC_SETUP_LINK, &mlink_desc) < 0) {
        ALOGE("%s::MEDIA_IOC_SETUP_LINK [src.entity=%d->sink.entity=%d] failed", __func__, mlink_desc.source.entity, mlink_desc.sink.entity);
        goto err;
    }

    sprintf(node, "%s%d", PFX_NODE_SUBDEV, 4); // Mixer0 minor=132 /dev/v4l-subdev4 // need to modify //carrotsm
    mSubdevMixerFd = open(node, O_RDWR, 0);
    if (mSubdevMixerFd < 0) {
        ALOGE("%s::open(%s) failed", __func__, node);
        goto err;
    }

    if (0 < mMediadevFd)
        close(mMediadevFd);
    mMediadevFd = -1;

   return true;

err :

    if (0 < mMediadevFd)
        close(mMediadevFd);

    if (0 < mSubdevMixerFd)
        close(mSubdevMixerFd);

    mMediadevFd = -1;
    mSubdevMixerFd = -1;

    return false;
}

bool SecHdmi::m_openLayer(int layer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::layer=%d", __func__, layer);
#endif
    char node[32];

    switch (layer) {
    case HDMI_LAYER_VIDEO:
        mVideodevFd[layer] = mSecGscaler.getVideodevFd();

        if (0 < mVideodevFd[layer]) {
            ALOGD("%s::Layer[%d] device already opened", __func__, layer);
            return true;
        }

        if (mSecGscaler.openVideodevFd() == false)
            ALOGE("%s::open(%s) failed", __func__, node);
        else
            mVideodevFd[layer] = mSecGscaler.getVideodevFd();

        goto open_success;
        break;
    case HDMI_LAYER_GRAPHIC_0:
        sprintf(node, "%s", TVOUT0_DEV_G0);
        break;
    case HDMI_LAYER_GRAPHIC_1:
        sprintf(node, "%s", TVOUT0_DEV_G1);
        break;
    default:
        ALOGE("%s::unmatched layer[%d]", __func__, layer);
        return false;
        break;
    }

    mVideodevFd[layer] = open(node, O_RDWR);
    if (mVideodevFd[layer] < 0) {
        ALOGE("%s::open(%s) failed", __func__, node);
        goto err;
    }

open_success :

#ifdef DEBUG_MSG_ENABLE
    ALOGD("layer=%d, mVideodevFd=%d", layer, mVideodevFd[layer]);
#endif

    if (tvout_std_v4l2_querycap(mVideodevFd[layer], node) < 0 ) {
        ALOGE("%s::tvout_std_v4l2_querycap failed", __func__);
        goto err;
    }

    return true;

err :

    if (0 < mVideodevFd[layer])
        close(mVideodevFd[layer]);

    mVideodevFd[layer] = -1;

    return false;

}

bool SecHdmi::m_closeLayer(int layer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::layer=%d", __func__, layer);
#endif
    switch (layer) {
    case HDMI_LAYER_VIDEO:
        mVideodevFd[layer] = mSecGscaler.getVideodevFd();

        if (mVideodevFd[layer] < 0) {
            ALOGD("%s::Layer[%d] device already closed", __func__, layer);
            return true;
        } else {
            mSecGscaler.closeVideodevFd();
            mVideodevFd[layer] = mSecGscaler.getVideodevFd();
        }
        goto close_success;
        break;
    case HDMI_LAYER_GRAPHIC_0:
    case HDMI_LAYER_GRAPHIC_1:
        /* clear buffer */
        if (0 < mVideodevFd[layer]) {
            if (tvout_std_v4l2_reqbuf(mVideodevFd[layer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, 0) < 0) {
                ALOGE("%s::tvout_std_v4l2_reqbuf(buf_num=%d)[graphic layer] failed", __func__, 0);
                return -1;
            }
        }
        break;
    default:
        ALOGE("%s::unmatched layer[%d]", __func__, layer);
        return false;
        break;
    }

    if (0 < mVideodevFd[layer]) {
        if (close(mVideodevFd[layer]) < 0) {
            ALOGE("%s::close %d layer failed", __func__, layer);
            return false;
        }
    }

    mVideodevFd[layer] = -1;

close_success :

    return true;
}

bool SecHdmi::m_reset(int w, int h, int dstX, int dstY, int colorFormat, int hdmiLayer, int num_of_hwc_layer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::w=%d, h=%d, dstX=%d, dstY=%d, colorFormat=%d, hdmiLayer=%d, num_of_hwc_layer=%d",
        __func__, w, h, dstX, dstY, colorFormat, hdmiLayer, num_of_hwc_layer);
#endif

    v4l2_std_id std_id = 0;

    int srcW = w;
    int srcH = h;
    int dstW = 0;
    int dstH = 0;

    if (mFlagHdmiStart[hdmiLayer] == true && m_stopHdmi(hdmiLayer) == false) {
        ALOGE("%s::m_stopHdmi: layer[%d] failed", __func__, hdmiLayer);
        return false;
    }

    if (m_closeLayer(hdmiLayer) == false) {
        ALOGE("%s::m_closeLayer: layer[%d] failed", __func__, hdmiLayer);
        return false;
    }

    if (m_openLayer(hdmiLayer) == false) {
        ALOGE("%s::m_closeLayer: layer[%d] failed", __func__, hdmiLayer);
        return false;
    }

    if (w != mSrcWidth[hdmiLayer] ||
        h != mSrcHeight[hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
        num_of_hwc_layer != mPreviousNumofHwLayer[hdmiLayer] ||
        colorFormat != mSrcColorFormat[hdmiLayer] ||
        mHdmiInfoChange == true) {
        int preVideoSrcColorFormat = mSrcColorFormat[hdmiLayer];
        int videoSrcColorFormat = colorFormat;

        if (preVideoSrcColorFormat != HAL_PIXEL_FORMAT_YCbCr_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP &&
            preVideoSrcColorFormat != HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED) {
                ALOGI("%s: Unsupported preVideoSrcColorFormat = 0x%x", __func__, preVideoSrcColorFormat);
                preVideoSrcColorFormat = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED;
        }

        if (hdmiLayer == HDMI_LAYER_VIDEO) {
            unsigned int full_wdith  = ALIGN(w, 16);
            unsigned int full_height = ALIGN(h, 16);

            if (mSecGscaler.setSrcParams(full_wdith, full_height, 0, 0,
                        (unsigned int*)&w, (unsigned int*)&h, colorFormat, true) == false) {
                ALOGE("%s::mSecGscaler.setSrcParams(w=%d, h=%d, color=%d) failed",
                        __func__, w, h, colorFormat);
                return false;
            }
            mGscalerDstColorFormat = V4L2_MBUS_FMT_YUV8_1X24;

            /* calculate destination buffer width and height */
            struct v4l2_rect rect;

            if (mUIRotVal == 0 || mUIRotVal == 180) {
                hdmi_cal_rect(srcW, srcH, mHdmiDstWidth, mHdmiDstHeight, &rect);
            } else {
                hdmi_cal_rect(srcH, srcW, mHdmiDstWidth, mHdmiDstHeight, &rect);
            }

            rect.width = ALIGN(rect.width, 16);

            if (mSecGscaler.setDstParams((unsigned int)rect.width, (unsigned int)rect.height, 0, 0,
                        (unsigned int*)&rect.width, (unsigned int*)&rect.height, mGscalerDstColorFormat, true) == false) {
                ALOGE("%s::mSecGscaler.setDstParams(w=%d, h=%d, V4L2_MBUS_FMT_YUV8_1X24) failed",
                        __func__, rect.width, rect.height);
                return false;
            }

            hdmi_set_videolayer(mSubdevMixerFd, mHdmiDstWidth, mHdmiDstHeight, &rect);
        } else {
            if (tvout_std_v4l2_s_ctrl(mVideodevFd[hdmiLayer], V4L2_CID_TV_LAYER_BLEND_ENABLE, 1) < 0) {
                ALOGE("%s::tvout_std_v4l2_s_ctrl [layer=%d] (V4L2_CID_TV_LAYER_BLEND_ENABLE) failed", __func__, hdmiLayer);
                return false;
            }

            if (tvout_std_v4l2_s_ctrl(mVideodevFd[hdmiLayer], V4L2_CID_TV_PIXEL_BLEND_ENABLE, 1) < 0) {
                ALOGE("%s::tvout_std_v4l2_s_ctrl [layer=%d] (V4L2_CID_TV_LAYER_BLEND_ENABLE) failed", __func__, hdmiLayer);
                return false;
            }

            if (tvout_std_v4l2_s_ctrl(mVideodevFd[hdmiLayer], V4L2_CID_TV_LAYER_BLEND_ALPHA, 255) < 0) {
                ALOGE("%s::tvout_std_v4l2_s_ctrl [layer=%d] (V4L2_CID_TV_LAYER_BLEND_ALPHA) failed", __func__, hdmiLayer);
                return false;
            }

            struct v4l2_rect rect;
            int tempSrcW, tempSrcH;
            int gr_frame_size = 0;

            if (mG2DUIRotVal == 0 || mG2DUIRotVal == 180) {
                tempSrcW = srcW;
                tempSrcH = srcH;
            } else {
                tempSrcW = srcH;
                tempSrcH = srcW;
            }

            hdmi_cal_rect(tempSrcW, tempSrcH, mHdmiDstWidth, mHdmiDstHeight, &rect);
            rect.left = ALIGN(rect.left, 16);

            if (num_of_hwc_layer == 0) { /* UI only mode */
                hdmi_set_g_Params(mSubdevMixerFd, mVideodevFd[hdmiLayer], hdmiLayer,
                                  colorFormat, srcW, srcH,
                                  rect.left, rect.top, rect.width, rect.height);
                dstW = rect.width;
                dstH = rect.height;
            } else { /* Video Playback + UI Mode */
                hdmi_set_g_Params(mSubdevMixerFd, mVideodevFd[hdmiLayer], hdmiLayer,
                                  colorFormat, srcW, srcH,
                                  dstX, dstY, mHdmiDstWidth, mHdmiDstHeight);
                dstW = mHdmiDstWidth;
                dstH = mHdmiDstHeight;
            }

#if defined(BOARD_USES_HDMI_FIMGAPI)
            gr_frame_size = dstW * dstH;
#else
            gr_frame_size = srcW * srcH;
#endif
            for (int buf_index = 0; buf_index < MAX_BUFFERS_MIXER; buf_index++) {
                int v4l2ColorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(colorFormat);
                switch (v4l2ColorFormat) {
                case V4L2_PIX_FMT_BGR32:
                case V4L2_PIX_FMT_RGB32:
                    mSrcBuffer[hdmiLayer][buf_index].size.s = gr_frame_size << 2;
                    break;
                case V4L2_PIX_FMT_RGB565X:
                    mSrcBuffer[hdmiLayer][buf_index].size.s = gr_frame_size << 1;
                    break;
                default:
                    ALOGE("%s::invalid color type", __func__);
                    return false;
                    break;
                }
            }

        }

        if (preVideoSrcColorFormat != videoSrcColorFormat)
            mHdmiInfoChange = true;

        mSrcWidth[hdmiLayer] = srcW;
        mSrcHeight[hdmiLayer] = srcH;
        mSrcColorFormat[hdmiLayer] = colorFormat;

        mHdmiResolutionWidth[hdmiLayer] = mHdmiDstWidth;
        mHdmiResolutionHeight[hdmiLayer] = mHdmiDstHeight;
        mPreviousNumofHwLayer[hdmiLayer] = num_of_hwc_layer;

#ifdef DEBUG_MSG_ENABLE
        ALOGD("m_reset saved param(%d, %d, %d, %d, %d, %d, %d)",
            srcW, mSrcWidth[hdmiLayer], \
            srcH, mSrcHeight[hdmiLayer], \
            colorFormat,mSrcColorFormat[hdmiLayer], \
            hdmiLayer);
#endif
    }

    if (mHdmiInfoChange == true) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("mHdmiInfoChange: %d", mHdmiInfoChange);
#endif

#if defined(BOARD_USES_CEC)
        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (mCECThread->mFlagRunning)
                mCECThread->stop();
        }
#endif

        if (m_setHdmiOutputMode(mHdmiOutputMode) == false) {
            ALOGE("%s::m_setHdmiOutputMode() failed", __func__);
            return false;
        }
        if (mHdmiOutputMode == COMPOSITE_OUTPUT_MODE) {
            std_id = composite_std_2_v4l2_std_id(mCompositeStd);
            if ((int)std_id < 0) {
                ALOGE("%s::composite_std_2_v4l2_std_id(%d) failed", __func__, mCompositeStd);
                return false;
            }
            if (m_setCompositeResolution(mCompositeStd) == false) {
                ALOGE("%s::m_setCompositeRsolution() failed", __func__);
                return false;
            }
        } else if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
                   mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
            if (m_setHdmiResolution(mHdmiResolutionValue) == false) {
                ALOGE("%s::m_setHdmiResolution() failed", __func__);
                return false;
            }

            if (m_setHdcpMode(mHdcpMode) == false) {
                ALOGE("%s::m_setHdcpMode() failed", __func__);
                return false;
            }
            std_id = mHdmiStdId;
        }

        if (mPreviousHdmiPresetId != mHdmiPresetId) {
            for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
                if (m_stopHdmi(layer) == false) {
                    ALOGE("%s::m_stopHdmi(%d) failed", __func__, layer);
                    return false;
                }
            }

            if (tvout_init(mVideodevFd[HDMI_LAYER_GRAPHIC_0], mHdmiPresetId) < 0) {
                ALOGE("%s::tvout_init(mHdmiPresetId=%d) failed", __func__, mHdmiPresetId);
                return false;
            }
            mPreviousHdmiPresetId = mHdmiPresetId;
        }

        if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
            mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
#if defined(BOARD_USES_CEC)
            if (!(mCECThread->mFlagRunning))
                mCECThread->start();
#endif

/*            if (m_setAudioMode(mAudioMode) == false)
                ALOGE("%s::m_setAudioMode() failed", __func__);
*/
        }

        mHdmiInfoChange = false;

    }

    return true;
}

bool SecHdmi::m_streamOn(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::hdmiLayer = %d", __func__, hdmiLayer);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagHdmiStart[hdmiLayer] == true) {
        ALOGE("%s::[layer=%d] already streamon", __func__, hdmiLayer);
        return true;
    }

    switch(hdmiLayer) {
    case HDMI_LAYER_GRAPHIC_0:
        break;
    case HDMI_LAYER_GRAPHIC_1:
        break;
    default :
        ALOGE("%s::unmathced layer(%d) failed", __func__, hdmiLayer);
        return false;
        break;
    }

    if (tvout_std_v4l2_qbuf(mVideodevFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR,
                            mSrcIndex[hdmiLayer], 1, &mSrcBuffer[hdmiLayer][0]) < 0) {
        ALOGE("%s::gsc_v4l2_queue(index : %d) (mSrcBufNum : %d) failed", __func__, mSrcIndex[hdmiLayer], 1);
        return false;
    }

    mSrcIndex[hdmiLayer]++;
    if (mSrcIndex[hdmiLayer] == MAX_BUFFERS_MIXER) {
        if (tvout_std_v4l2_streamon(mVideodevFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
            ALOGE("%s::gsc_v4l2_stream_on() failed", __func__);
            return false;
        }
        mFlagHdmiStart[hdmiLayer] = true;
    }

    if (mSrcIndex[hdmiLayer] >= MAX_BUFFERS_MIXER) {
        mSrcIndex[hdmiLayer] = 0;
    }

    return true;
}

bool SecHdmi::m_run(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::hdmiLayer = %d", __func__, hdmiLayer);
#endif

    int buf_index = 0;

    if (mFlagHdmiStart[hdmiLayer] == false || mFlagLayerEnable[hdmiLayer] == false) {
        ALOGD("%s::HDMI(%d layer) started not yet", __func__, hdmiLayer);
        return true;
    }

    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
        if (mSecGscaler.run() == false) {
            ALOGE("%s::mSecGscaler.draw() failed", __func__);
            return false;
        }
        break;
    case HDMI_LAYER_GRAPHIC_0 :
    case HDMI_LAYER_GRAPHIC_1 :
        if (tvout_std_v4l2_dqbuf(mVideodevFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, &buf_index, 1) < 0) {
            ALOGE("%s::tvout_std_v4l2_dqbuf(mNumOfBuf : %d, dqIndex=%d) failed", __func__, 1, buf_index);
            return false;
        }

        if (tvout_std_v4l2_qbuf(mVideodevFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR,
                                mSrcIndex[hdmiLayer], 1, &mSrcBuffer[hdmiLayer][mSrcIndex[hdmiLayer]]) < 0) {
            ALOGE("%s::tvout_std_v4l2_qbuf(mNumOfBuf : %d,mSrcIndex=%d) failed", __func__, 1, mSrcIndex[hdmiLayer]);
            return false;
        }

        mSrcIndex[hdmiLayer]++;
        if (mSrcIndex[hdmiLayer] >= MAX_BUFFERS_MIXER) {
            mSrcIndex[hdmiLayer] = 0;
        }

        break;
    default :
        ALOGE("%s::unmathced layer(%d) failed", __func__, hdmiLayer);
        return false;
        break;
    }

    return true;
}

bool SecHdmi::m_startHdmi(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::hdmiLayer = %d", __func__, hdmiLayer);
#endif

    int buf_index = 0;

    if (mFlagHdmiStart[hdmiLayer] == true) {
        ALOGD("%s::already HDMI(%d layer) started..", __func__, hdmiLayer);
        return true;
    }

#ifdef DEBUG_MSG_ENABLE
    ALOGD("### %s: hdmiLayer(%d) called", __func__, hdmiLayer);
#endif
    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
        if (mSecGscaler.streamOn() == false) {
            ALOGE("%s::mSecGscaler.streamOn() failed", __func__);
            return false;
        }
        if (mSecGscaler.getFlagSteamOn() == true)
            mFlagHdmiStart[hdmiLayer] = true;
        break;
    case HDMI_LAYER_GRAPHIC_0 :
    case HDMI_LAYER_GRAPHIC_1 :
        if (m_streamOn(hdmiLayer) == false) {
            ALOGE("%s::m_streamOn layer(%d) failed", __func__, hdmiLayer);
            return false;
        }
        break;
    default :
        ALOGE("%s::unmathced layer(%d) failed", __func__, hdmiLayer);
        return false;
        break;
    }
    return true;
}

bool SecHdmi::m_stopHdmi(int hdmiLayer)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s::hdmiLayer = %d", __func__, hdmiLayer);
#endif

    if (mFlagHdmiStart[hdmiLayer] == false) {
        ALOGD("%s::already HDMI(%d layer) stopped..", __func__, hdmiLayer);
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s : layer[%d] called", __func__, hdmiLayer);
#endif

    switch (hdmiLayer) {
     case HDMI_LAYER_VIDEO:
        if (mSecGscaler.streamOff() == false) {
            ALOGE("%s::mSecGscaler.streamOff() failed", __func__);
            return false;
        }
        mFlagHdmiStart[hdmiLayer] = false;
        break;
    case HDMI_LAYER_GRAPHIC_1 :
    case HDMI_LAYER_GRAPHIC_0 :
#if defined(BOARD_USES_HDMI_FIMGAPI)
        cur_g2d_address = 0;
#endif
        if (tvout_std_v4l2_streamoff(mVideodevFd[hdmiLayer], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
            ALOGE("%s::tvout_std_v4l2_streamon layer(%d) failed", __func__, hdmiLayer);
            return false;
        }

        mSrcIndex[hdmiLayer] = 0;
        mFlagHdmiStart[hdmiLayer] = false;
        break;
    default :
        ALOGE("%s::unmathced layer(%d) failed", __func__, hdmiLayer);
        return false;
        break;
    }

    return true;
}

bool SecHdmi::m_setHdmiOutputMode(int hdmiOutputMode)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (hdmiOutputMode == mCurrentHdmiOutputMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiOutputMode(%d) \n", __func__, hdmiOutputMode);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        ALOGE("%s::hdmi_outputmode_2_v4l2_output_type(%d) fail\n", __func__, hdmiOutputMode);
        return false;
    }

    output_type = v4l2OutputType;

    mCurrentHdmiOutputMode = hdmiOutputMode;

    return true;
}

bool SecHdmi::m_setCompositeResolution(unsigned int compositeStdId)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    int w = 0;
    int h = 0;

    if (mHdmiOutputMode != COMPOSITE_OUTPUT_MODE) {
        ALOGE("%s::not supported output type \n", __func__);
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
        ALOGE("%s::unmathced composite_std(%d)", __func__, compositeStdId);
        return false;
    }

    t_std_id      = composite_std_2_v4l2_std_id(mCompositeStd);

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = -1;
    return true;
}

bool SecHdmi::m_setHdmiResolution(unsigned int hdmiResolutionValue)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (hdmiResolutionValue == mCurrentHdmiResolutionValue) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdmiResolutionValue(%d) \n", __func__, hdmiResolutionValue);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    int w = 0;
    int h = 0;

    v4l2_std_id std_id;
    if (mHdmiOutputMode >= HDMI_OUTPUT_MODE_YCBCR &&
        mHdmiOutputMode <= HDMI_OUTPUT_MODE_DVI) {
        if (hdmi_resolution_2_std_id(hdmiResolutionValue, &w, &h, &std_id, &mHdmiPresetId) < 0) {
            ALOGE("%s::hdmi_resolution_2_std_id(%d) fail\n", __func__, hdmiResolutionValue);
            return false;
        }
        mHdmiStdId    = std_id;
    } else {
        ALOGE("%s::not supported output type \n", __func__);
        return false;
    }

    t_std_id      = std_id;

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;

    mCurrentHdmiResolutionValue = hdmiResolutionValue;

#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::mHdmiDstWidth = %d, mHdmiDstHeight = %d, mHdmiStdId = 0x%x, hdmiResolutionValue = 0x%x\n",
                __func__,
                mHdmiDstWidth,
                mHdmiDstHeight,
                mHdmiStdId,
                hdmiResolutionValue);
#endif

    return true;
}

bool SecHdmi::m_setHdcpMode(bool hdcpMode)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (hdcpMode == mCurrentHdcpMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same hdcpMode(%d) \n", __func__, hdcpMode);
#endif

        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    if (hdcpMode == true)
        g_hdcp_en = 1;
    else
        g_hdcp_en = 0;

    mCurrentHdcpMode = hdcpMode;

    return true;
}

bool SecHdmi::m_setAudioMode(int audioMode)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    if (audioMode == mCurrentAudioMode) {
#ifdef DEBUG_HDMI_HW_LEVEL
        ALOGD("%s::same audioMode(%d) \n", __func__, audioMode);
#endif
        return true;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    if (hdmi_check_audio(mVideodevFd[HDMI_LAYER_GRAPHIC_0]) < 0) {
        ALOGE("%s::hdmi_check_audio() fail \n", __func__);
        return false;
    }

    mCurrentAudioMode = audioMode;

    return true;
}

int SecHdmi::m_resolutionValueIndex(unsigned int ResolutionValue)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

    int index = -1;

    for (int i = 0; i < mHdmiSizeOfResolutionValueList; i++) {
        if (mHdmiResolutionValueList[i] == ResolutionValue) {
            index = i;
            break;
        }
    }
    return index;
}

bool SecHdmi::m_flagHWConnected(void)
{
#ifdef DEBUG_MSG_ENABLE
    ALOGD("%s", __func__);
#endif

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("### %s called\n", __func__);
#endif

    bool ret = true;
    int hdmiStatus = hdmi_cable_status();

    if (hdmiStatus <= 0) {
#ifdef DEBUG_HDMI_HW_LEVEL
            ALOGD("%s::hdmi_cable_status() fail \n", __func__);
#endif
        ret = false;
    } else {
        ret = true;
    }

    return ret;
}

}; // namespace android
