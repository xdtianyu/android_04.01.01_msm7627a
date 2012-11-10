# =============================================================================
#
# Module: libPaApi(Static and Shared variant)
#
# =============================================================================

LOCAL_PATH	:= $(call my-dir)

#Now the Shared Object
include $(CLEAR_VARS)

LOCAL_MODULE	:= libPaApi
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_C_INCLUDES += bionic \
    external/stlport/stlport

# Add your folders with header files here (absolute paths)
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/Public \
	$(COMP_PATH_MobiCore)/inc \
	$(COMP_PATH_MobiCore)/inc/TlCm \
	$(APP_PROJECT_PATH)/ClientLib/public

# Add your source files here (relative paths)
LOCAL_SRC_FILES	+= tlcCmApi.cpp

LOCAL_SHARED_LIBRARIES	+= libMcRegistry libMcClient
LOCAL_STATIC_LIBRARIES =  libstlport_static
LOCAL_CPPFLAGS += -fno-rtti -fno-exceptions

include $(COMP_PATH_Logwrapper)/Android.mk

include $(BUILD_SHARED_LIBRARY)
