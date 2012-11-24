/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
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

/*
 * @file    SW_Scale_up_Y_NEON.S
 * @brief
 * @author  MinGu Jeon (mingu85.jeon@samsung.com)
 * @version 1.0
 * @history
 *   2012.05.09 : Create
 */

/*
 * Do scaling up the YUV422data
 * This program is only supported for  NV16type (YCbCr422 2planes foward order)
 * This Function is scaling Y data up
 * @param src_width
 *  Width of source image
 *
 * @param src_height
 *  Height of source image
 *
 * @param dst_width
 *  Width of result image
 *
 * @param dst_height
 *  Height of result image
 *
 * @param MainHorRatio
 *  The ratio of scaling in horizontal. (src_width << 14)/dst_width
 *
 * @param MainVerRatio
 *  The ratio of scaling in vertical. (src_height << 14)/dst_height
 *
 * @param origin_ptr
 *  Address of CbCr filed in source image
 *
 * @param dst_ptr
 *  Address of CbCr filed in result image
 */
    .arch armv7-a
    .text
    .global SW_Scale_up_Y_NEON
    .type   SW_Scale_up_Y_NEON, %function
SW_Scale_up_Y_NEON:

    .fnstart

    @r0     src_width
    @r1     src_height / modified origin_ptr
    @r2     dst_width
    @r3     dst_height
    @r4     MainHorRatio / SRC_LX_LONG
    @r5     MainVerRatio / SRC_RX_LONG
    @r6     origin_ptr / SRC_LX
    @r7     dst_ptr / SRC_RX
    @r8     temp
    @r9     temp
    @r10    temp2 / vertical
    @r11    temp3 / horizontal
    @r12    temp4 / old_SRC_BY_LONG
    @r14    temp5 / new_SRC_BY_LONG

    @q0     = {MainHorRatio, MainHorRatio*2, MainHorRatio*3, MainHorRatio*4}
    @q1     = {new SRC_BY_LONG}
    @q2     = {new SRC_RX_LONG}
    @q5     = {W1_0, W1_2, W1_1, W1_3, W2_0, W2_2, W2_1, W2_3}
    @q6     = {old SRC_RX_LONG}
    @q3     = {Y1....Y16}
    @q4     = {NY1.....NY16}
    @d20    = {dT1, dT1, dB1, dB1}
    @q11    = {dR1, dL1, dR2, dL2, dR3, dL3, dR4, dL4}
    @d14[0] = {original_src_width = ( dst_width * ratio + 1<<14 ) >> 14}
    @d14[1] = {original_dst_width = ( dst_height * ratio + 1<<14 ) >> 14}
    @d30    = {0x02020202}
    @d31    = {0x03020100}


     stmfd       sp!, {r4-r12,r14}
     mov         r14, r13
     add         r14, #40
     ldmia       r14!, {r4-r7}

     ldr        r8, =0x02020202
     ldr        r9, =0x03020100
     mov        r10, #0x1
     mov        r11, #0x2
     mov        r12, #0x3
     mov        r14, #0x4

     vmov.u32   d2[1], r4
     vmov.u32   d0[0], r10
     vmov.u32   d0[1], r11
     vmov.u32   d1[0], r12
     vmov.u32   d1[1], r14
     vmul.i32   q0, d2[1]               @q0     = {MainHorRatio,MainHorRatio*2,MainHorRatio*3,MainHorRatio*4}
     vdup.32    d30, r8
     vdup.32    d31, r9

     mul        r8, r2, r4
     lsl        r10, #14
     sub        r10, #1
     add        r8, r10
     lsr        r8, #14                 @r8 = ((dst_width * MainHorRatio) + 0x4000) >> 14
     sub        r8, #8                  @r8 = r8 - 8 (origin_src_width - 8)
     vmov.u32   d14[0], r8              @d14[0] = origin_src_width

     mul        r9, r3, r5
     add        r9, r10
     lsr        r9, #14
     sub        r9, #1
     vmov.u32   d14[1], r9              @d14[1] = origin_src_height


