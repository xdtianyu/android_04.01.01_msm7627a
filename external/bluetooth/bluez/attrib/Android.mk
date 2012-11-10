LOCAL_PATH:= $(call my-dir)

# Attrib plugin

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	att.c \
	client.c \
	gatt.c \
	gattrib.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.93\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DCONFIGDIR=\"/etc/bluetooth\" \
	-DANDROID \

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../gdbus \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../btio \
	$(call include-path-for, glib) \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libbluetoothd \
	libdbus \
	libglib


LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE:=libattrib_static
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

#
# gatttool
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	utils.c \
	interactive.c \
	gatttool.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.93\" -fpermissive

LOCAL_C_INCLUDES:=\
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../btio \
	$(call include-path-for, glib) \
	$(call include-path-for, glib)/glib

LOCAL_SHARED_LIBRARIES := \
	libbluetoothd \
	libbluetooth \
	libglib \
	libbtio

LOCAL_MODULE := gatttool
LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)
