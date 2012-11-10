ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= powertop
LOCAL_CFLAGS += -DNO_NCURSES
LOCAL_CFLAGS += -DNO_SUGGESTIONS
LOCAL_CFLAGS += -DPATH_MAX=256
ifeq ($(BOARD_USES_QCOM_HARDWARE),true)
	LOCAL_CFLAGS += -DPLATFORM_NO_INT0
endif
LOCAL_SRC_FILES := powertop.c \
		   config.c \
		   process.c \
		   misctips.c \
		   bluetooth.c \
		   display.c \
		   suggestions.c \
		   cpufreq.c \
		   cpufreqstats.c \
		   sata.c \
		   xrandr.c \
		   intelcstates.c \
		   usb.c \
		   urbnum.c \
		   msmpmstats.c

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)/
include $(BUILD_EXECUTABLE)

endif