start:
    mov         r12, #0x4000
    mov         r14, #0x4000

    mov         r10, #0                 @r10 = vertical = 0
    vmov.u16    d15, #0x100

HV0:
    vmov.i32    q6, #0x4000
    vadd.u32    q2, q6, q0              @q2 = q6 + q0
    vdup.32     q3, r4                  @q3 = {MainHorRatio, MainHorRatio, MainHorRatio, MainHorRatio}
    vsub.u32    q2, q2, q3              @q2 = q2 - q3

    mov         r8, r14, lsr #14
    mov         r9, r12, lsr #14
    sub         r1, r8, r9              @r1 = new_SRC_BY_LONG - old_SRC_BY_LONG

    mul         r8, r1, r0
    add         r6, r8                  @origin_ptr += width * ((new_SRC_BY_LONG-old_SRC_BY_LONG)>>14)

    mov         r8, r14, lsr #6
    lsl         r8, r8, #24
    lsr         r8, r8, #24             @dB
    rsb         r9, r8, #0x100          @dT

    vdup.16     d18, r9
    vdup.16     d19, r8

    vmov.u32    r11, d14[0]             @r11 = horizontal = 0
    lsl         r11, #14
    add         r11, #0x4000
    mov         r1, #0

    mov         r8, r6

HW0:
    pld         [r8]
    vld1.8      {d6}, [r8]              @d6 = {Y1, Y2, Y3, Y4, Y5, .. Y8}
    add         r8, r0
    pld         [r8]
    vld1.8      {d7}, [r8]              @d7 = {NY1, NY2, NY3, NY4, .. NY8}
    sub         r8, r0

    vzip.8      d6, d7                  @q3 = {Y1, NY1, Y2, NY2, Y3, NY3, .. Y8, NY8}

    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8            @q12 = {new SRC_RX_LONG}>>14 - {old SRC_RX_LONG}>>14
                                        @q12 = Position of pixels
    vshr.u32    q11, q2, #6             @q9 = {new SRC_RX_LONG} >> 6 = SRC_RX
    vshl.u32    q11, #24
    vshr.u32    q11, #24
    vmovn.u32   d16, q11                @d16 = dR
    vsub.u16    d17, d15, d16           @d17 = 256 - dR = dL

    vdup.u32    q6, d5[1]
    vmov.u32    r9, d25[1]

    vmull.u16   q11, d17, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d26 = {W0_0, W0_1, W0_2, W0_3}
    vmovn.u32   d26, q11

    vmull.u16   q11, d16, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d28 = {W1_0, W1_1, W1_2, W1_3}
    vmovn.u32   d28, q11

    vmull.u16   q11, d17, d19

    vshl.u32    q11, #15
    vshr.u32    q11, #23
    vmovn.u32   d27, q11           @d27 = {W2_0, W2_1, W2_2, W2_3}

    vadd.u16    d22, d26, d27
    vadd.u16    d22, d22, d28
    vsub.u16    d29, d15, d22           @d29 = {W3_0, W3_1, W3_2, W3_3}

    vmov.u32    d10, d30
    vmov.u32    d11, d30
    vmul.u32    q11, q5, q12            @q11 = Position of pixels

    vadd.u32    d22, d31
    vadd.u32    d23, d31

    vtrn.16     d26, d27                @d26 = {W0_0, W2_0, W1_0, W3_0}
    vtrn.16     d28, d29                @d27 = {W0_1, W2_1, W1_1, W3_1}
    vtrn.32     q13, q14                @d28 = {W0_2, W2_2, W1_2, W3_2}
                                        @d29 = {W0_3, W2_3, W2_3, W3_3}

    vtbl.8      d20, {d6, d7}, d22      @d28 = {LT, LB, RT, RB, LT, LB, RT, RB}

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q13
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d2, d10
    vrshr.u32   d2, #8                  @d2 = {new pixel 1,2}

    vtbl.8      d20, {d6, d7}, d23

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q14              @q9 = {W3_0*LT,W3_1*RT,W3_2*LB,W3_3*RB}
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d3, d10
    vrshr.u32   d3, #8                  @d3 = {new pixel 3,4}

    vmovn.u32   d21, q1
    vmov.u16    d22, #0
    vuzp.8      d21, d22
    vst1.u64    d21, [r7]
    add         r7, #4

    vadd.u32    q2, q6, q0

    add         r8, r9
    add         r1, #4
    sub         r11, r4, lsl #2

    cmp         r11, r4
    bgt         HW0

