target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:64:128-a0:0:64-n32-S64"
target triple = "armv7-none-linux-gnueabi"

define internal <4 x float> @smear_4f(float %in) nounwind readnone alwaysinline {
  %1 = insertelement <4 x float> undef, float %in, i32 0
  %2 = insertelement <4 x float> %1, float %in, i32 1
  %3 = insertelement <4 x float> %2, float %in, i32 2
  %4 = insertelement <4 x float> %3, float %in, i32 3
  ret <4 x float> %4
}

define internal <2 x float> @smear_2f(float %in) nounwind readnone alwaysinline {
  %1 = insertelement <2 x float> undef, float %in, i32 0
  %2 = insertelement <2 x float> %1, float %in, i32 1
  ret <2 x float> %2
}

declare <2 x float> @llvm.arm.neon.vmins.v2f32(<2 x float>, <2 x float>) nounwind readnone
declare <4 x float> @llvm.arm.neon.vmins.v4f32(<4 x float>, <4 x float>) nounwind readnone
declare <2 x float> @llvm.arm.neon.vmaxs.v2f32(<2 x float>, <2 x float>) nounwind readnone
declare <4 x float> @llvm.arm.neon.vmaxs.v4f32(<4 x float>, <4 x float>) nounwind readnone

define <4 x float> @_Z5clampDv4_fS_S_(<4 x float> %value, <4 x float> %low, <4 x float> %high) nounwind readonly {
  %1 = tail call <4 x float> @llvm.arm.neon.vmins.v4f32(<4 x float> %value, <4 x float> %high) nounwind readnone
  %2 = tail call <4 x float> @llvm.arm.neon.vmaxs.v4f32(<4 x float> %1, <4 x float> %low) nounwind readnone
  ret <4 x float> %2
}

define <4 x float> @_Z5clampDv4_fff(<4 x float> %value, float %low, float %high) nounwind readonly {
  %_high = tail call <4 x float> @smear_4f(float %high) nounwind readnone
  %_low = tail call <4 x float> @smear_4f(float %low) nounwind readnone
  %out = tail call <4 x float> @_Z5clampDv4_fS_S_(<4 x float> %value, <4 x float> %_low, <4 x float> %_high) nounwind readonly
  ret <4 x float> %out
}

define <3 x float> @_Z5clampDv3_fS_S_(<3 x float> %value, <3 x float> %low, <3 x float> %high) nounwind readonly {
  %_value = shufflevector <3 x float> %value, <3 x float> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %_low = shufflevector <3 x float> %low, <3 x float> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %_high = shufflevector <3 x float> %high, <3 x float> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %a = tail call <4 x float> @llvm.arm.neon.vmins.v4f32(<4 x float> %_value, <4 x float> %_high) nounwind readnone
  %b = tail call <4 x float> @llvm.arm.neon.vmaxs.v4f32(<4 x float> %a, <4 x float> %_low) nounwind readnone
  %c = shufflevector <4 x float> %b, <4 x float> undef, <3 x i32> <i32 0, i32 1, i32 2>
  ret <3 x float> %c
}

define <3 x float> @_Z5clampDv3_fff(<3 x float> %value, float %low, float %high) nounwind readonly {
  %_value = shufflevector <3 x float> %value, <3 x float> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %_high = tail call <4 x float> @smear_4f(float %high) nounwind readnone
  %_low = tail call <4 x float> @smear_4f(float %low) nounwind readnone
  %a = tail call <4 x float> @llvm.arm.neon.vmins.v4f32(<4 x float> %_value, <4 x float> %_high) nounwind readnone
  %b = tail call <4 x float> @llvm.arm.neon.vmaxs.v4f32(<4 x float> %a, <4 x float> %_low) nounwind readnone
  %c = shufflevector <4 x float> %b, <4 x float> undef, <3 x i32> <i32 0, i32 1, i32 2>
  ret <3 x float> %c
}


define <2 x float> @_Z5clampDv2_fS_S_(<2 x float> %value, <2 x float> %low, <2 x float> %high) nounwind readonly {
  %1 = tail call <2 x float> @llvm.arm.neon.vmins.v2f32(<2 x float> %value, <2 x float> %high) nounwind readnone
  %2 = tail call <2 x float> @llvm.arm.neon.vmaxs.v2f32(<2 x float> %1, <2 x float> %low) nounwind readnone
  ret <2 x float> %2
}

define <2 x float> @_Z5clampDv2_fff(<2 x float> %value, float %low, float %high) nounwind readonly {
  %_high = tail call <2 x float> @smear_2f(float %high) nounwind readnone
  %_low = tail call <2 x float> @smear_2f(float %low) nounwind readnone
  %a = tail call <2 x float> @llvm.arm.neon.vmins.v2f32(<2 x float> %value, <2 x float> %_high) nounwind readnone
  %b = tail call <2 x float> @llvm.arm.neon.vmaxs.v2f32(<2 x float> %a, <2 x float> %_low) nounwind readnone
  ret <2 x float> %b
}


define float @_Z5clampfff(float %value, float %low, float %high) nounwind readonly {
  %_value = tail call <2 x float> @smear_2f(float %value) nounwind readnone
  %_low = tail call <2 x float> @smear_2f(float %low) nounwind readnone
  %_high = tail call <2 x float> @smear_2f(float %high) nounwind readnone
  %a = tail call <2 x float> @llvm.arm.neon.vmins.v2f32(<2 x float> %_value, <2 x float> %_high) nounwind readnone
  %b = tail call <2 x float> @llvm.arm.neon.vmaxs.v2f32(<2 x float> %a, <2 x float> %_low) nounwind readnone
  %c = extractelement <2 x float> %b, i32 0
  ret float %c
}

