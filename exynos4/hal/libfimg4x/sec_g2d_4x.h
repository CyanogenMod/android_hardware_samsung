/*
 * Copyright 2011, Samsung Electronics Co. LTD
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
#ifndef __SEC_G2D_4X_H
#define __SEC_G2D_4X_H __FILE__

#define SEC_G2D_DEV_NAME             "/dev/fimg2d"

/* ioctl commands */
#define FIMG2D_IOCTL_MAGIC           'F'
#define FIMG2D_BITBLT_BLIT           _IOWR(FIMG2D_IOCTL_MAGIC, 0, struct fimg2d_blit)
#define FIMG2D_BITBLT_SYNC           _IO(FIMG2D_IOCTL_MAGIC, 1)
#define FIMG2D_BITBLT_VERSION        _IOR(FIMG2D_IOCTL_MAGIC, 2, struct fimg2d_version)

#define G2D_ALPHA_VALUE_MAX          (255)

enum addr_space {
    ADDR_UNKNOWN,
    ADDR_PHYS,
    ADDR_KERN,
    ADDR_USER,
    ADDR_DEVICE,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum pixel_order {
    AX_RGB = 0,
    RGB_AX,
    AX_BGR,
    BGR_AX,
    ARGB_ORDER_END,

    P1_CRY1CBY0,
    P1_CBY1CRY0,
    P1_Y1CRY0CB,
    P1_Y1CBY0CR,
    P1_ORDER_END,

    P2_CRCB,
    P2_CBCR,
    P2_ORDER_END,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum color_format {
    CF_XRGB_8888 = 0,
    CF_ARGB_8888,
    CF_RGB_565,
    CF_XRGB_1555,
    CF_ARGB_1555,
    CF_XRGB_4444,
    CF_ARGB_4444,
    CF_RGB_888,
    CF_YCBCR_444,
    CF_YCBCR_422,
    CF_YCBCR_420,
    CF_A8,
    CF_L8,
    SRC_DST_FORMAT_END,

    CF_MSK_1BIT,
    CF_MSK_4BIT,
    CF_MSK_8BIT,
    CF_MSK_16BIT_565,
    CF_MSK_16BIT_1555,
    CF_MSK_16BIT_4444,
    CF_MSK_32BIT_8888,
    MSK_FORMAT_END,
};

enum rotation {
    ORIGIN,
    ROT_90,    /* clockwise */
    ROT_180,
    ROT_270,
    XFLIP,    /* x-axis flip */
    YFLIP,    /* y-axis flip */
};

/**
 * @NO_REPEAT: no effect
 * @REPEAT_NORMAL: repeat horizontally and vertically
 * @REPEAT_PAD: pad with pad color
 * @REPEAT_REFLECT: reflect horizontally and vertically
 * @REPEAT_CLAMP: pad with edge color of original image
 *
 * DO NOT CHANGE THIS ORDER
 */
enum repeat {
    NO_REPEAT = 0,
    REPEAT_NORMAL,    /* default setting */
    REPEAT_PAD,
    REPEAT_REFLECT, REPEAT_MIRROR = REPEAT_REFLECT,
    REPEAT_CLAMP,
};

enum scaling {
    NO_SCALING,
    SCALING_NEAREST,
    SCALING_BILINEAR,
};

/**
 * @SCALING_PERCENTAGE: percentage of width, height
 * @SCALING_PIXELS: coordinate of src, dest
 */
enum scaling_factor {
    SCALING_PERCENTAGE,
    SCALING_PIXELS,
};

/**
 * premultiplied alpha
 */
enum premultiplied {
    PREMULTIPLIED,
    NON_PREMULTIPLIED,
};

/**
 * @TRANSP: discard bluescreen color
 * @BLUSCR: replace bluescreen color with background color
 */
enum bluescreen {
    OPAQUE,
    TRANSP,
    BLUSCR,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum blit_op {
    BLIT_OP_SOLID_FILL = 0,

    BLIT_OP_CLR,
    BLIT_OP_SRC, BLIT_OP_SRC_COPY = BLIT_OP_SRC,
    BLIT_OP_DST,
    BLIT_OP_SRC_OVER,
    BLIT_OP_DST_OVER, BLIT_OP_OVER_REV = BLIT_OP_DST_OVER,
    BLIT_OP_SRC_IN,
    BLIT_OP_DST_IN, BLIT_OP_IN_REV = BLIT_OP_DST_IN,
    BLIT_OP_SRC_OUT,
    BLIT_OP_DST_OUT, BLIT_OP_OUT_REV = BLIT_OP_DST_OUT,
    BLIT_OP_SRC_ATOP,
    BLIT_OP_DST_ATOP, BLIT_OP_ATOP_REV = BLIT_OP_DST_ATOP,
    BLIT_OP_XOR,

    BLIT_OP_ADD,
    BLIT_OP_MULTIPLY,
    BLIT_OP_SCREEN,
    BLIT_OP_DARKEN,
    BLIT_OP_LIGHTEN,

    BLIT_OP_DISJ_SRC_OVER,
    BLIT_OP_DISJ_DST_OVER, BLIT_OP_SATURATE = BLIT_OP_DISJ_DST_OVER,
    BLIT_OP_DISJ_SRC_IN,
    BLIT_OP_DISJ_DST_IN, BLIT_OP_DISJ_IN_REV = BLIT_OP_DISJ_DST_IN,
    BLIT_OP_DISJ_SRC_OUT,
    BLIT_OP_DISJ_DST_OUT, BLIT_OP_DISJ_OUT_REV = BLIT_OP_DISJ_DST_OUT,
    BLIT_OP_DISJ_SRC_ATOP,
    BLIT_OP_DISJ_DST_ATOP, BLIT_OP_DISJ_ATOP_REV = BLIT_OP_DISJ_DST_ATOP,
    BLIT_OP_DISJ_XOR,

    BLIT_OP_CONJ_SRC_OVER,
    BLIT_OP_CONJ_DST_OVER, BLIT_OP_CONJ_OVER_REV = BLIT_OP_CONJ_DST_OVER,
    BLIT_OP_CONJ_SRC_IN,
    BLIT_OP_CONJ_DST_IN, BLIT_OP_CONJ_IN_REV = BLIT_OP_CONJ_DST_IN,
    BLIT_OP_CONJ_SRC_OUT,
    BLIT_OP_CONJ_DST_OUT, BLIT_OP_CONJ_OUT_REV = BLIT_OP_CONJ_DST_OUT,
    BLIT_OP_CONJ_SRC_ATOP,
    BLIT_OP_CONJ_DST_ATOP, BLIT_OP_CONJ_ATOP_REV = BLIT_OP_CONJ_DST_ATOP,
    BLIT_OP_CONJ_XOR,

    /* Add new operation type here */

    /* user select coefficient manually */
    BLIT_OP_USER_COEFF,

    /* end of blit operation */
    BLIT_OP_END,

    /* driver not supporting format */
    BLIT_OP_NOT_SUPPORTED
};

#define MAX_FIMG2D_BLIT_OP (int)BLIT_OP_END

struct fimg2d_version {
    unsigned int hw;
    unsigned int sw;
};

/**
 * @start: start address or unique id of image
 * @size: whole length of allocated image
 * @cacheable: memory is cacheable
 * @pinnable: memory is pinnable. currently not supported.
 */
struct fimg2d_addr {
    enum addr_space type;
    unsigned long start;
    size_t size;
    int cacheable;
    int pinnable;
};

struct fimg2d_rect {
    int x1;
    int y1;
    int x2;    /* x1 + width */
    int y2; /* y1 + height */
};

/**
 * if factor is percentage, scale_w and scale_h are valid
 * if factor is pixels, src_w, src_h, dst_w, dst_h are valid
 */
struct fimg2d_scale {
    enum scaling mode;
    enum scaling_factor factor;

    /* percentage */
    int scale_w;
    int scale_h;

    /* pixels */
    int src_w, src_h;
    int dst_w, dst_h;
};

/**
 * coordinate from start address(0,0) of image
 */
struct fimg2d_clip {
    bool enable;
    int x1;
    int y1;
    int x2;    /* x1 + width */
    int y2; /* y1 + height */
};

struct fimg2d_repeat {
    enum repeat mode;
    unsigned long pad_color;
};

/**
 * @bg_color: bg_color is valid only if bluescreen mode is BLUSCR.
 */
struct fimg2d_bluscr {
    enum bluescreen mode;
    unsigned long bs_color;
    unsigned long bg_color;
};

/**
 * @plane2: address info for CbCr in YCbCr 2plane mode
 */
struct fimg2d_image {
    struct fimg2d_addr addr;
    struct fimg2d_addr plane2;
    int width;
    int height;
    int stride;
    enum pixel_order order;
    enum color_format fmt;
};

struct fimg2d_param {
    enum blit_op op;
    unsigned long fillcolor;
    unsigned char g_alpha;
    enum premultiplied premult;
    bool dither;
    enum rotation rotate;
    struct fimg2d_scale *scaling;
    struct fimg2d_repeat *repeat;
    struct fimg2d_bluscr *bluscr;
    struct fimg2d_clip *clipping;
};

/**
 * @g_alpha: 0xff is opaque, 0x0 is transparnet
 * @seq_no: used for debugging
 */
struct fimg2d_blit {
    enum blit_op op;

    enum premultiplied premult;
    unsigned char g_alpha;
    bool dither;
    enum rotation rotate;
    struct fimg2d_scale *scaling;
    struct fimg2d_repeat *repeat;
    struct fimg2d_bluscr *bluscr;
    struct fimg2d_clip *clipping;

    unsigned long solid_color;
    struct fimg2d_image *src;
    struct fimg2d_image *dst;
    struct fimg2d_image *msk;

    struct fimg2d_rect *src_rect;
    struct fimg2d_rect *dst_rect;
    struct fimg2d_rect *msk_rect;

    unsigned int seq_no;
};
#endif /* __SEC_G2D_4X_H__ */