LAST_START0:
    mov         r8, r6
    vmov.u32    r9, d14[0]
    add         r8, r9

    lsl         r9, #14
    add         r9, #0x4000

    vdup.32     q6, r9

    vld1.8      {d6}, [r8]
    add         r8, r0
    vld1.8      {d7}, [r8]
    sub         r8, r0

    vzip.8      d6, d7

    vmov.u64    d8, d7
    vshr.u64    d8, #48

LAST_HW0:
    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8            @q12 = {new SRC_RX_LONG}>>14 - {old SRC_RX_LONG}>>14
                                        @q12 = Position of pixels
    vshr.u32    q11, q2, #6             @q9 = {new SRC_RX_LONG} >> 6 = SRC_RX
    vshl.u32    q11, #24
    vshr.u32    q11, #24
    vmovn.u32   d16, q11                @d22 = dR
    vsub.u16    d17, d15, d16           @d23 = 256 - dR = dL

    vmull.u16   q11, d17, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d26 = {W0_0, W0_1, W0_2, W0_3}
    vmovn.u32   d26, q11

    vmull.u16   q11, d16, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d28 = {W1_0, W1_1, W1_2, W1_3}
    vmovn.u32   d28, q11

    vmull.u16   q11, d17, d19

    vshl.u32    q11, #15
    vshr.u32    q11, #23
    vmovn.u32   d27, q11           @d27 = {W2_0, W2_1, W2_2, W2_3}

    vadd.u16    d22, d26, d27
    vadd.u16    d22, d22, d28
    vsub.u16    d29, d15, d22      @d29 = {W3_0, W3_1, W3_2, W3_3}

    vmov.u32    d10, d30
    vmov.u32    d11, d30
    vmul.u32    q11, q5, q12

    vadd.u32    d22, d31
    vadd.u32    d23, d31

    vtrn.16     d26, d27                @d26 = {W0_0, W2_0, W1_0, W3_0}
    vtrn.16     d28, d29                @d27 = {W0_1, W2_1, W1_1, W3_1}
    vtrn.32     q13, q14                @d28 = {W0_2, W2_2, W1_2, W3_2}
                                        @d29 = {W0_3, W2_3, W2_3, W3_3}

    vtbl.8      d20, {d6, d7, d8}, d22

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q13
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d2, d10
    vrshr.u32   d2, #8                  @d2 = {new pixel 1,2}

    vtbl.8      d20, {d6, d7, d8}, d23

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q14             @q14 = {W3_0*LT,W3_1*RT,W3_2*LB,W3_3*RB}
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d3, d10
    vrshr.u32   d3, #8                  @d3 = {new pixel 3,4}

    vmovn.u32   d21, q1
    vmov.u16    d22, #0
    vuzp.8      d21, d22
    vmov.u32    r9, d21[0]

    str         r9, [r7]
    add         r7, #4
    vdup.u32    q10, d0[0]
    vshl.u32    q10, #2
    vadd.u32    q2, q10

    add         r1, #4
    cmp         r1, r2
    bcc         LAST_HW0

LAST_END_HW0:
    vmov.u32    r9, d14[1]
    mov         r12, r14                @old_SRC_BY_LONG = new_SRC_BY_LONG
    add         r14, r5                 @new_SRC_BY_LONG += MainVerRatio
    add         r10, #1                 @vertical++

    mov         r8, r14, lsr #14
    cmp         r8, r9
    bls         HV0




