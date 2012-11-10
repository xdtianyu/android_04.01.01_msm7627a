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

    .global S32A_Opaque_BlitRow32_arm
    .func S32A_Opaque_BlitRow32_arm

S32A_Opaque_BlitRow32_arm:

    push     {r4-r11}
#if __ARM_ARCH__ == 7 || defined(__ARM_NEON__)

    cmp      r2,#24
    blt      .Lless_than_24

    vpush    {Q4-Q7}

    vmov.i16 q14,#0x100               //Q14.16 = 256
//prefix
    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)
                                    //update source ptr but not dst ptr
    vld4.8  {d4, d5, d6, d7}, [r0]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)
    add      r3, r0, #32 // minus 16 to pretend the last round
    mov      r5, #64
    sub      r2,r2,#8
.Lloop:
    pld     [r1, #256]
    pld     [r0, #256]
    sub      r2,r2,#16
    vsubw.u8 q4,q14,d3               //Q4.16 = 256-d3

    //It has to be 24 since we pre-load 8 word for the next rounds
    cmp      r2,#16

    vmovl.u8 q6,d4                  //Q6 = vmovl.u8 d4
    vmovl.u8 q7,d5                  //Q7 = vmovl.u8 d5
    vmovl.u8 q8,d6                  //Q8 = vmovl.u8 d6
    vmovl.u8 q9,d7                  //Q9 = vmovl.u8 d7


    vmul.i16 q6,q6,q4               //Q6 = Q6 * Q4
    vmul.i16 q7,q7,q4               //Q7 = Q7 * Q4

    vld4.8  {d20, d21, d22, d23}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)

    vmul.i16 q8,q8,q4               //Q8 = Q8 * Q4
    vmul.i16 q9,q9,q4               //Q9 = Q9 * Q4

    vld4.8  {d24, d25, d26, d27}, [r3]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)

    vshrn.i16 d4,q6,#8              //d4 = Q6.16 shrn 8
    vshrn.i16 d5,q7,#8              //d5 = Q7.16 shrn 8
    vshrn.i16 d6,q8,#8              //d6 = Q8.16 shrn 8
    vshrn.i16 d7,q9,#8              //d7 = Q9.16 shrn 8

    vadd.i8  d4,d4,d0               //d4 = d4+d0
    vadd.i8  d5,d5,d1               //d5 = d5+d1
    vadd.i8  d6,d6,d2               //d6 = d6+d2
    vadd.i8  d7,d7,d3               //d7 = d7+d3

    vst4.8  {d4, d5, d6, d7}, [r0], r5 //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7
    //add r0, r0, r5

    //The next 4 words
    vsubW.u8 q4,q14,d23               //Q4.16 = 256-d3

    vmovl.u8 q6,d24                  //Q6 = vmovl.u8 d4
    vmovl.u8 q7,d25                  //Q7 = vmovl.u8 d5
    vmovl.u8 q8,d26                  //Q8 = vmovl.u8 d6
    vmovl.u8 q9,d27                  //Q9 = vmovl.u8 d7

    vmul.i16 q6,q6,q4               //Q6 = Q6 * Q4
    vmul.i16 q7,q7,q4               //Q7 = Q7 * Q4

    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)

    vmul.i16 q8,q8,q4               //Q8 = Q8 * Q4
    vmul.i16 q9,q9,q4               //Q9 = Q9 * Q4

    vld4.8  {d4, d5, d6, d7}, [r0]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)
    vshrn.i16 d24,q6,#8              //d4 = Q6.16 shrn 8
    vshrn.i16 d25,q7,#8              //d5 = Q7.16 shrn 8
    vshrn.i16 d26,q8,#8              //d6 = Q8.16 shrn 8
    vshrn.i16 d27,q9,#8              //d7 = Q9.16 shrn 8

    vadd.i8  d24,d24,d20               //d4 = d4+d0
    vadd.i8  d25,d25,d21               //d5 = d5+d1
    vadd.i8  d26,d26,d22               //d6 = d6+d2
    vadd.i8  d27,d27,d23               //d7 = d7+d3

    vst4.8  {d24, d25, d26, d27}, [r3], r5 //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7
    //add r3, r3, r5

    bge      .Lloop

