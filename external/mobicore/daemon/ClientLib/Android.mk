# =============================================================================
#
# Module: libMcClient.so
# 
# C(version) Client Lib for Linux TLCs
#
# =============================================================================

LOCAL_PATH	:= $(call my-dir)
MY_CLIENTLIB_PATH	:= $(LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE	:= libMcClient
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

# External include files
LOCAL_C_INCLUDES += bionic \
	external/stlport/stlport

# Add new folders with header files here
LOCAL_C_INCLUDES +=\
	$(LOCAL_PATH)/public \
	$(APP_PROJECT_PATH) \
	$(APP_PROJECT_PATH)/Daemon/public \
	$(APP_PROJECT_PATH)/Kernel \
	$(APP_PROJECT_PATH)/Kernel/Platforms/Generic \
	$(APP_PROJECT_PATH)/Common

# Add new folders with header files here
LOCAL_C_INCLUDES +=\
	$(COMP_PATH_MobiCore)/inc \
	$(COMP_PATH_MobiCoreDriverMod)/Public

# Add new source files here
LOCAL_SRC_FILES +=\
    Device.cpp\
    ClientLib.cpp\
    Session.cpp

LOCAL_STATIC_LIBRARIES =  libstlport_static libMcKernel libMcCommon

LOCAL_CPPFLAGS += -fno-rtti -fno-exceptions
include $(COMP_PATH_Logwrapper)/Android.mk

include $(BUILD_SHARED_LIBRARY)
