target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:64:128-a0:0:64-n32-S64"
target triple = "armv7-none-linux-gnueabi"

define <2 x float> @_Z14convert_float2Dv2_h(<2 x i8> %in) nounwind readnone alwaysinline {
  %1 = uitofp <2 x i8> %in to <2 x float>
  ret <2 x float> %1
}

define <3 x float> @_Z14convert_float3Dv3_h(<3 x i8> %in) nounwind readnone alwaysinline {
  %1 = uitofp <3 x i8> %in to <3 x float>
  ret <3 x float> %1
}

define <4 x float> @_Z14convert_float4Dv4_h(<4 x i8> %in) nounwind readnone alwaysinline {
  %1 = uitofp <4 x i8> %in to <4 x float>
  ret <4 x float> %1
}

define <2 x float> @_Z14convert_float2Dv2_c(<2 x i8> %in) nounwind readnone alwaysinline {
  %1 = sitofp <2 x i8> %in to <2 x float>
  ret <2 x float> %1
}

define <3 x float> @_Z14convert_float3Dv3_c(<3 x i8> %in) nounwind readnone alwaysinline {
  %1 = sitofp <3 x i8> %in to <3 x float>
  ret <3 x float> %1
}

define <4 x float> @_Z14convert_float4Dv4_c(<4 x i8> %in) nounwind readnone alwaysinline {
  %1 = sitofp <4 x i8> %in to <4 x float>
  ret <4 x float> %1
}

define <2 x float> @_Z14convert_float2Dv2_t(<2 x i16> %in) nounwind readnone alwaysinline {
  %1 = uitofp <2 x i16> %in to <2 x float>
  ret <2 x float> %1
}

define <3 x float> @_Z14convert_float3Dv3_t(<3 x i16> %in) nounwind readnone alwaysinline {
  %1 = uitofp <3 x i16> %in to <3 x float>
  ret <3 x float> %1
}

define <4 x float> @_Z14convert_float4Dv4_t(<4 x i16> %in) nounwind readnone alwaysinline {
  %1 = uitofp <4 x i16> %in to <4 x float>
  ret <4 x float> %1
}

define <2 x float> @_Z14convert_float2Dv2_s(<2 x i16> %in) nounwind readnone alwaysinline {
  %1 = sitofp <2 x i16> %in to <2 x float>
  ret <2 x float> %1
}

define <3 x float> @_Z14convert_float3Dv3_s(<3 x i16> %in) nounwind readnone alwaysinline {
  %1 = sitofp <3 x i16> %in to <3 x float>
  ret <3 x float> %1
}

define <4 x float> @_Z14convert_float4Dv4_s(<4 x i16> %in) nounwind readnone alwaysinline {
  %1 = sitofp <4 x i16> %in to <4 x float>
  ret <4 x float> %1
}

define <2 x float> @_Z14convert_float2Dv2_j(<2 x i32> %in) nounwind readnone alwaysinline {
  %1 = uitofp <2 x i32> %in to <2 x float>
  ret <2 x float> %1
}

define <3 x float> @_Z14convert_float3Dv3_j(<3 x i32> %in) nounwind readnone alwaysinline {
  %1 = uitofp <3 x i32> %in to <3 x float>
  ret <3 x float> %1
}

define <4 x float> @_Z14convert_float4Dv4_j(<4 x i32> %in) nounwind readnone alwaysinline {
  %1 = uitofp <4 x i32> %in to <4 x float>
  ret <4 x float> %1
}

define <2 x float> @_Z14convert_float2Dv2_i(<2 x i32> %in) nounwind readnone alwaysinline {
  %1 = sitofp <2 x i32> %in to <2 x float>
  ret <2 x float> %1
}

define <3 x float> @_Z14convert_float3Dv3_i(<3 x i32> %in) nounwind readnone alwaysinline {
  %1 = sitofp <3 x i32> %in to <3 x float>
  ret <3 x float> %1
}

define <4 x float> @_Z14convert_float4Dv4_i(<4 x i32> %in) nounwind readnone alwaysinline {
  %1 = sitofp <4 x i32> %in to <4 x float>
  ret <4 x float> %1
}

