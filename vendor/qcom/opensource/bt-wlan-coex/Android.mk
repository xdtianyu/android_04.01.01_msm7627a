# Build the Coex solution for BlueZ + Libra: BT Coex Shim, BTC-ES
# But only for builds with the Libra WLAN

ifeq ($(BOARD_HAVE_BLUETOOTH), true)
ifeq ($(BOARD_HAS_QCOM_WLAN), true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(call is-android-codename-in-list,ECLAIR FROYO),true)
# BlueZ Includes
BLUEZ_ROOT := $(LOCAL_PATH)/../../../../external/bluetooth/bluez/include/bluetooth
else
# BlueZ Includes
BLUEZ_ROOT := $(LOCAL_PATH)/../../../../external/bluetooth/bluez/lib/bluetooth
endif #froyo and older

ifeq ($(call is-android-codename-in-list,ECLAIR FROYO GINGERBREAD),true)
LOCAL_CFLAGS += -DDBUS_OLD_APIS
endif # gingerbread and older

# This is used by BT Coex Shim and BTC-ES
ifeq ($(TARGET_BUILD_TYPE),debug)
LOCAL_CFLAGS += -DBTC_DEBUG
LOCAL_CFLAGS += -DBTCES_DEBUG
endif

# BTC make does this, ensure compatibility
LOCAL_CFLAGS += \
        -fno-short-enums

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/btc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/btces
LOCAL_C_INCLUDES += $(BLUEZ_ROOT)
LOCAL_C_INCLUDES += $(call include-path-for, dbus)

ifeq ($(call is-android-codename-in-list,ECLAIR FROYO),true)
LOCAL_STATIC_LIBRARIES := \
	libbluez-common-static

LOCAL_SHARED_LIBRARIES := \
        libdbus \
        libbluetooth

else
LOCAL_SHARED_LIBRARIES := \
	libdbus     \
	libbluetooth \
	libcutils
endif #froyo and older

#Enabling warnings to be treated as errors.
LOCAL_CFLAGS += -Werror -Wall -Wextra

LOCAL_SRC_FILES := \
        bt_coex_shim.cpp \
        btc/wlan_btc_usr_svc.c \
        btces/btces_api.cpp \
        btces/btces_pfal.cpp

# This library is for BT Coex using BlueZ stack with Qualcomm (Libra) WLAN
LOCAL_MODULE := btwlancoex
LOCAL_MODULE_TAGS := optional debug

LOCAL_PRELINK_MODULE := false
include $(BUILD_EXECUTABLE)

endif # BOARD_HAS_QCOM_WLAN
endif # BOARD_HAVE_BLUETOOTH
