/*
 * Copyright 2006, The Android Open Source Project
 * Copyright (c) 2009, Code Aurora Forum.
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


/*
 * This file is derived from libpixelflinger version of BLIT routine.
 * Algorithm used for BLIT operation here is equivalent to the one in
 * C function, S32A_D565_Opaque. Use neon instructions to process 16 pixels
 * at-a-time on armv7. If the number of pixels is less than 16 and/or the
 * architecture is armv6 and below, use regular arm instructions. Regular
 * arm code combines two 16-bit writes into one 32-bit write to destination,
 * uses destination and source pre-loads, and unrolls the main loop thrice.
 */
    .text
    .align

    .global S32A_D565_Opaque_arm

// uses r6, r7, r8, r9, r10, lr

.macro pixel,   DREG, SRC, FB, OFFSET

    // SRC = AABBGGRR
    subs   r7, r10, \SRC, lsr #24           // sAA = 255 - sAA
    beq    1f

.if \OFFSET

    // red
    mov     lr, \DREG, lsr #(\OFFSET + 6 + 5)
    smlabb  lr, r7, lr, r8
    and     r6, \SRC, r10
    add     lr, lr, lr, lsr #5
    add     lr, r6, lr, lsr #5
    lsr     lr, #3
    orr     \FB, lr, lsl #(\OFFSET + 11)

        // green
        and     r6, \DREG, #(0x3F<<(\OFFSET + 5))
        lsr     r6, #5
        smlabt  r6, r7, r6, r9
        and     lr, r10, \SRC, lsr #(8)
        add     r6, r6, r6, lsr #6
        add     r6, lr, r6, lsr #6
        lsr     r6, #2
        orr     \FB, \FB, r6, lsl #(\OFFSET + 5)

            // blue
            and     lr, \DREG, #(0x1F << \OFFSET)
            smlabt  lr, r7, lr, r8
            and     r6, r10, \SRC, lsr #(8+8)
            add     lr, lr, lr, lsr #5
            add     lr, r6, lr, lsr #5
            lsr     lr, #3
            orr     \FB, \FB, lr, lsl #\OFFSET

.else

    // red
    mov     lr, \DREG, lsr #(6+5)
    and     lr, lr, #0x1F
    smlabb  lr, r7, lr, r8
    and     r6, \SRC, r10
    add     lr, lr, lr, lsr #5
    add     lr, r6, lr, lsr #5
    lsr     lr, #3
    mov     \FB, lr, lsl #11

        // green
        and     r6, \DREG, #(0x3F<<5)
        lsr     r6, #5
        smlabb  r6, r7, r6, r9
        and     lr, r10, \SRC, lsr #(8)
        add     r6, r6, r6, lsr #6
        add     r6, lr, r6, lsr #6
        lsr     r6, #2
        orr     \FB, \FB, r6, lsl #5

            // blue
            and     lr, \DREG, #0x1F
            smlabb  lr, r7, lr, r8
            and     r6, r10, \SRC, lsr #(8+8)
            add     lr, lr, lr, lsr #5
            add     lr, r6, lr, lsr #5
            orr     \FB, \FB, lr, lsr #3

.endif
   b      2f

   /*
    * When alpha = 255, down scale the source RGB pixel (24 bits)
    * to 16 bits(RGB565)
    */
1:
    lsl    r6, \SRC, #8
    lsr    lr, \SRC, #5
    and    r7, r6, #0xf800
    and    lr, lr, #0x7e0
    orr    lr, lr, r7

.if \OFFSET
    orr    lr, lr, r6, lsr #27
    orr    \FB, \FB, lr, lsl #(\OFFSET)
.else
    orr    \FB, lr, r6, lsr #27
.endif

2:
.endm


// r0:  dst ptr
// r1:  src ptr
// r2:  count
// r3:  d
// r4:  s0
// r5:  s1
// r6:  pixel
// r7:  pixel
// r8:  0x10
// r9:  0x20
// r10: 0xFF
// r11: free
// r12: scratch
// r14: free

S32A_D565_Opaque_arm:
    stmfd	sp!, {r4-r10, lr}

#if __ARM_ARCH__ == 7 || defined(__ARM_NEON__)
    subs    r2, r2, #16

    blo     blit_less_than_16_left

    vmov.u16 q12,  #0x80
    vmov.u8  q13,  #0xf8

