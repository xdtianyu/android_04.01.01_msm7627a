LOCAL_PATH:= $(call my-dir)

#
# libplugin
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	mgmtops.c \
	dbusoob.c \
	gatt-example.c \
	service.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.93\" \
	-DBLUETOOTH_PLUGIN_BUILTIN \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\"

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../btio \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../gdbus \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../btio \
	$(LOCAL_PATH)/../attrib \
	$(call include-path-for, glib) \
	$(call include-path-for, dbus) \

LOCAL_SHARED_LIBRARIES := \
	libbluetoothd \
	libbluetooth \
	libcutils \
	libdbus \
	libglib \

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE:=libbuiltinplugin

include $(BUILD_STATIC_LIBRARY)
