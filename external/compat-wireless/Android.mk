LOCAL_PATH := $(call my-dir)
ifeq ($(call is-android-codename,JELLY_BEAN),true)
	DLKM_DIR := $(TOP)/device/qcom/common/dlkm
else
	DLKM_DIR := build/dlkm
endif

ifeq ($(BOARD_HAS_ATH_WLAN), true)

export BUILD_ATH6KL_VER_32=1
export HAVE_CFG80211=1

include $(CLEAR_VARS)
LOCAL_MODULE             := cfg80211.ko
LOCAL_MODULE_TAGS        := debug
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/ath6kl
include $(DLKM_DIR)/AndroidKernelModule.mk

include $(CLEAR_VARS)
LOCAL_MODULE             := ath6kl_sdio.ko
LOCAL_MODULE_KBUILD_NAME := wlan.ko
LOCAL_MODULE_TAGS        := debug
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/ath6kl
include $(DLKM_DIR)/AndroidKernelModule.mk
endif

ifeq ($(BOARD_HAS_ATH_ETH_ALX), true)

export BUILD_ATH_ETH_ALX=1

include $(CLEAR_VARS)
LOCAL_MODULE             := compat.ko
LOCAL_MODULE_TAGS        := debug
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/compat
include $(DLKM_DIR)/AndroidKernelModule.mk

include $(CLEAR_VARS)
LOCAL_MODULE             := alx.ko
LOCAL_MODULE_KBUILD_NAME := eth.ko
LOCAL_MODULE_TAGS        := debug
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/alx
include $(DLKM_DIR)/AndroidKernelModule.mk

endif

ifeq ($(BOARD_HAS_ATH_WLAN_AR6004), true)

export BUILD_ATH6KL_VER_35=1
export HAVE_CFG80211=1

include $(CLEAR_VARS)
LOCAL_MODULE             := cfg80211.ko
LOCAL_MODULE_TAGS        := debug
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/ath6kl-3.5
include $(DLKM_DIR)/AndroidKernelModule.mk

include $(CLEAR_VARS)
LOCAL_MODULE             := ath6kl_usb.ko
LOCAL_MODULE_TAGS        := debug
LOCAL_MODULE_KBUILD_NAME := wlan.ko
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/ath6kl-3.5
include $(DLKM_DIR)/AndroidKernelModule.mk

include $(LOCAL_PATH)/drivers/net/wireless/ath/ath6kl-3.5/android_sdio/Android.mk

endif