define <2 x float> @_Z14convert_float2Dv2_f(<2 x float> %in) nounwind readnone alwaysinline {
  ret <2 x float> %in
}

define <3 x float> @_Z14convert_float3Dv3_f(<3 x float> %in) nounwind readnone alwaysinline {
  ret <3 x float> %in
}

define <4 x float> @_Z14convert_float4Dv4_f(<4 x float> %in) nounwind readnone alwaysinline {
  ret <4 x float> %in
}

;---

define <4 x i8> @_Z14convert_uchar4Dv4_f(<4 x float> %in) nounwind readnone alwaysinline {
  %1 = fptoui <4 x float> %in to <4 x i32>
  %2 = trunc <4 x i32> %1 to <4 x i16>
  %3 = shufflevector <4 x i16> %2, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %4 = trunc <8 x i16> %3 to <8 x i8>
  %5 = shufflevector <8 x i8> %4, <8 x i8> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ret <4 x i8> %5
}

define <3 x i8> @_Z14convert_uchar3Dv3_f(<3 x float> %in) nounwind readnone alwaysinline {
  %in2 = shufflevector <3 x float> %in, <3 x float> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %1 = fptoui <4 x float> %in2 to <4 x i32>
  %2 = trunc <4 x i32> %1 to <4 x i16>
  %3 = shufflevector <4 x i16> %2, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %4 = trunc <8 x i16> %3 to <8 x i8>
  %5 = shufflevector <8 x i8> %4, <8 x i8> undef, <3 x i32> <i32 0, i32 1, i32 2>
  ret <3 x i8> %5
}

define <2 x i8> @_Z14convert_uchar2Dv2_f(<2 x float> %in) nounwind readnone alwaysinline {
  %in2 = shufflevector <2 x float> %in, <2 x float> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %1 = fptoui <4 x float> %in2 to <4 x i32>
  %2 = trunc <4 x i32> %1 to <4 x i16>
  %3 = shufflevector <4 x i16> %2, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %4 = trunc <8 x i16> %3 to <8 x i8>
  %5 = shufflevector <8 x i8> %4, <8 x i8> undef, <2 x i32> <i32 0, i32 1>
  ret <2 x i8> %5
}

define <4 x i8> @_Z14convert_uchar4Dv4_h(<4 x i8> %in) nounwind readnone alwaysinline {
  ret <4 x i8> %in
}

define <3 x i8> @_Z14convert_uchar3Dv3_h(<3 x i8> %in) nounwind readnone alwaysinline {
  ret <3 x i8> %in
}

define <2 x i8> @_Z14convert_uchar2Dv2_h(<2 x i8> %in) nounwind readnone alwaysinline {
  ret <2 x i8> %in
}

define <4 x i8> @_Z14convert_uchar4Dv4_c(<4 x i8> %in) nounwind readnone alwaysinline {
  ret <4 x i8> %in
}

define <3 x i8> @_Z14convert_uchar3Dv3_c(<3 x i8> %in) nounwind readnone alwaysinline {
  ret <3 x i8> %in
}

define <2 x i8> @_Z14convert_uchar2Dv2_c(<2 x i8> %in) nounwind readnone alwaysinline {
  ret <2 x i8> %in
}


define <4 x i8> @_Z14convert_uchar4Dv4_t(<4 x i16> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <4 x i16> %in, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %2 = trunc <8 x i16> %1 to <8 x i8>
  %3 = shufflevector <8 x i8> %2, <8 x i8> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ret <4 x i8> %3
}

define <3 x i8> @_Z14convert_uchar3Dv3_t(<3 x i16> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <3 x i16> %in, <3 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 5, i32 5>
  %2 = trunc <8 x i16> %1 to <8 x i8>
  %3 = shufflevector <8 x i8> %2, <8 x i8> undef, <3 x i32> <i32 0, i32 1, i32 2>
  ret <3 x i8> %3
}

define <2 x i8> @_Z14convert_uchar2Dv2_t(<2 x i16> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <2 x i16> %in, <2 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 2, i32 2, i32 2, i32 2, i32 2>
  %2 = trunc <8 x i16> %1 to <8 x i8>
  %3 = shufflevector <8 x i8> %2, <8 x i8> undef, <2 x i32> <i32 0, i32 1>
  ret <2 x i8> %3
}

