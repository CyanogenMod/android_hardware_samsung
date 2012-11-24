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
 * @file    SW_Memcpy_NEON.S
 * @brief
 * @author  MinGu Jeon (mingu85.jeon@samsung.com)
 * @version 1.0
 * @history
 *   2012.05.09 : Create
 */
    .arch armv7-a
    .text
    .global SW_Memcpy_NEON
    .type   SW_Memcpy_NEON, %function
SW_Memcpy_NEON:
    .fnstart

    stmfd       sp!, {r4-r12,r14}
    mul         r8, r0, r1
    add         r14, r13, #40
    ldmia       r14!, {r4-r5}
LOOPJ:
    pld         [r2]
    vld1.8      {d0}, [r2]
    add         r2, r2, #8

    pld         [r3]
    vld1.8      {d1}, [r3]
    add         r3, r3, #8

    pld         [r4]
    vst1.8      {d0}, [r4]
    add         r4, #8

    pld         [r5]
    vst1.8      {d1}, [r5]
    add         r5, #8

    sub         r8, #8
    cmp         r8, #0
    bgt         LOOPJ

    ldmfd       sp!, {r4-r12, r14}
    mov         pc, lr
.fnend


