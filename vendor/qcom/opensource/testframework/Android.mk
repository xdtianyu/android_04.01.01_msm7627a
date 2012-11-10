LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifdef TARGET_USES_TESTFRAMEWORK
#testframework lib
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := \
        src/TestFrameworkApi.cpp \
        src/TestFrameworkCommon.cpp \
        src/TestFrameworkHash.cpp \
        src/TestFramework.cpp \
        src/TestFrameworkService.cpp

LOCAL_CFLAGS := -DCUSTOM_EVENTS_TESTFRAMEWORK

LOCAL_C_INCLUDES := $(TOP)/vendor/qcom/opensource/testframework

LOCAL_SHARED_LIBRARIES += \
        libutils \
        libcutils \
        libbinder

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libtestframework
include $(BUILD_SHARED_LIBRARY)

#testframework servcice
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        src/TestFrameworkServiceMain.cpp \
	src/TFSShell.cpp

LOCAL_C_INCLUDES := vendor/qcom/opensource/testframework

LOCAL_SHARED_LIBRARIES := libtestframework libcutils libutils libbinder

LOCAL_CFLAGS := -DCUSTOM_EVENTS_TESTFRAMEWORK
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= testframeworkservice

include $(BUILD_EXECUTABLE)

endif
