# =============================================================================
#
# Module: libCommon.a - classes shared by various modules
#
# =============================================================================

LOCAL_PATH	:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE	:= libMcCommon
LOCAL_MODULE_TAGS := optional

# Add new source files here
LOCAL_SRC_FILES +=\
	CMutex.cpp\
	Connection.cpp\
    NetlinkConnection.cpp\
	CSemaphore.cpp\
	CThread.cpp

# Header files required by components including this module
LOCAL_EXPORT_C_INCLUDES	:= $(LOCAL_PATH)
LOCAL_EXPORT_CPPFLAGS += -fno-rtti -fno-exceptions

LOCAL_C_INCLUDES += bionic \
	external/stlport/stlport

LOCAL_CPPFLAGS += -fno-rtti -fno-exceptions

include $(COMP_PATH_Logwrapper)/Android.mk

include $(BUILD_STATIC_LIBRARY)
