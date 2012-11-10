ifeq ($(BOARD_HAVE_QCOM_FM),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src/com/quicinc/fmradio) \
	src/com/quicinc/fmradio/IFMRadioServiceCallbacks.aidl \
	src/com/quicinc/fmradio/IFMRadioService.aidl \
	src/com/quicinc/fmradio/IFMTransmitterServiceCallbacks.aidl \
	src/com/quicinc/fmradio/IFMTransmitterService.aidl \


ifeq ($(call is-android-codename-in-list,JELLY_BEAN ICECREAM_SANDWICH HONEYCOMB),true)
LOCAL_SRC_FILES +=  $(call all-java-files-under, src/com/quicinc/hc_utils)
else
LOCAL_SRC_FILES +=  $(call all-java-files-under, src/com/quicinc/utils)
endif
LOCAL_PACKAGE_NAME := FM
LOCAL_CERTIFICATE := platform

include $(BUILD_PACKAGE)

endif
