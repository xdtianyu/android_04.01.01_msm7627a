# =============================================================================
#
# Module: libMcRegistry.a - MobiCore driver registry
#
# =============================================================================

LOCAL_PATH	:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE	:= libMcRegistry

# Prevent not-yet-used functions of being ignored by linker
LOCAL_LDLIBS	:= -Wl,-whole-archive

LOCAL_C_INCLUDES += bionic \
    external/stlport/stlport

# Add new folders with header files here
LOCAL_C_INCLUDES	+=\
	$(LOCAL_PATH)/Public\
	$(MY_CLIENTLIB_PATH)/public\
	$(COMP_PATH_MobiCore)/inc

# Add new source files here
LOCAL_SRC_FILES		+= Registry.cpp

# Header files for components including this module
LOCAL_EXPORT_C_INCLUDES	+=\
	$(LOCAL_PATH)/Public\
	$(MY_CLIENTLIB_PATH)/public

LOCAL_CPPFLAGS += -fno-rtti -fno-exceptions
include $(COMP_PATH_Logwrapper)/Android.mk

include $(BUILD_STATIC_LIBRARY)

##################################################
## Shared Object
##################################################
include $(CLEAR_VARS)

LOCAL_MODULE	:= libMcRegistry
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

# Prevent not-yet-used functions of being ignored by linker
LOCAL_LDLIBS	:= -Wl,-whole-archive

LOCAL_C_INCLUDES += bionic \
    external/stlport/stlport

# Add new folders with header files here
LOCAL_C_INCLUDES	+=\
	$(LOCAL_PATH)/Public\
	$(MY_CLIENTLIB_PATH)/public\
	$(COMP_PATH_MobiCore)/inc

# Add new source files here
LOCAL_SRC_FILES		+= Registry.cpp

# Header files for components including this module
LOCAL_EXPORT_C_INCLUDES	+=\
	$(LOCAL_PATH)/Public\
	$(MY_CLIENTLIB_PATH)/public

LOCAL_STATIC_LIBRARIES = libstlport_static
LOCAL_CPPFLAGS += -fno-rtti -fno-exceptions

include $(COMP_PATH_Logwrapper)/Android.mk

include $(BUILD_SHARED_LIBRARY)