blit_neon_loop:
    /*
     * Load 64 bytes from source and 32 bytes from destination
     * note that source pixels are 4 bytes wide and
     * destination pixels are 2 bytes wide.
     */
    vld4.8  {d2, d4, d6, d8}, [r1]!
    vld4.8  {d3, d5, d7, d9}, [r1]!

    vand.8  d10, d8, d9
    vmov    r3, r4, d10

    cmp     r3, #0xffffffff
    cmpeq   r4, #0xffffffff
    bne     blit_alpha_not_255

    // alpha equals 255 case

    vshl.u8   q0, q2, #3

    subs    r2, r2, #16

    vsri.u8   q1, q2, #5
    vsri.u8   q0, q3, #3

    // store the rgb destination values back to memory
    vst2.8  {d0, d2}, [r0]!
    vst2.8  {d1, d3}, [r0]!

    blo     blit_less_than_16_left
    b       blit_neon_loop

blit_alpha_not_255:
    // alpha = 255 - alpha
    vmvn.u8 q0, q4

    vld2.8 {q5, q6}, [r0]

    vshl.u8 q7, q6, #3

    subs    r2, r2, #16

    vand.u8 q6, q6, q13

    vmov.16   q8, q12
    vmov.16   q9, q12

    vsri.u8 q7, q5, #5
    vshl.u8 q5, q5, #3

    vmlal.u8 q8, d0, d12
    vmlal.u8 q9, d1, d13

    vshl.u8 q7, q7, #2

    vshr.u16  q10, q8, #5
    vshr.u16  q11, q9, #5
    vaddhn.u16 d12, q8, q10
    vaddhn.u16 d13, q9, q11

    vmov.16   q8, q12
    vmov.16   q9, q12
    vmlal.u8 q8, d0, d14
    vmlal.u8 q9, d1, d15

    vqadd.u8  q6, q6, q1

    vshr.u16  q10, q8, #6
    vshr.u16  q11, q9, #6
    vaddhn.u16 d14, q8, q10
    vaddhn.u16 d15, q9, q11

    vmov.16   q8, q12
    vmov.16   q9, q12
    vmlal.u8 q8, d0, d10
    vmlal.u8 q9, d1, d11

    vqadd.u8  q7, q7, q2

    vshl.u8  q5, q7, #3

    vshr.u16  q10, q8, #5
    vshr.u16  q11, q9, #5

    vsri.u8  q6, q7, #5

    vaddhn.u16 d16, q8, q10
    vaddhn.u16 d17, q9, q11
    vqadd.u8  q8, q8, q3

    vsri.u8  q5, q8, #3

    // store the rgb destination values back to memory
    vst2.8  {d10, d12}, [r0]!
    vst2.8  {d11, d13}, [r0]!

    blo     blit_less_than_16_left
    b       blit_neon_loop
#endif

blit_less_than_16_left:
    pld     [r1]

    mov     r8,  #0x10
    mov     r9,  #0x20
    mov     r10, #0xFF

#if __ARM_ARCH__ == 7 || defined(__ARM_NEON__)
    adds    r2, r2, #14
#else
    subs    r2, r2, #2
#endif

    pld     [r0]
    blo     9f

    // The main loop is unrolled thrice and process 6 pixels
8:  ldmia   r1!, {r4, r5}
    // stream the source
    pld     [r1, #32]
    add     r0, r0, #4
    // it's all zero, skip this pixel
    orrs    r3, r4, r5
    beq     7f

    // load the destination
    ldr     r3, [r0, #-4]
    // stream the destination
    pld     [r0, #32]
    pixel   r3, r4, r12, 0
    pixel   r3, r5, r12, 16
    // effectively, we're getting write-combining by virtue of the
    // cpu's write-back cache.
    str     r12, [r0, #-4]

    // 2nd iteration of the loop, don't stream anything
    subs    r2, r2, #2
    blt     9f
    ldmia   r1!, {r4, r5}
    add     r0, r0, #4
    orrs    r3, r4, r5
    beq     7f
    ldr     r3, [r0, #-4]
    pixel   r3, r4, r12, 0
    pixel   r3, r5, r12, 16
    str     r12, [r0, #-4]

    // 3rd iteration of the loop, don't stream anything
    subs    r2, r2, #2
    blt     9f
    ldmia   r1!, {r4, r5}
    add     r0, r0, #4
    orrs    r3, r4, r5
    beq     7f
    ldr     r3, [r0, #-4]
    pixel   r3, r4, r12, 0
    pixel   r3, r5, r12, 16
    str     r12, [r0, #-4]

7:  subs    r2, r2, #2
    blo     9f
    b       8b

9:  adds    r2, r2, #1
    ldmlofd sp!, {r4-r10, lr}        // return
    bxlo    lr

    // last pixel left
    ldr     r4, [r1], #4
    ldrh    r3, [r0]
    pixel   r3, r4, r12, 0
    strh    r12, [r0], #2
    ldmfd   sp!, {r4-r10, lr}        // return
    bx      lr
