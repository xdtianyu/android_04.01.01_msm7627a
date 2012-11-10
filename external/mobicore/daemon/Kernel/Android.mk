# =============================================================================
#
# Module: libKernel.a - Kernel module access classes
#
# =============================================================================

LOCAL_PATH	:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE	:= libMcKernel

# Include platform specific sub-makefiles
ifdef PLATFORM
  include $(LOCAL_PATH)/Platforms/Generic/Android.mk
  include $(LOCAL_PATH)/Platforms/$(PLATFORM)/Android.mk
else 
  include $(LOCAL_PATH)/Platforms/Generic/Android.mk
endif

# Add new folders with header files here
LOCAL_C_INCLUDES +=\
	$(COMP_PATH_MobiCoreDriverMod)/Public \
	$(APP_PROJECT_PATH)/Common \
	$(LOCAL_PATH) 

# Add new source files here
LOCAL_SRC_FILES +=\
	CKMod.cpp

# Header files for components including this module
LOCAL_EXPORT_C_INCLUDES	+=\
	$(LOCAL_PATH)

LOCAL_CPPFLAGS += -fno-rtti -fno-exceptions

include $(COMP_PATH_Logwrapper)/Android.mk

include $(BUILD_STATIC_LIBRARY)