//There are 8 words left unprocessed from previous round
    vsubw.u8 q4,q14,d3               //Q4.16 = 256-d3

    cmp      r2,#8

    vmovl.u8 q6,d4                  //Q6 = vmovl.u8 d4
    vmovl.u8 q7,d5                  //Q7 = vmovl.u8 d5
    vmovl.u8 q8,d6                  //Q8 = vmovl.u8 d6
    vmovl.u8 q9,d7                  //Q9 = vmovl.u8 d7

    vmul.i16 q6,q6,q4               //Q6 = Q6 * Q4
    vmul.i16 q7,q7,q4               //Q7 = Q7 * Q4
    vmul.i16 q8,q8,q4               //Q8 = Q8 * Q4
    vmul.i16 q9,q9,q4               //Q9 = Q9 * Q4

    vshrn.i16 d4,q6,#8              //d4 = Q6.16 shrn 8
    vshrn.i16 d5,q7,#8              //d5 = Q7.16 shrn 8
    vshrn.i16 d6,q8,#8              //d6 = Q8.16 shrn 8
    vshrn.i16 d7,q9,#8              //d7 = Q9.16 shrn 8

    vadd.i8  d4,d4,d0               //d4 = d4+d0
    vadd.i8  d5,d5,d1               //d5 = d5+d1
    vadd.i8  d6,d6,d2               //d6 = d6+d2
    vadd.i8  d7,d7,d3               //d7 = d7+d3

    vst4.8  {d4, d5, d6, d7}, [r0]! //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7

.Lless_than_16:
    cmp      r2,#8
    blt      .Lless_than_8

    sub      r2,r2,#8

    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)
                                    //update source ptr but not dst ptr
    vld4.8  {d4, d5, d6, d7}, [r0]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)

    vsubw.u8 q4,q14,d3               //Q4.16 = 256-d3

    vmovl.u8 q6,d4                  //Q6 = vmovl.u8 d4
    vmovl.u8 q7,d5                  //Q7 = vmovl.u8 d5
    vmovl.u8 q8,d6                  //Q8 = vmovl.u8 d6
    vmovl.u8 q9,d7                  //Q9 = vmovl.u8 d7

    vmul.i16 q6,q6,q4               //Q6 = Q6 * Q4
    vmul.i16 q7,q7,q4               //Q7 = Q7 * Q4
    vmul.i16 q8,q8,q4               //Q8 = Q8 * Q4
    vmul.i16 q9,q9,q4               //Q9 = Q9 * Q4

    vshrn.i16 d4,q6,#8              //d4 = Q6.16 shrn 8
    vshrn.i16 d5,q7,#8              //d5 = Q7.16 shrn 8
    vshrn.i16 d6,q8,#8              //d6 = Q8.16 shrn 8
    vshrn.i16 d7,q9,#8              //d7 = Q9.16 shrn 8

    vadd.i8  d4,d4,d0               //d4 = d4+d0
    vadd.i8  d5,d5,d1               //d5 = d5+d1
    vadd.i8  d6,d6,d2               //d6 = d6+d2
    vadd.i8  d7,d7,d3               //d7 = d7+d3

    vst4.8  {d4, d5, d6, d7}, [r0]! //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7

.Lless_than_8:
    vpop     {Q4-Q7}

.Lless_than_4:
    cmp      r2, #1
    bmi      .Lexit
    b        .Lresidual_loop

.Lless_than_24:
    cmp      r2,#8
    blt      .Lless_than_4

