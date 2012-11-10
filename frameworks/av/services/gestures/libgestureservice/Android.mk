LOCAL_PATH:= $(call my-dir)

#
# libgestureservice
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    GestureDeviceService.cpp

LOCAL_SHARED_LIBRARIES:= \
    libui \
    libutils \
    libbinder \
    libcutils \
    libmedia \
    libgesture_client \
    libgui \
    libhardware

LOCAL_CFLAGS += -DLOGE=ALOGE
LOCAL_CFLAGS += -DLOGV=ALOGV
LOCAL_CFLAGS += -DLOGW=ALOGW
LOCAL_CFLAGS += -DLOGD=ALOGD
LOCAL_CFLAGS += -DLOGI=ALOGI
LOCAL_CFLAGS += -DLOGE_IF=ALOGE_IF
LOCAL_CFLAGS += -DLOGV_IF=ALOGV_IF
LOCAL_CFLAGS += -DLOGW_IF=ALOGW_IF
LOCAL_CFLAGS += -DLOGD_IF=ALOGD_IF

LOCAL_MODULE:= libgestureservice
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
