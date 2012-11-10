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
    .global S32_Opaque_D32_nofilter_DX_gether
    .func S32_Opaque_D32_nofilter_DX_gether
S32_Opaque_D32_nofilter_DX_gether:
          push     {r0-r11,lr}
          asr      r0,r2,#3
          sub      sp,sp,#4              //23
          cmp      r0,#0
          str      r0,[sp,#0] //r0 = count >> 3
          ble      .L1_140
          ldr      r4,[sp,#4] //r4 = r0 (dst)
          mov      r0,r3
          add      r12,r3,#4
          asr      r8,r2,#3
.L1_52:
          ldm      r3!, {r0,r6,r9,r11}
          lsr      r5,r0,#16            //30
          ldr      r5,[r1,r5,lsl #2]   //30
          lsr      r7,r6,#16            //32
          ldr      r7,[r1,r7,lsl #2]     //31
          uxth     r0,r0                 //34
          ldr      r0,[r1,r0,lsl #2]     //34
          uxth     r6,r6                 //31
          ldr      r6,[r1,r6,lsl #2]   //32
          //stm      r4!, {r0,r5,r6,r7}         ;35
          lsr      r10,r9,#16            //30
          ldr      r10,[r1,r10,lsl #2]   //30
          lsr      lr,r11,#16            //32
          ldr      lr,[r1,lr,lsl #2]     //31
          uxth     r9,r9                 //34
          ldr      r9,[r1,r9,lsl #2]     //34
          uxth     r11,r11                 //31
          ldr      r11,[r1,r11,lsl #2]   //32
          subs     r8,r8,#1
          stm      r4!, {r0,r5,r6,r7,r9,r10,r11,lr}         //35

          bne      .L1_52

          ldr      r0,[sp,#0]  // count >> 3
          mov      r12,r0
          ldr      r0,[sp,#4]  //r0 = dst
          add      r0,r0,r12,lsl #5 //dst += count >>3 << 5
          str      r0,[sp,#4]  //save r0 into stack again
.L1_140:
//;;39         const uint16_t* SK_RESTRICT xx = (const uint16_t*)(xy);
//;;40         for (i = (count & 7); i > 0; --i) {
          tst      r2,#7
          beq      .L1_184
          ldr      r0,[sp,#4]  //r0 = currnt dst
          and      r2,r2,#7
.L1_156:
//;;41             //SkASSERT(*xx < (unsigned)s.fBitmap->width());
//;;42             src = srcAddr[*xx++]; *colors++ = RETURNDST(src);
          ldrh     r4,[r3],#2
          add      r12,r0,#4
//;;43         }
          subs     r2,r2,#1
          ldr      r4,[r1,r4,lsl #2]     //42
          str      r4,[r0,#0]            //42
          mov      r0,r12                //42
          bne      .L1_156
.L1_184:
//;;44     }
          add      sp,sp,#0x14
          pop      {r4-r11,pc}

.endfunc
.size S32_Opaque_D32_nofilter_DX_gether, .-S32_Opaque_D32_nofilter_DX_gether