.Lloop_8:
    sub      r2,r2,#8
    // We already read the 8 words from the previous pipe line
    vld4.8  {d0, d1, d2, d3}, [r1]! //d0,d1,d2,d3 = sourc rgb(0,1,2,3) A(0,1,2,3)
                                    //update source ptr but not dst ptr
    vld4.8  {d4, d5, d6, d7}, [r0]  //d4,d5,d6,d7 = dst rgb(0,1,2,3) A(0,1,2,3)

    vmov.i16 q10,#0x100               //Q4.16 = 256
    vsubW.u8 q10,q10,d3               //Q4.16 = 256-d3

    cmp      r2,#8

    vmovl.u8 q12,d4                  //Q6 = vmovl.u8 d4
    vmovl.u8 q13,d5                  //Q7 = vmovl.u8 d5
    vmovl.u8 q8,d6                  //Q8 = vmovl.u8 d6
    vmovl.u8 q9,d7                  //Q9 = vmovl.u8 d7

    vmul.i16 q12,q12,q10               //Q6 = Q6 * Q4
    vmul.i16 q13,q13,q10               //Q7 = Q7 * Q4
    vmul.i16 q8,q8,q10               //Q8 = Q8 * Q4
    vmul.i16 q9,q9,q10               //Q9 = Q9 * Q4

    vshrn.i16 d4,q12,#8              //d4 = Q6.16 shrn 8
    vshrn.i16 d5,q13,#8              //d5 = Q7.16 shrn 8
    vshrn.i16 d6,q8,#8              //d6 = Q8.16 shrn 8
    vshrn.i16 d7,q9,#8              //d7 = Q9.16 shrn 8

    vadd.i8  d4,d4,d0               //d4 = d4+d0
    vadd.i8  d5,d5,d1               //d5 = d5+d1
    vadd.i8  d6,d6,d2               //d6 = d6+d2
    vadd.i8  d7,d7,d3               //d7 = d7+d3

    vst4.8  {d4, d5, d6, d7}, [r0]! //dst rgb(0,1,2,3) A(0,1,2,3) = d4,d5,d6,d7

    bge      .Lloop_8
    b        .Lless_than_4

#endif

/*
 * r0 - dst
 * r1 - src
 * r2 - count
 */
.Lresidual_loop:
    mov      r10, #0xFF
    orr      r10, r10, r10, lsl #16    //mask = r10 = 0x00FF00FF

    subs     r2, r2, #2
    blt      .Lblitrow32_single_loop

.Lblitrow32_double_loop:
    ldm      r0, {r3, r4}
    ldm      r1!, {r5, r6}

    orrs     r9, r3, r4
    beq      .Lblitrow32_loop_cond

    // First iteration
    lsr      r7, r5, #24               //extract alpha
    and      r8, r3, r10               //rb = (dst & mask)
    rsb      r7, r7, #256              //r5 = scale = (255-alpha)+1
    and      r9, r10, r3, lsr #8       //ag = (dst>>8) & mask

    mul      r11, r8, r7               //RB = rb * scale
    mul      r3, r9, r7                //AG = ag * scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r3, r3, r10, lsl #8       //r9 = AG & ~mask

    lsr      r7, r6, #24               //extract alpha for second iteration
    orr      r3, r3, r11

    // Second iteration
    and      r8, r4, r10               //rb = (dst & mask)
    rsb      r7, r7, #256              //r5 = scale = (255-alpha)+1
    and      r9, r10, r4, lsr #8       //ag = (dst>>8) & mask

    mul      r11, r8, r7               //RB = rb * scale
    mul      r4, r9, r7                //AG = ag * scale

    // combine RB and AG
    and      r11, r10, r11, lsr #8     //r8 = (RB>>8) & mask
    and      r4, r4, r10, lsl #8       //r9 = AG & ~mask
    orr      r4, r4, r11

    // add src to combined value
    add      r5, r5, r3
    add      r6, r6, r4

.Lblitrow32_loop_cond:
    subs     r2, r2, #2
    stm      r0!, {r5, r6}

    bge      .Lblitrow32_double_loop

.Lblitrow32_single_loop:
    adds     r2, #1
    blo      .Lexit

    ldr      r3, [r0]
    ldr      r5, [r1], #4

    cmp      r3, #0
    beq      .Lblitrow32_single_store

    lsr      r7, r5, #24               //extract alpha
    and      r8, r3, r10               //rb = (dst & mask)
    rsb      r7, r7, #256              //r5 = scale = (255-alpha)+1
    and      r9, r10, r3, lsr #8       //ag = (dst>>8) & mask

    mul      r8, r8, r7                //RB = rb * scale
    mul      r9, r9, r7                //AG = ag * scale

    // combine RB and AG
    and      r8, r10, r8, lsr #8       //r8 = (RB>>8) & mask
    and      r9, r9, r10, lsl #8       //r9 = AG & ~mask
    orr      r3, r8, r9

    add      r5, r5, r3                //add src to combined value

.Lblitrow32_single_store:
    str      r5, [r0], #4

.Lexit:
    pop      {r4-r11}
    bx       lr
