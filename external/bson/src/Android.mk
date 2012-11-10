LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libbson

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := bson.c numbers.c

LOCAL_CFLAGS := -DMONGO_HAVE_STDINT

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
