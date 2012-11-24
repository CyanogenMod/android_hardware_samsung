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
 * @file    SW_Scale_up_CbCr_NEON.S
 * @brief
 * @author  MinGu Jeon (mingu85.jeon@samsung.com)
 * @version 1.0
 * @history
 *   2012.05.09 : Create
 */

/*
 * Do scaling up the YUV422data
 * This program is only supported for  NV16type (YCbCr422 2planes foward order)
 * This Function is scaling CbCr data up
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
    .global SW_Scale_up_CbCr_NEON
    .type   SW_Scale_up_CbCr_NEON, %function
SW_Scale_up_CbCr_NEON:
    .fnstart

    @r0     src_width
    @r1     src_height / modified origin_ptr
    @r2     dst_width
    @r3     dst_height
    @r4     MainHorRatio
    @r5     MainVerRatio
    @r6     origin_ptr
    @r7     dst_ptr
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
    @d14[0] = {original_src_width = ( dst_width * ratio + 1<<14) >> 14 )
    @d14[1] = {original_dst_width = ( dst_height * ratio + 1<<14) >> 14 )
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

     lsr        r2, #1                  @r2 = dst_width / 2

     vmov.u32   d2[1], r4
     vmov.u32   d0[0], r10
     vmov.u32   d0[1], r11
     vmov.u32   d1[0], r12
     vmov.u32   d1[1], r14
     vmul.i32   q0, d2[1]               @q0 = {MainHorRatio*1,MainHorRatio*2,MainHorRatio*3,MainHorRatio*4}
     vdup.32    d30, r8
     vdup.32    d31, r9

     mul        r8, r2, r4
     mov        r10, #0x4000
     sub        r10, #1
     add        r8, r10
     lsr        r8, #14                 @r8 = ((dst_width * MainHorRatio) + 0x4000) >> 14
     sub        r8, #8                  @r8 = r8 - 8(src_width - 8)
     vmov.u32   d14[0], r8              @d14[0] = origin_src_width

     mul        r9, r3, r5
     add        r9, r10                 @r9 = ((dst_height * MainVerRatio) + 0x4000) >> 14
     lsr        r9, #14
     sub        r9, #1
     vmov.u32   d14[1], r9              @d14[1] = origin_src_height

start:
    mov         r12, #0x4000            @r12 = set old_SRC_BY_LONG
    mov         r14, #0x4000            @r14 = set new_SRC_BY_LONG

    mov         r10, #0                 @r10 = vertical = 0
    vmov.u16    d15, #0x100             @d15 = {0x100, 0x100, 0x100, 0x100}

HV0:
    vmov.i32    q6, #0x4000             @q6 = {SRC_RX_LONG1, SRC_RX_LONG2, SRC_RX_LONG3, SRC_RX_LONG4}
                                        @q6 = set default value of SRC_RX_LONG
    vadd.u32    q2, q6, q0
    vdup.32     q3, r4                  @q3 = {MainHorRatio, MainHorRatio, MainHorRatio, MainHorRatio}
    vsub.u32    q2, q2, q3              @q2 = q2 - q3
                                        @q2 = {0, MainHorRatio, MainHorRatio*2, MainHorRatio*3}

    mov         r8, r14, lsr #14
    mov         r9, r12, lsr #14
    sub         r1, r8, r9              @r1 = (new_SRC_BY_LONG>>14) - (old_SRC_BY_LONG>>14)

    mul         r8, r1, r0
    add         r6, r8                  @dst_ptr += width * (new_SRC_BY_LONG>>14 - old_SRC_BY_LONG>>14)

    mov         r8, r14, lsr #6         @r8 = SRC_RX
    lsl         r8, r8, #24
    lsr         r8, r8, #24             @set dB = SRC_RX & 0xFF
    rsb         r9, r8, #0x100          @set dT = 256 - dB

    vdup.16     d18, r9                 @q9 = {dT, dT, dB, dB}
    vdup.16     d19, r8

    vmov.u32    r11, d14[0]
    lsl         r11, #14
    add         r11, #0x4000            @r11 = origin_src_width >> 14
    mov         r1, #0

    mov         r8, r6                  @r8 = src_address

HW0:
    pld         [r8]
    vld2.8      {d6, d7}, [r8]          @d6 = {Cb1, Cb2,.. Cb8}, d7 = {Cr1, Cr2, .. Cr8}
    add         r8, r0
    pld         [r8]
    vld2.8      {d8, d9}, [r8]          @d8 = {NCb1, NCb2,.. NCb8}, d9 = { NCr1, NCr2, .. NCr8}
    sub         r8, r0

    vzip.8      d6, d8                  @q3 = {Cb1, NCb1, Cb2, NCb2, Cb3, NCb3, .. Cb8, NCb8}
    vzip.8      d7, d9
    vswp        d7, d8                  @q4 = {Cr1, NCr1, Cr2, NCr2, ..... Cr8, NCr8}

    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8            @q12 = position of new pixels

    vshr.u32    q11, q2, #6             @q9 = {new SRC_RX_LONG} >> 6 = SRC_RX
    vshl.u32    q11, #24
    vshr.u32    q11, #24
    vmovn.u32   d16, q11                @d16 = dR
    vsub.u16    d17, d15, d16           @d17 = 256 - dR = dL

    vdup.u32    q6, d5[1]               @q6{old SRC_RX_LONG} = q2{new SRC_RX_LONG}
                                        @update old SRC_RX_LONG
    vmov.u32    r9, d25[1]              @get position of last pixel

    vmull.u16   q11, d17, d18           @dL * dT = W0

    vshl.u32    q11, #15
    vshr.u32    q11, #23                @d26 = {W0_0, W0_1, W0_2, W0_3}
    vmovn.u32   d26, q11

    vmull.u16   q11, d16, d18           @dR * dT = W2

    vshl.u32    q11, #15
    vshr.u32    q11, #23                @d28 = {W1_0, W1_1, W1_2, W1_3}
    vmovn.u32   d28, q11

    vmull.u16   q11, d17, d19           @dL * dB = W1

    vshl.u32    q11, #15
    vshr.u32    q11, #23
    vmovn.u32   d27, q11                @d27 = {W2_0, W2_1, W2_2, W2_3}

    vadd.u16    d22, d26, d27           @W3 = 256 - (W0 + W1 + W2)
    vadd.u16    d22, d22, d28
    vsub.u16    d29, d15, d22           @d29 = {W3_0, W3_1, W3_2, W3_3}

    vmov.u32    d10, d30
    vmov.u32    d11, d30
    vmul.u32    q11, q5, q12            @q11 = insert index of data which is used for interpolation

    vadd.u32    d22, d31
    vadd.u32    d23, d31

    vtrn.16     d26, d27                @d26 = {W0_0, W2_0, W1_0, W3_0}
    vtrn.16     d28, d29                @d27 = {W0_1, W2_1, W1_1, W3_1}
    vtrn.32     q13, q14                @d28 = {W0_2, W2_2, W1_2, W3_2}
                                        @d29 = {W0_3, W2_3, W2_3, W3_3}

    vtbl.8      d20, {d6, d7}, d22      @d20(Cb) = {LT, RT, LB, RB, LT, RT, LB, RB}

    vmovl.u8    q5, d20
    vmul.u16    q10, q5, q13            @q10(Cb) = {LT*W0_0, RT*W2_0,..., RT*W1_1, RB*W3_1}
    vpadd.u16   d10, d20, d21           @d2(Cb) = {LT*W0+LB*W2+RT*W1+RB*W3}
    vpaddl.u16  d2, d10

    vtbl.8      d20, {d8, d9}, d22      @d20(Cr) = {LT, RT, LB, RB, LT, RT, LB, RB}

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q13            @q10(Cr) = {LT*W0_0, LB*W2_0,..., RT*W1_1, RB*W3_1}
    vpadd.u16   d10, d20, d21           @d3(Cr) = {LT*W0+LB*W2+RT*W1+RB*W3}
    vpaddl.u16  d3, d10

    vrshr.u32   q1, #8                  @q1 = {new pixel of Cb 1,2, new pixel of Cr 1,2}

    vtbl.8      d20, {d6, d7}, d23      @d20(Cb) = {LT*W0_2, LB*W2_2,..., RT*W1_3, RB*W3_3}

    vmovl.u8    q5, d20
    vmul.u16    q10, q5, q14            @q10(Cb) = {LT*W0+LB*W2+RT*W1+RB*W3}
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d6, d10

    vtbl.8     d20, {d8, d9}, d23       @d20(Cr) = {LT*W0_2, LB*W2_2,..., RT*W1_3, RB*W3_3}

    vmovl.u8    q5, d20
    vmul.u16    q10, q5, q14
    vpadd.u16   d10, d20, d21           @q10(Cr) = {LT*W0+LB*W2+RT*W1+RB*W3}
    vpaddl.u16  d7, d10

    vrshr.u32   q3, #8                  @q3 = {new pixel of Cb 3,4, new pixel of Cr 3,4}

    vzip.32     d2, d3
    vzip.32     d6, d7
    vmovn.u32   d10, q1
    vmovn.u32   d11, q3

    vuzp.8      d10, d11                @save to dst_ptr
    vst1.8      {d10}, [r7]

    add         r7, #8
    vadd.u32    q2, q6, q0              @update SRC_RX_LONG

    add         r8, r9, lsl #1          @update origin_ptr

    add         r1, #4
    sub         r11, r4, lsl #2
    cmp         r11, r4
    bgt         HW0

LAST_START0:
    mov         r8, r6
    vmov.u32    r9, d14[0]
    add         r8, r9, lsl #1

    lsl         r9, #14
    add         r9, #0x4000

    vdup.32     q6, r9                  @update old SRC_RX_LONG to ((origin_width-8)<<14+1<<14)

    pld         [r8]
    vld2.8      {d6, d7}, [r8]          @d6 = {Cb1, Cb2,.. Cb8}, d7 = {Cr1, Cr2, .. Cr8}
    add         r8, r0
    pld         [r8]
    vld2.8      {d9, d10}, [r8]          @d8 = {NCb1, NCb2,.. NCb8}, d9 = { NCr1, NCr2, .. NCr8}
    sub         r8, r0

    vzip.8      d6, d9                  @q3 = {Cb1, NCb1, Cb2, NCb2, Cb3, NCb3, .. Cb8, NCb8}
    vzip.8      d7, d10
    vswp        d7, d9                  @q4 = {Cr1, NCr1, Cr2, NCr2, ..... Cr8, NCr8}

    vmov.u64    d8, d7
    vmov.u64    d11, d10

    vshr.u64    d8, #48
    vshr.u64    d11, #48

LAST_HW0:
    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8            @q12 = position of new pixels

    vshr.u32    q11, q2, #6
    vshl.u32    q11, #24
    vshr.u32    q11, #24

    vmovn.u32   d16, q11
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

    vmov.u32    d2, d30
    vmov.u32    d3, d30
    vmul.u32    q11, q1, q12

    vadd.u32    d22, d31
    vadd.u32    d23, d31

    vtrn.16     d26, d27                @d26 = {W0_0, W2_0, W1_0, W3_0}
    vtrn.16     d28, d29                @d27 = {W0_1, W2_1, W1_1, W3_1}
    vtrn.32     q13, q14                @d28 = {W0_2, W2_2, W1_2, W3_2}
                                        @d29 = {W0_3, W2_3, W2_3, W3_3}

    vtbl.8      d20, {d6, d7, d8}, d22

    vmovl.u8    q1, d20

    vmul.u16    q10, q1, q13
    vpadd.u16   d16, d20, d21
    vpaddl.u16  d2, d16

    vtbl.8      d20, {d9, d10, d11}, d22

    vmovl.u8    q8, d20

    vmul.u16    q10, q8, q13
    vpadd.u16   d17, d20, d21
    vpaddl.u16  d3, d17

    vrshr.u32   q1, #8

    vtbl.8      d20, {d6, d7, d8}, d23

    vmovl.u8    q8, d20

    vmul.u16    q10, q8, q14
    vpadd.u16   d16, d20, d21
    vpaddl.u16  d26, d16

    vtbl.8      d20, {d9, d10, d11}, d23

    vmovl.u8    q8, d20

    vmul.u16    q10, q8, q14
    vpadd.u16   d17, d20, d21
    vpaddl.u16  d27, d17

    vrshr.u32   q13, #8

    vzip.32     d2, d3
    vzip.32     d26, d27
    vmovn.u32   d22, q1
    vmovn.u32   d23, q13

    vuzp.8      d22, d23
    vst1.8      {d22}, [r7]

    add         r7, #8
    vdup.u32    q1, d0[0]
    vshl.u32    q1, #2
    vadd.u32    q2, q1

    add         r1, #4
    cmp         r1, r2
    bcc         LAST_HW0

LAST_END_HW0:
    vmov.u32    r9, d14[1]
    mov         r12, r14
    add         r14, r5
    add         r10, #1

    mov         r8, r14, lsr #14
    cmp         r8, r9
    bls         HV0

LAST_HV1:
@    cmp         r10, r3
@    beq         endnow

    vmov.i32    q6, #0x4000
    vadd.u32    q2, q6, q0              @q2 = q6 + q0
    vdup.32     q3, r4                  @q3 = {MainHorRatio, MainHorRatio, MainHorRatio, MainHorRatio}
    vsub.u32    q2, q2, q3              @q2 = q2 - q3

    mov         r8, r14, lsr #14
    mov         r9, r12, lsr #14
    sub         r1, r8, r9              @r1 = new_SRC_BY_LONG - old_SRC_BY_LONG

    mul         r8, r1, r0
    add         r6, r8                  @dst_ptr += width * ((new_SRC_BY_LONG-old_SRC_BY_LONG)>>14)

    mov         r8, r14, lsr #6
    lsl         r8, r8, #24
    lsr         r8, r8, #24             @dB
    rsb         r9, r8, #0x100          @dT

    vdup.16     d18, r9
    vdup.16     d19, r8

    vmov.u32    r11, d14[0]
    lsl         r11, #14
    add         r11, #0x4000
    mov         r1, #0

    mov         r8, r6

HW1:
    pld         [r8]
    vld2.8      {d6, d7}, [r8]          @d6 = {Cb1, Cb2,.. Cb8}, d7 = {Cr1, Cr2, .. Cr8}
    vmov.u64    q4, q3

    vzip.8      d6, d8                  @q3 = {Cb1, NCb1, Cb2, NCb2, Cb3, NCb3, .. Cb8, NCb8}
    vzip.8      d7, d9
    vswp        d7, d8                  @q4 = {Cr1, NCr1, Cr2, NCr2, ..... Cr8, NCr8}

    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8

    vshr.u32    q11, q2, #6              @q9 = {new SRC_RX_LONG} >> 6 = SRC_RX
    vshl.u32    q11, #24
    vshr.u32    q11, #24
    vmovn.u32   d16, q11                 @d22 = dR
    vsub.u16    d17, d15, d16           @d23 = 256 - dR = dL

    vdup.u32    q6, d5[1]               @q6{old SRC_RX_LONG} = q2{new SRC_RX_LONG}
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

    vtbl.8      d20, {d6, d7}, d22      @d28 = {LT, RT, LB, RB, LT, RT, LB, RB}

    vmovl.u8    q5, d20                 @q5(Cb)
    vmul.u16    q10, q5, q13            @q10(Cb) = {LT*W1_0, RT*W1_2, LB*W1_1, RB*W1_3,.....}
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d2, d10

    vtbl.8      d20, {d8, d9}, d22      @d10(Cr) = {LT, RT, LB, RB, LT, RT, LB, RB}

    vmovl.u8    q5, d20

    vmul.u16    q10, q5, q13
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d3, d10

    vrshr.u32   q1, #8                  @q1 = {new pixel of Cb 1,2, new pixel of Cr 1,2}

    vtbl.8      d20, {d6, d7}, d23

    vmovl.u8    q5, d20
    vmul.u16    q10, q5, q14
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d16, d10

    vtbl.8       d20, {d8, d9}, d23

    vmovl.u8    q5, d20
    vmul.u16    q10, q5, q14
    vpadd.u16   d10, d20, d21
    vpaddl.u16  d17, d10

    vrshr.u32   q8, #8

    vzip.32     d2, d3
    vzip.32     d16, d17
    vmovn.u32   d10, q1
    vmovn.u32   d11, q8

    vuzp.8      d10, d11
    vst1.8      {d10}, [r7]

    add         r7, #8
    vadd.u32    q2, q6, q0

    add         r8, r9, lsl #1
    add         r1, #4
    sub         r11, r4, lsl #2
    cmp         r11, r4
    bgt         HW1

LAST_START1:
    mov         r8, r6
    vmov.u32    r9, d14[0]
    add         r8, r9, lsl #1

    lsl         r9, #14
    add         r9, #0x4000

    vdup.32     q6, r9

    vld2.8     {d6, d7}, [r8]
    vmov.u64    d9, d6
    vmov.u64    d10, d7

    vzip.8      d6, d9
    vzip.8      d7, d10
    vswp        d7, d9

    vmov.u64    d8, d7
    vmov.u64    d11, d10

    vshr.u64    d8, #48
    vshr.u64    d11, #48

LAST_HW1:
    vshr.u32    q11, q2, #14
    vshr.u32    q8, q6, #14
    vsub.u32    q12, q11, q8

    vshr.u32    q11, q2, #6
    vshl.u32    q11, #24
    vshr.u32    q11, #24

    vmovn.u32   d16, q11
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

    vmov.u32    d2, d30
    vmov.u32    d3, d30
    vmul.u32    q11, q1, q12

    vadd.u32    d22, d31
    vadd.u32    d23, d31

    vtrn.16     d26, d27                @d26 = {W0_0, W2_0, W1_0, W3_0}
    vtrn.16     d28, d29                @d27 = {W0_1, W2_1, W1_1, W3_1}
    vtrn.32     q13, q14                @d28 = {W0_2, W2_2, W1_2, W3_2}
                                        @d29 = {W0_3, W2_3, W2_3, W3_3}

    vtbl.8      d20, {d6, d7, d8}, d22

    vmovl.u8    q1, d20

    vmul.u16    q10, q1, q13
    vpadd.u16   d16, d20, d21
    vpaddl.u16  d2, d16

    vtbl.8      d20, {d9, d10, d11}, d22

    vmovl.u8    q8, d20

    vmul.u16    q10, q8, q13
    vpadd.u16   d16, d20, d21
    vpaddl.u16  d3, d16

    vrshr.u32   q1, #8

    vtbl.8      d20, {d6, d7, d8}, d23

    vmovl.u8    q8, d20

    vmul.u16    q10, q8, q14
    vpadd.u16   d16, d20, d21
    vpaddl.u16  d22, d16

    vtbl.8      d20, {d9, d10, d11}, d23

    vmovl.u8    q8, d20

    vmul.u16    q10, q8, q14
    vpadd.u16   d16, d20, d21
    vpaddl.u16  d23, d16

    vrshr.u32   q11, #8

    vzip.32     d2, d3
    vzip.32     d22, d23
    vmovn.u32   d16, q1
    vmovn.u32   d17, q11

    vuzp.8      d16, d17
    vst1.8      {d16}, [r7]

    add         r7, #8
    vdup.u32    q1, d0[0]
    vshl.u32    q1, #2
    vadd.u32    q2, q1

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
