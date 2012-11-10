# =============================================================================
#
# MC driver device files
#
# =============================================================================

# This is not a separate module.
# Only for inclusion by other modules.

MY_MCDRV_DEVICE_PATH	:= $(call my-dir)
MY_MCDRV_DEVICE_PATH_REL	:= Device

include $(MY_MCDRV_DEVICE_PATH)/Platforms/Android.mk

# Add new folders with header files here
LOCAL_C_INCLUDES +=\
	$(MY_MCDRV_DEVICE_PATH)\
	$(MY_MCDRV_DEVICE_PATH)/public

# Add new source files here
LOCAL_SRC_FILES +=\
	$(MY_MCDRV_DEVICE_PATH_REL)/DeviceIrqHandler.cpp\
	$(MY_MCDRV_DEVICE_PATH_REL)/DeviceScheduler.cpp\
	$(MY_MCDRV_DEVICE_PATH_REL)/MobiCoreDevice.cpp\
	$(MY_MCDRV_DEVICE_PATH_REL)/NotificationQueue.cpp\
	$(MY_MCDRV_DEVICE_PATH_REL)/TrustletSession.cpp\
