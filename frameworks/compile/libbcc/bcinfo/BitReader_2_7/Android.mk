LOCAL_PATH:= $(call my-dir)

LLVM_ROOT_PATH := $(call get_llvm_root_path)
include $(LLVM_ROOT_PATH)/llvm.mk

bitcode_reader_2_7_SRC_FILES := \
  BitcodeReader.cpp

# For the host
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(bitcode_reader_2_7_SRC_FILES)

LOCAL_CFLAGS += -D__HOST__

LOCAL_MODULE:= lib${LLVM_VER}LLVMBitReader_2_7

LOCAL_MODULE_TAGS := optional

include $(LLVM_HOST_BUILD_MK)
include $(LLVM_GEN_INTRINSICS_MK)
include $(BUILD_HOST_STATIC_LIBRARY)

# For the device
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(bitcode_reader_2_7_SRC_FILES)

LOCAL_MODULE:= lib${LLVM_VER}LLVMBitReader_2_7

LOCAL_MODULE_TAGS := optional

include $(LLVM_DEVICE_BUILD_MK)
include $(LLVM_GEN_INTRINSICS_MK)
include $(BUILD_STATIC_LIBRARY)
