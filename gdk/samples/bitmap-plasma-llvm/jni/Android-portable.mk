LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := plasma_portable
LOCAL_SRC_FILES := libplasma.c

include $(BUILD_BITCODE)
