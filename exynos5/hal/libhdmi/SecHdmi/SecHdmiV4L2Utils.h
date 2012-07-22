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

#ifndef __HDMI_HAL_V4L2_UTILS_H__
#define __HDMI_HAL_V4L2_UTILS_H__

//#define ALOG_NDEBUG 0
//#define ALOG_TAG "libhdmi"

#include "fimd_api.h"
#include "SecBuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace android {

void display_menu(void);

int tvout_init(int fd_tvout, __u32 preset_id);
int tvout_deinit();
int tvout_std_v4l2_querycap(int fd, char *node);
int tvout_std_v4l2_s_fmt(int fd, enum v4l2_buf_type type, enum v4l2_field field, int w, int h, int colorformat, int num_planes);
int tvout_std_v4l2_s_crop(int fd, enum v4l2_buf_type type, enum v4l2_field field, int x, int y, int w, int h);
int tvout_std_v4l2_s_ctrl(int fd, int id, int value);
int tvout_std_v4l2_reqbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int num_bufs);
int tvout_std_v4l2_querybuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int buf_index, unsigned int num_planes, SecBuffer *secBuf);
int tvout_std_v4l2_qbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, int buf_index, int num_planes, SecBuffer *secBuf);
int tvout_std_v4l2_dqbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, int *buf_index, int num_planes);
int tvout_std_v4l2_streamon(int fd, enum v4l2_buf_type type);
int tvout_std_v4l2_streamoff(int fd, enum v4l2_buf_type type);

int tvout_v4l2_enum_output(int fp, struct v4l2_output *output);
int tvout_v4l2_s_output(int fp, int index);
int tvout_v4l2_g_output(int fp, int *index);
int tvout_std_v4l2_enum_dv_presets(int fd);
int tvout_std_v4l2_s_dv_preset(int fd, struct v4l2_dv_preset *preset);
int tvout_std_subdev_s_fmt(int fd, unsigned int pad, int w, int h, enum v4l2_mbus_pixelcode code);
int tvout_std_subdev_s_crop(int fd, unsigned int pad, int w, int h, int x, int y);

void hdmi_cal_rect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect);
int hdmi_set_videolayer(int fd, int hdmiW, int hdmiH, struct v4l2_rect * rect);
int hdmi_set_graphiclayer(int fd_subdev, int fd_videodev,int layer,
        int srcColorFormat,
        int src_w, int src_h,
        unsigned int src_address, SecBuffer * dstBuffer,
        int dst_x, int dst_y, int dst_w, int dst_h,
        int rotVal);
int hdmi_set_g_Params(int fd_subdev, int fd_videodev, int layer,
                      int srcColorFormat,
                      int src_w, int src_h,
                      int dst_x, int dst_y, int dst_w, int dst_h);

int hdmi_cable_status();
int hdmi_outputmode_2_v4l2_output_type(int output_mode);
int hdmi_v4l2_output_type_2_outputmode(int v4l2_output_type);
int composite_std_2_v4l2_std_id(int std);

int hdmi_check_output_mode(int v4l2_output_type);
int hdmi_check_resolution(v4l2_std_id std_id);

int hdmi_resolution_2_std_id(unsigned int resolution, int *w, int *h, v4l2_std_id *std_id, __u32 *preset_id);
int hdmi_enable_hdcp(int fd, unsigned int hdcp_en);
int hdmi_check_audio(int fd);

#ifdef __cplusplus
}
#endif

}  //namespace android

#endif //__HDMI_HAL_V4L2_UTILS_H__
