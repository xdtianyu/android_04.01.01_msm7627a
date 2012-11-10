#
# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

ifeq (darwin,$(BUILD_OS))
else

LOCAL_PATH := $(call my-dir)

# Executable for host
# ========================================================
include $(CLEAR_VARS)

LOCAL_MODULE := bcc

LOCAL_SRC_FILES := \
  main.cpp

LOCAL_SHARED_LIBRARIES := \
  libbcc

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../include

LOCAL_MODULE_TAGS := tests eng

LOCAL_LDLIBS = -ldl

LOCAL_CFLAGS += -D__HOST__ -Wall -Werror

include $(BUILD_HOST_EXECUTABLE)

# Executable for target
# ========================================================
include $(CLEAR_VARS)

LOCAL_MODULE := bcc

LOCAL_SRC_FILES := \
  main.cpp

LOCAL_SHARED_LIBRARIES := libdl libstlport libbcinfo libbcc

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../include

LOCAL_MODULE_TAGS := optional

include external/stlport/libstlport.mk
include $(BUILD_EXECUTABLE)

endif
