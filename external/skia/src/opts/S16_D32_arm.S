/*
 * Copyright (c) 2005-2008, The Android Open Source Project
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
    .align


    .global Blit_Pixel16ToPixel32
    .func Blit_Pixel16ToPixel32

//void Blit_Pixel16ToPixel32( uint32_t * colors, const uint16_t *srcAddr, int n );
// r0:  dst ptr
// r1:  src ptr
// r2:  n 

Blit_Pixel16ToPixel32:
    cmp      r2,#32
    blt      .Lless_than_32

    vld2.u8 {d0, d2}, [r1]! //q0, lower 8 bits
    vld2.u8 {d1, d3}, [r1]! //q1, higher 8 bits
    vmov.u8 q13, #0xff

.Lpipeline_neon_loop:
    pld     [r1, #256]
    vmov.u8 q12, #31
    vand.u8 q12, q12, q0   //q12 is B channels  

    vshr.u8 q11, q0, #5    //lower 3 bits of G channels
    vshl.u8 q14, q1, #5
    vshr.u8 q14, q14, #2   //higher 3 bits of G channels
    vorr   q11, q11, q14   //q11 is G channels

    vshr.u8 q10, q1, #3    //q10 is R channels

    sub      r2,r2,#16

    vld2.u8 {d0, d2}, [r1]! //q0, lower 8 bits
    vld2.u8 {d1, d3}, [r1]! //q1, higher 8 bits

    vshl.u8 q2, q12, #3 // b << (8-SK_B16_BITS) 
    vshr.u8 q12, q12, #2 // b >> (2* SK_B16_BITS -8 ) 
    vorr q12, q12, q2   //B channels 

    cmp      r2,#32
    vshl.u8 q3, q11, #2 // b << (8-SK_G16_BITS) 
    vshr.u8 q11, q11, #4 // b >> (2* SK_G16_BITS -8 ) 
    vorr q11, q11, q3   //G channels 

    vshl.u8 q8, q10, #3 // b << (8-SK_R16_BITS) 
    vshr.u8 q10, q10, #2 // b >> (2* SK_R16_BITS -8 ) 
    vorr q10, q10, q8   //R channels 


    vst4.u8 {d20, d22, d24, d26}, [r0]!
    vst4.u8 {d21, d23, d25, d27}, [r0]!
    bge  .Lpipeline_neon_loop

.Lsurfix:
    vmov.u8 q12, #31
    vand.u8 q12, q12, q0   //q12 is B channels  

    vshr.u8 q11, q0, #5    //lower 3 bits of G channels
    vshl.u8 q14, q1, #5
    vshr.u8 q14, q14, #2   //higher 3 bits of G channels
    vorr   q11, q11, q14   //q11 is G channels

    vshr.u8 q10, q1, #3    //q10 is R channels

    sub      r2,r2,#16

    vshl.u8 q2, q12, #3 // b << (8-SK_B16_BITS) 
    vshr.u8 q12, q12, #2 // b >> (2* SK_B16_BITS -8 ) 
    vorr q12, q12, q2   //B channels 

    vshl.u8 q3, q11, #2 // b << (8-SK_G16_BITS) 
    vshr.u8 q11, q11, #4 // b >> (2* SK_G16_BITS -8 ) 
    vorr q11, q11, q3   //G channels 

    vshl.u8 q8, q10, #3 // b << (8-SK_R16_BITS) 
    vshr.u8 q10, q10, #2 // b >> (2* SK_R16_BITS -8 ) 
    vorr q10, q10, q8   //R channels 

    vst4.u8 {d20, d22, d24, d26}, [r0]!
    vst4.u8 {d21, d23, d25, d27}, [r0]!


.Lless_than_32:
    cmp      r2,#16
    blt      .Lless_than_16
    //vpush    {Q4-Q7}

.Lneon_loop:
    pld     [r1, #256]
    vld2.u8 {d0, d2}, [r1]! //q0, lower 8 bits
    vld2.u8 {d1, d3}, [r1]! //q1, higher 8 bits

    vmov.u8 q12, #31
    vand.u8 q12, q12, q0   //q12 is B channels  

    vshr.u8 q11, q0, #5    //lower 3 bits of G channels
    vshl.u8 q14, q1, #5
    vshr.u8 q14, q14, #2   //higher 3 bits of G channels
    vorr   q11, q11, q14   //q11 is G channels

    vshr.u8 q10, q1, #3    //q10 is R channels

    sub      r2,r2,#16

    vshl.u8 q2, q12, #3 // b << (8-SK_B16_BITS) 
    vshr.u8 q12, q12, #2 // b >> (2* SK_B16_BITS -8 ) 
    vorr q12, q12, q2   //B channels 

    cmp      r2,#16
    vshl.u8 q3, q11, #2 // b << (8-SK_G16_BITS) 
    vshr.u8 q11, q11, #4 // b >> (2* SK_G16_BITS -8 ) 
    vorr q11, q11, q3   //G channels 

    vshl.u8 q8, q10, #3 // b << (8-SK_R16_BITS) 
    vshr.u8 q10, q10, #2 // b >> (2* SK_R16_BITS -8 ) 
    vorr q10, q10, q8   //R channels 

    vmov.u8 q13, #0xff

    vst4.u8 {d20, d22, d24, d26}, [r0]!
    vst4.u8 {d21, d23, d25, d27}, [r0]!
    bge  .Lneon_loop

    //vpop    {Q4-Q7}

.Lless_than_16:


    cmp r2, #0  // 0x0
    ble .Lend 

    push    {r4, r5, r6, r7, r8}


    lsl r2, r2, #1
    mov r3, #0  // 0x0

.Lloop:

    ldrh    r6, [r1, r3]

    and r5, r6, #31 // 0x1f //r5 is B

    ubfx    r4, r6, #5, #6 // r4 is G

    lsr ip, r6, #11  //ip is R


    lsl r8, r5, #3
    lsl r6, r4, #2
    lsr r7, ip, #2

    orr r5, r8, r5, lsr #2
    orr ip, r7, ip, lsl #3
    orr r4, r6, r4, lsr #4
    orr ip, ip, #-16777216  // 0xff000000
    orr r5, ip, r5, lsl #16
    orr r4, r5, r4, lsl #8
    str r4, [r0, r3, lsl #1]
    add r3, r3, #2  // 0x2

    cmp r3, r2
    bne .Lloop 

    pop {r4, r5, r6, r7, r8}
.Lend:
    bx  lr

