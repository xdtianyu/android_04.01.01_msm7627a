/*
 * Copyright (c) 2005-2008, The Android Open Source Project
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

    .text

    .global S32A_Blend_BlitRow32_arm_neon
    .func S32A_Blend_BlitRow32_arm_neon

S32A_Blend_BlitRow32_arm_neon:

    // calculate src_scale = aa + 1
    add      r3, r3, #1

#if __ARM_ARCH__ == 7 || defined(__ARM_NEON__)

    cmp     r2,#24
    blt      .Lslow_path

    push {r4, r5}
    vpush    {q4-q7}
    vmov.u16 q14,#0x100

    // store src_scale in q4
    vdup.u16  q4, r3

    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)
                                    //update source ptr but not dst ptr
    vld4.8  {d4, d5, d6, d7}, [r0]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)
    add      r4, r0, #32 // minus 16 to pretend the last round
    mov      r5, #64
    sub      r2,r2,#8

.Lloop:
    pld     [r1, #256]
    pld     [r0, #256]
    subs     r2, r2, #16
    cmp      r2,#16

    // expand destination from 8-bit to 16-bit
    vmovl.u8 q6, d4
    vmovl.u8 q7, d5
    vmovl.u8 q8, d6
    vmovl.u8 q9, d7

    // expand source from 8-bit to 16-bit
    vmovl.u8 q13, d3
    vmovl.u8 q10, d0
    vmovl.u8 q11, d1
    vmovl.u8 q12, d2

                                    //update source ptr but not dst ptr
    // calculate destination scale
    vmul.u16 q5, q13, q4
    vshr.u16 q5, q5, #8
    vsub.u16 q5, q14, q5

    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)
    // multiply destination ARGB components with dst_scale
    vmul.u16 q6, q6, q5
    vmul.u16 q7, q7, q5
    vmul.u16 q8, q8, q5
    vmul.u16 q9, q9, q5


    vld4.8  {d4, d5, d6, d7}, [r4]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)

    // multiply source ARGB components with src_scale
    vmul.u16 q10, q10, q4
    vmul.u16 q11, q11, q4
    vmul.u16 q12, q12, q4
    vmul.u16 q13, q13, q4


    // add processed src and dest pixels and extract high bytes
    vqadd.u8  q10, q6, q10
    vqadd.u8  q11, q7, q11
    vqadd.u8  q12, q8, q12
    vqadd.u8  q13, q9, q13

    vshrn.u16 d20, q10, #8
    vshrn.u16 d21, q11, #8
    vshrn.u16 d22, q12, #8
    vshrn.u16 d23, q13, #8

    vst4.8  {d20, d21, d22, d23}, [r0], r5 //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7

    // expand destination from 8-bit to 16-bit
    vmovl.u8 q6, d4
    vmovl.u8 q7, d5
    vmovl.u8 q8, d6
    vmovl.u8 q9, d7

    // expand source from 8-bit to 16-bit
    vmovl.u8 q13, d3
    vmovl.u8 q10, d0
    vmovl.u8 q11, d1
    vmovl.u8 q12, d2

    // calculate destination scale
    vmul.u16 q5, q13, q4
    vshr.u16 q5, q5, #8
    vsub.u16 q5, q14, q5

    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)

    // multiply destination ARGB components with dst_scale
    vmul.u16 q6, q6, q5
    vmul.u16 q7, q7, q5
    vmul.u16 q8, q8, q5
    vmul.u16 q9, q9, q5

    vld4.8  {d4, d5, d6, d7}, [r0]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)

    // multiply source ARGB components with src_scale
    vmul.u16 q10, q10, q4
    vmul.u16 q11, q11, q4
    vmul.u16 q12, q12, q4
    vmul.u16 q13, q13, q4


    // add processed src and dest pixels and extract high bytes
    vqadd.u8  q10, q6, q10
    vqadd.u8  q11, q7, q11
    vqadd.u8  q12, q8, q12
    vqadd.u8  q13, q9, q13

    vshrn.u16 d20, q10, #8
    vshrn.u16 d21, q11, #8
    vshrn.u16 d22, q12, #8
    vshrn.u16 d23, q13, #8

    vst4.8  {d20, d21, d22, d23}, [r4], r5 //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7

    bge     .Lloop

//There are 8 words left unprocessed from previous round
    // expand destination from 8-bit to 16-bit
    vmovl.u8 q6, d4
    vmovl.u8 q7, d5
    vmovl.u8 q8, d6
    vmovl.u8 q9, d7

    // expand source from 8-bit to 16-bit
    vmovl.u8 q13, d3
    vmovl.u8 q10, d0
    vmovl.u8 q11, d1
    vmovl.u8 q12, d2

    // calculate destination scale
    vmul.u16 q5, q13, q4
    vshr.u16 q5, q5, #8
    vsub.u16 q5, q14, q5

    // multiply destination ARGB components with dst_scale
    vmul.u16 q6, q6, q5
    vmul.u16 q7, q7, q5
    vmul.u16 q8, q8, q5
    vmul.u16 q9, q9, q5

    // multiply source ARGB components with src_scale
    vmul.u16 q10, q10, q4
    vmul.u16 q11, q11, q4
    vmul.u16 q12, q12, q4
    vmul.u16 q13, q13, q4

    // add processed src and dest pixels and extract high bytes
    vqadd.u8  q10, q6, q10
    vqadd.u8  q11, q7, q11
    vqadd.u8  q12, q8, q12
    vqadd.u8  q13, q9, q13

    vshrn.u16 d20, q10, #8
    vshrn.u16 d21, q11, #8
    vshrn.u16 d22, q12, #8
    vshrn.u16 d23, q13, #8

    vst4.8  {d20, d21, d22, d23}, [r0]! //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7

.Lless_than_16:
    cmp      r2,#8
    blt      .Lless_than_8

    sub      r2,r2,#8

    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)
                                    //update source ptr but not dst ptr
    vld4.8  {d4, d5, d6, d7}, [r0]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)

    // expand destination from 8-bit to 16-bit
    vmovl.u8 q6, d4
    vmovl.u8 q7, d5
    vmovl.u8 q8, d6
    vmovl.u8 q9, d7

    // expand source from 8-bit to 16-bit
    vmovl.u8 q13, d3
    vmovl.u8 q10, d0
    vmovl.u8 q11, d1
    vmovl.u8 q12, d2

    // calculate destination scale
    vmul.u16 q5, q13, q4
    vshr.u16 q5, q5, #8
    vsub.u16 q5, q14, q5

    // multiply destination ARGB components with dst_scale
    vmul.u16 q6, q6, q5
    vmul.u16 q7, q7, q5
    vmul.u16 q8, q8, q5
    vmul.u16 q9, q9, q5

    // multiply source ARGB components with src_scale
    vmul.u16 q10, q10, q4
    vmul.u16 q11, q11, q4
    vmul.u16 q12, q12, q4
    vmul.u16 q13, q13, q4

    // add processed src and dest pixels and extract high bytes
    vqadd.u8  q10, q6, q10
    vqadd.u8  q11, q7, q11
    vqadd.u8  q12, q8, q12
    vqadd.u8  q13, q9, q13

    vshrn.u16 d4, q10, #8
    vshrn.u16 d5, q11, #8
    vshrn.u16 d6, q12, #8
    vshrn.u16 d7, q13, #8

    vst4.8  {d4, d5, d6, d7}, [r0]! //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7

.Lless_than_8:
    vpop     {q4-q7}
    pop {r4, r5}

.Lslow_path:
    adds     r2, #0
    bxeq      lr
#endif

/*
 * r0 - dst
 * r1 - src
 * r2 - count
 * r3 - alpha
 */
    push     {r4-r11, lr}

    mov      r10, #0xFF
    orr      r10, r10, r10, lsl #16    //mask = r10 = 0x00FF00FF

    subs     r2, r2, #2
    blt      .Lblitrow32_single_loop

