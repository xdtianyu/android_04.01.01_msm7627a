LOCAL_PATH := $(call my-dir)

ifeq ($(call is-android-codename,JELLY_BEAN),true)
  DLKM_DIR := $(TOP)/device/qcom/common/dlkm
else
  DLKM_DIR := build/dlkm
endif

# cfg80211_sdio.ko will only use at build time, 
# NOT include in the output image.
# Therefore keep this LOCAL_MODULE_TAGS as optional
# ath6kl_sdio modules will use the cfg80211.ko on top level

include $(CLEAR_VARS)
LOCAL_MODULE             := cfg80211_sdio.ko
LOCAL_MODULE_TAGS        := optional
LOCAL_MODULE_KBUILD_NAME := cfg80211.ko
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/ath6kl-3.5
include $(DLKM_DIR)/AndroidKernelModule.mk

include $(CLEAR_VARS)
LOCAL_MODULE             := ath6kl_sdio.ko
LOCAL_MODULE_TAGS        := debug
LOCAL_MODULE_KBUILD_NAME := wlan.ko
LOCAL_MODULE_PATH        := $(TARGET_OUT)/lib/modules/ath6kl-3.5
include $(DLKM_DIR)/AndroidKernelModule.mk
