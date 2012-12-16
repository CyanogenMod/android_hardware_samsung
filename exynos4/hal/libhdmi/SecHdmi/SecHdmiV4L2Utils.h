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

//#define LOG_NDEBUG 0
//#define LOG_TAG "libhdmi"
#include "fimd_api.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace android {

void display_menu(void);

int tvout_open(const char *fp_name);
int tvout_init(v4l2_std_id std_id);
int tvout_deinit();
int tvout_v4l2_querycap(int fp);
int tvout_v4l2_g_std(int fp, v4l2_std_id *std_id);
int tvout_v4l2_s_std(int fp, v4l2_std_id std_id);
int tvout_v4l2_enum_std(int fp, struct v4l2_standard *std, v4l2_std_id std_id);
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output);
int tvout_v4l2_s_output(int fp, int index);
int tvout_v4l2_g_output(int fp, int *index);
int tvout_v4l2_enum_fmt(int fp, struct v4l2_fmtdesc *desc);
int tvout_v4l2_g_fmt(int fp, int buf_type, void* ptr);
int tvout_v4l2_s_fmt(int fp, int buf_type, void *ptr);
int tvout_v4l2_g_fbuf(int fp, struct v4l2_framebuffer *frame);
int tvout_v4l2_s_fbuf(int fp, struct v4l2_framebuffer *frame);
int tvout_v4l2_s_baseaddr(int fp, void *base_addr);
int tvout_v4l2_g_crop(int fp, unsigned int type, struct v4l2_rect *rect);
int tvout_v4l2_s_crop(int fp, unsigned int type, struct v4l2_rect *rect);
int tvout_v4l2_start_overlay(int fp);
int tvout_v4l2_stop_overlay(int fp);

int hdmi_init_layer(int layer);
int hdmi_deinit_layer(int layer);
int hdmi_set_v_param(int layer,
        int src_w, int src_h, int colorFormat,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_w, int dst_h);
int hdmi_gl_set_param(int layer,
        int srcColorFormat,
        int src_w, int src_h,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_x, int dst_y, int dst_w, int dst_h,
        int rotVal);
void hdmi_cal_rect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect);
int hdmi_cable_status();
int hdmi_outputmode_2_v4l2_output_type(int output_mode);
int hdmi_v4l2_output_type_2_outputmode(int v4l2_output_type);
int composite_std_2_v4l2_std_id(int std);

int hdmi_check_output_mode(int v4l2_output_type);
int hdmi_check_resolution(v4l2_std_id std_id);
int hdmi_resolution_2_std_id(unsigned int resolution, unsigned int s3dMode, int *w, int *h, v4l2_std_id *std_id);
int hdmi_enable_hdcp(unsigned int hdcp_en);
int hdmi_check_audio(void);

#ifdef __cplusplus
}
#endif

}  //namespace android

#endif //__HDMI_HAL_V4L2_UTILS_H__