LAST_HV1:
    vmov.i32    q6, #0x4000             @q6 = {SRC_RX_LONG1, SRC_RX_LONG2, SRC_RX_LONG3, SRC_RX_LONG4}
    vadd.u32    q2, q6, q0              @q2 = q6 + q0
    vdup.32     q3, r4                  @q3 = {MainHorRatio, MainHorRatio, MainHorRatio, MainHorRatio}
    vsub.u32    q2, q2, q3              @q2 = q2 - q3

    mov         r8, r14, lsr #14
    mov         r9, r12, lsr #14
    sub         r1, r8, r9              @r1 = new_SRC_BY_LONG - old_SRC_BY_LONG

    mul         r8, r1, r0
    add         r6, r8                  @origin_ptr += width * ((new_SRC_BY_LONG-old_SRC_BY_LONG)>>14)

    mov         r8, r14, lsr #6
    lsl         r8, r8, #24
    lsr         r8, r8, #24             @dB
    rsb         r9, r8, #0x100          @dT

    vdup.16     d18, r9
    vdup.16     d19, r8

    vmov.u32    r11, d14[0]             @r11 = horizontal = 0
    lsl         r11, #14
    add         r11, #0x4000
    mov         r1, #0

    mov         r8, r6

HW1:
    pld         [r8]
    vld1.8      {d6}, [r8]              @d6 = {Y1, Y2, Y3, Y4, Y5, .. Y8}
    vmov.u64    d7, d6

    vzip.8      d6, d7                  @q3 = {Y1, NY1, Y2, NY2, Y3, NY3, .. Y8, NY8}

    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8            @q12 = {new SRC_RX_LONG}>>14 - {old SRC_RX_LONG}>>14
                                        @q12 = Position of pixels
    vshr.u32    q11, q2, #6              @q9 = {new SRC_RX_LONG} >> 6 = SRC_RX
    vshl.u32    q11, #24
    vshr.u32    q11, #24
    vmovn.u32   d16, q11                 @d22 = dR
    vsub.u16    d17, d15, d16           @d23 = 256 - dR = dL

    vdup.u32    q6, d5[1]
    vmov.u32    r9, d25[1]

    vmull.u16   q11, d17, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d26 = {W0_0, W0_1, W0_2, W0_3}
    vmovn.u32   d26, q11

    vmull.u16   q11, d16, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d28 = {W1_0, W1_1, W1_2, W1_3}
    vmovn.u32   d28, q11

    vmull.u16   q11, d17, d19

    vshl.u32    q11, #15
    vshr.u32    q11, #23
    vmovn.u32   d27, q11           @d27 = {W2_0, W2_1, W2_2, W2_3}

    vadd.u16    d22, d26, d27
    vadd.u16    d22, d22, d28
    vsub.u16    d29, d15, d22           @d29 = {W3_0, W3_1, W3_2, W3_3}

    vmov.u32    d10, d30
    vmov.u32    d11, d30
    vmul.u32    q11, q5, q12            @q11 = Position of pixels

    vadd.u32    d22, d31
    vadd.u32    d23, d31

    vtrn.16     d26, d27                @d26 = {W0_0, W2_0, W1_0, W3_0}
    vtrn.16     d28, d29                @d27 = {W0_1, W2_1, W1_1, W3_1}
    vtrn.32     q13, q14                @d28 = {W0_2, W2_2, W1_2, W3_2}
                                        @d29 = {W0_3, W2_3, W2_3, W3_3}
    vtbl.8      d20, {d6, d7}, d22

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q13
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d2, d10
    vrshr.u32   d2, #8                  @d2 = {new pixel 1,2}

    vtbl.8      d20, {d6, d7}, d23

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q14              @q9 = {W3_0*LT,W3_1*RT,W3_2*LB,W3_3*RB}
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d3, d10
    vrshr.u32   d3, #8                  @d3 = {new pixel 3,4}

    vmovn.u32   d21, q1
    vmov.u16    d22, #0
    vuzp.8      d21, d22
    vst1.u64    d21, [r7]
    add         r7, #4

    vadd.u32    q2, q6, q0

    add         r8, r9
    add         r1, #4
    sub         r11, r4, lsl #2
    cmp         r11, r4
    bgt         HW1