define <4 x i8> @_Z14convert_uchar4Dv4_s(<4 x i16> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <4 x i16> %in, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %2 = trunc <8 x i16> %1 to <8 x i8>
  %3 = shufflevector <8 x i8> %2, <8 x i8> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ret <4 x i8> %3
}

define <3 x i8> @_Z14convert_uchar3Dv3_s(<3 x i16> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <3 x i16> %in, <3 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 5, i32 5>
  %2 = trunc <8 x i16> %1 to <8 x i8>
  %3 = shufflevector <8 x i8> %2, <8 x i8> undef, <3 x i32> <i32 0, i32 1, i32 2>
  ret <3 x i8> %3
}

define <2 x i8> @_Z14convert_uchar2Dv2_s(<2 x i16> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <2 x i16> %in, <2 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 2, i32 2, i32 2, i32 2, i32 2>
  %2 = trunc <8 x i16> %1 to <8 x i8>
  %3 = shufflevector <8 x i8> %2, <8 x i8> undef, <2 x i32> <i32 0, i32 1>
  ret <2 x i8> %3
}


define <4 x i8> @_Z14convert_uchar4Dv4_j(<4 x i32> %in) nounwind readnone alwaysinline {
  %1 = trunc <4 x i32> %in to <4 x i16>
  %2 = shufflevector <4 x i16> %1, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %3 = trunc <8 x i16> %2 to <8 x i8>
  %4 = shufflevector <8 x i8> %3, <8 x i8> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ret <4 x i8> %4
}

define <3 x i8> @_Z14convert_uchar3Dv3_j(<3 x i32> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <3 x i32> %in, <3 x i32> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %2 = trunc <4 x i32> %1 to <4 x i16>
  %3 = shufflevector <4 x i16> %2, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %4 = trunc <8 x i16> %3 to <8 x i8>
  %5 = shufflevector <8 x i8> %4, <8 x i8> undef, <3 x i32> <i32 0, i32 1, i32 2>
  ret <3 x i8> %5
}

define <2 x i8> @_Z14convert_uchar2Dv2_j(<2 x i32> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <2 x i32> %in, <2 x i32> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %2 = trunc <4 x i32> %1 to <4 x i16>
  %3 = shufflevector <4 x i16> %2, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %4 = trunc <8 x i16> %3 to <8 x i8>
  %5 = shufflevector <8 x i8> %4, <8 x i8> undef, <2 x i32> <i32 0, i32 1>
  ret <2 x i8> %5
}

define <4 x i8> @_Z14convert_uchar4Dv4_i(<4 x i32> %in) nounwind readnone alwaysinline {
  %1 = trunc <4 x i32> %in to <4 x i16>
  %2 = shufflevector <4 x i16> %1, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %3 = trunc <8 x i16> %2 to <8 x i8>
  %4 = shufflevector <8 x i8> %3, <8 x i8> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ret <4 x i8> %4
}

define <3 x i8> @_Z14convert_uchar3Dv3_i(<3 x i32> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <3 x i32> %in, <3 x i32> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %2 = trunc <4 x i32> %1 to <4 x i16>
  %3 = shufflevector <4 x i16> %2, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %4 = trunc <8 x i16> %3 to <8 x i8>
  %5 = shufflevector <8 x i8> %4, <8 x i8> undef, <3 x i32> <i32 0, i32 1, i32 2>
  ret <3 x i8> %5
}

define <2 x i8> @_Z14convert_uchar2Dv2_i(<2 x i32> %in) nounwind readnone alwaysinline {
  %1 = shufflevector <2 x i32> %in, <2 x i32> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %2 = trunc <4 x i32> %1 to <4 x i16>
  %3 = shufflevector <4 x i16> %2, <4 x i16> undef, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  %4 = trunc <8 x i16> %3 to <8 x i8>
  %5 = shufflevector <8 x i8> %4, <8 x i8> undef, <2 x i32> <i32 0, i32 1>
  ret <2 x i8> %5
}

