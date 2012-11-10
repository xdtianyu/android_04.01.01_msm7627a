/*
 * Copyright (C) 2012 The Android Open Source Project
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


#include "rs_types.rsh"

extern float __attribute__((overloadable)) clamp(float amount, float low, float high) {
    return amount < low ? low : (amount > high ? high : amount);
}
#ifdef QCOM_LLVM
extern float2 __attribute__((overloadable)) clamp(float2 amount, float2 low, float2 high) {
    float2 r;
    r = amount < low ? low : ( amount > high ? high : amount );
    return r;
}
extern float3 __attribute__((overloadable)) clamp(float3 amount, float3 low, float3 high) {
    float3 r;
    r = amount < low ? low : ( amount > high ? high : amount );
    return r;
}
extern float4 __attribute__((overloadable)) clamp(float4 amount, float4 low, float4 high) {
    float4 r;
    r = amount < low ? low : ( amount > high ? high : amount );
    return r;
}
extern float2 __attribute__((overloadable)) clamp(float2 amount, float low, float high) {
    float2 r;
    float2 vlow = low;
    float2 vhigh = high;
    r = amount < vlow ? vlow : ( amount > vhigh ? vhigh : amount );
    return r;
}
extern float3 __attribute__((overloadable)) clamp(float3 amount, float low, float high) {
    float3 r;
    float3 vlow = low;
    float3 vhigh = high;
    r = amount < vlow ? vlow : ( amount > vhigh ? vhigh : amount );
    return r;
}
extern float4 __attribute__((overloadable)) clamp(float4 amount, float low, float high) {
    float4 r;
    float4 vlow = low;
    float4 vhigh = high;
    r = amount < vlow ? vlow : ( amount > vhigh ? vhigh : amount );
    return r;
}

#else
extern float2 __attribute__((overloadable)) clamp(float2 amount, float2 low, float2 high) {
    float2 r;
    r.x = amount.x < low.x ? low.x : (amount.x > high.x ? high.x : amount.x);
    r.y = amount.y < low.y ? low.y : (amount.y > high.y ? high.y : amount.y);
    return r;
}

extern float3 __attribute__((overloadable)) clamp(float3 amount, float3 low, float3 high) {
    float3 r;
    r.x = amount.x < low.x ? low.x : (amount.x > high.x ? high.x : amount.x);
    r.y = amount.y < low.y ? low.y : (amount.y > high.y ? high.y : amount.y);
    r.z = amount.z < low.z ? low.z : (amount.z > high.z ? high.z : amount.z);
    return r;
}

extern float4 __attribute__((overloadable)) clamp(float4 amount, float4 low, float4 high) {
    float4 r;
    r.x = amount.x < low.x ? low.x : (amount.x > high.x ? high.x : amount.x);
    r.y = amount.y < low.y ? low.y : (amount.y > high.y ? high.y : amount.y);
    r.z = amount.z < low.z ? low.z : (amount.z > high.z ? high.z : amount.z);
    r.w = amount.w < low.w ? low.w : (amount.w > high.w ? high.w : amount.w);
    return r;
}

extern float2 __attribute__((overloadable)) clamp(float2 amount, float low, float high) {
    float2 r;
    r.x = amount.x < low ? low : (amount.x > high ? high : amount.x);
    r.y = amount.y < low ? low : (amount.y > high ? high : amount.y);
    return r;
}

extern float3 __attribute__((overloadable)) clamp(float3 amount, float low, float high) {
    float3 r;
    r.x = amount.x < low ? low : (amount.x > high ? high : amount.x);
    r.y = amount.y < low ? low : (amount.y > high ? high : amount.y);
    r.z = amount.z < low ? low : (amount.z > high ? high : amount.z);
    return r;
}

extern float4 __attribute__((overloadable)) clamp(float4 amount, float low, float high) {
    float4 r;
    r.x = amount.x < low ? low : (amount.x > high ? high : amount.x);
    r.y = amount.y < low ? low : (amount.y > high ? high : amount.y);
    r.z = amount.z < low ? low : (amount.z > high ? high : amount.z);
    r.w = amount.w < low ? low : (amount.w > high ? high : amount.w);
    return r;
}
#endif