.Lblitrow32_double_loop:
    ldm      r0, {r4, r5}
    ldm      r1!, {r6, r7}

    /* First iteration */
    lsr      lr, r6, #24               //extract src_alpha

    // calculate dst_scale = 256 - ((src_alpha*src_scale)>>8)
    mul      lr, r3, lr
    lsr      lr, #8
    rsb      lr, lr, #256

    // src processing
    and      r8, r6, r10               //rb = (src & mask)
    and      r9, r10, r6, lsr #8       //ag = (src>>8) & mask

    mul      r11, r8, r3               //RB = rb * src_scale
    mul      r6, r9, r3                //AG = ag * src_scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r6, r6, r10, lsl #8       //r9 = AG & ~mask

    orr      r6, r6, r11

    // dst processing
    and      r8, r4, r10               //rb = (dst & mask)
    and      r9, r10, r4, lsr #8       //ag = (dst>>8) & mask

    mul      r11, r8, lr              //RB = rb * dst_scale
    mul      r4, r9, lr               //AG = ag * dst_scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r4, r4, r10, lsl #8       //r9 = AG & ~mask

    orr      r4, r4, r11

    /* Second iteration */
    lsr      lr, r7, #24               //extract src_alpha

    // calculate dst_scale = 256 - ((src_alpha*src_scale)>>8)
    mul      lr, r3, lr
    lsr      lr, #8
    rsb      lr, lr, #256

    // src processing
    and      r8, r7, r10               //rb = (src & mask)
    and      r9, r10, r7, lsr #8       //ag = (src>>8) & mask

    mul      r11, r8, r3               //RB = rb * src_scale
    mul      r7, r9, r3                //AG = ag * src_scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r7, r7, r10, lsl #8       //r9 = AG & ~mask

    orr      r7, r7, r11

    // dst processing
    and      r8, r5, r10               //rb = (dst & mask)
    and      r9, r10, r5, lsr #8       //ag = (dst>>8) & mask

    mul      r11, r8, lr              //RB = rb * dst_scale
    mul      r5, r9, lr               //AG = ag * dst_scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r5, r5, r10, lsl #8       //r9 = AG & ~mask

    orr      r5, r5, r11


    // add processed src and dst
    add      r6, r6, r4
    add      r7, r7, r5

    subs     r2, r2, #2
    stm      r0!, {r6, r7}

    bge      .Lblitrow32_double_loop

