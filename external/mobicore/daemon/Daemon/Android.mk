# =============================================================================
#
# Module: mcDriverDaemon
#
# =============================================================================
LOCAL_PATH := $(call my-dir)
MY_MCDRIVER_PATH := $(LOCAL_PATH)

include $(CLEAR_VARS)

LOCAL_MODULE	:= mcDriverDaemon
LOCAL_MODULE_TAGS := optional

# Add new subdirectories containing code here
include $(LOCAL_PATH)/Device/Android.mk
include $(LOCAL_PATH)/Server/Android.mk

LOCAL_C_INCLUDES += bionic \
	external/stlport/stlport

# Add new folders with header files here
LOCAL_C_INCLUDES += \
	$(COMP_PATH_MobiCore)/inc \
	$(COMP_PATH_MobiCoreDriverMod)/Public \
	$(APP_PROJECT_PATH)/ClientLib/public \
	$(APP_PROJECT_PATH)/Kernel \
	$(APP_PROJECT_PATH)/Kernel/Platforms/Generic \
	$(APP_PROJECT_PATH)/Common \
	$(APP_PROJECT_PATH)/Registry/Public \
	$(MY_MCDRIVER_PATH)/public

# Add new source files here
LOCAL_SRC_FILES += \
	MobiCoreDriverDaemon.cpp

LOCAL_CPPFLAGS += -fno-rtti -fno-exceptions
# Modules this one depnds on (depending ones first)
LOCAL_STATIC_LIBRARIES = libstlport_static libMcKernel libMcCommon libMcRegistry

include $(COMP_PATH_Logwrapper)/Android.mk

include $(BUILD_EXECUTABLE)