LAST_START1:
    mov         r8, r6
    vmov.u32    r9, d14[0]
    add         r8, r9

    lsl         r9, #14
    add         r9, #0x4000
    vdup.32     q6, r9

    vld1.8      {d6}, [r8]
    vmov.u64    d7, d6

    vzip.8      d6, d7

    vmov.u64    d8, d7
    vshr.u64    d8, #48

LAST_HW1:
    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8            @q12 = {new SRC_RX_LONG}>>14 - {old SRC_RX_LONG}>>14
                                        @q12 = Position of pixels
    vshr.u32    q11, q2, #6              @q9 = {new SRC_RX_LONG} >> 6 = SRC_RX
    vshl.u32    q11, #24
    vshr.u32    q11, #24
    vmovn.u32   d16, q11                 @d16 = dR
    vsub.u16    d17, d15, d16           @d17 = 256 - dR = dL

    vmull.u16   q11, d17, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d26 = {W0_0, W0_1, W0_2, W0_3}
    vmovn.u32   d26, q11

    vmull.u16   q11, d16, d18

    vshl.u32    q11, #15
    vshr.u32    q11, #23            @d28 = {W1_0, W1_1, W1_2, W1_3}
    vmovn.u32   d28, q11

    vmull.u16   q11, d17, d19

    vshl.u32    q11, #15
    vshr.u32    q11, #23
    vmovn.u32   d27, q11           @d27 = {W2_0, W2_1, W2_2, W2_3}

    vadd.u16    d22, d26, d27
    vadd.u16    d22, d22, d28
    vsub.u16    d29, d15, d22           @d29 = {W3_0, W3_1, W3_2, W3_3}

    vmov.u32    d10, d30
    vmov.u32    d11, d30
    vmul.u32    q11, q5, q12

    vadd.u32    d22, d31
    vadd.u32    d23, d31

    vtrn.16     d26, d27                @d26 = {W0_0, W2_0, W1_0, W3_0}
    vtrn.16     d28, d29                @d27 = {W0_1, W2_1, W1_1, W3_1}
    vtrn.32     q13, q14                @d28 = {W0_2, W2_2, W1_2, W3_2}
                                        @d29 = {W0_3, W2_3, W2_3, W3_3}

    vtbl.8      d20, {d6, d7, d8}, d22      @d28 = {LT, LB, RT, RB, LT, LB, RT, RB}

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q13
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d2, d10
    vrshr.u32   d2, #8                  @d2 = {new pixel 1,2}

    vtbl.8      d20, {d6, d7, d8}, d23

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q14             @q10 = {W3_0*LT,W3_1*RT,W3_2*LB,W3_3*RB}
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d3, d10
    vrshr.u32   d3, #8                  @d3 = {new pixel 3,4}

    vmovn.u32   d21, q1
    vmov.u16    d22, #0
    vuzp.8      d21, d22
    vmov.u32    r9, d21[0]

    str         r9, [r7]
    add         r7, #4
    vdup.u32    q10, d0[0]
    vshl.u32    q10, #2
    vadd.u32    q2, q10

    add         r1, #4
    cmp         r1, r2
    bcc         LAST_HW1

    mov         r12, r14
    add         r14, r5
    add         r10, #1

    cmp         r10, r3
    bcc         LAST_HV1

endnow:
    mov         r0, r8
    ldmfd       sp!, {r4-r12, r14}
    mov         pc, lr

.fnend