.Lblitrow32_single_loop:
    adds     r2, #1
    blo      .Lexit

    ldr      r4, [r0]
    ldr      r6, [r1], #4

    lsr      lr, r6, #24               //extract src_alpha

    // calculate dst_scale = 256 - ((src_alpha*src_scale)>>8)
    mul      lr, r3, lr
    lsr      lr, #8
    rsb      lr, lr, #256

    // src processing
    and      r8, r6, r10               //rb = (src & mask)
    and      r9, r10, r6, lsr #8       //ag = (src>>8) & mask

    mul      r11, r8, r3               //RB = rb * src_scale
    mul      r6, r9, r3                //AG = ag * src_scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r6, r6, r10, lsl #8       //r9 = AG & ~mask

    orr      r6, r6, r11

    // dst processing
    and      r8, r4, r10               //rb = (dst & mask)
    and      r9, r10, r4, lsr #8       //ag = (dst>>8) & mask

    mul      r11, r8, lr              //RB = rb * dst_scale
    mul      r4, r9, lr               //AG = ag * dst_scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r4, r4, r10, lsl #8       //r9 = AG & ~mask

    orr      r4, r4, r11

    add      r6, r6, r4                //add processed src and dst

    str      r6, [r0], #4

.Lexit:
    pop      {r4-r11, lr}
    bx       lr
