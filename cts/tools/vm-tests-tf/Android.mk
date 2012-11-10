# Copyright (C) 2011 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

define all-src-files
$(patsubst ./%,%,$(shell cd $(LOCAL_PATH) && find src -name '*.java'))
endef

# buildutil java library
# ============================================================
include $(CLEAR_VARS)

LOCAL_TEST_TYPE := vmHostTest
LOCAL_JAR_PATH := android.core.vm-tests-tf.jar

LOCAL_SRC_FILES := $(call all-src-files)

LOCAL_MODULE := cts-tf-dalvik-buildutil
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE_TAGS := optional

LOCAL_JAVA_LIBRARIES := dx dasm cfassembler junit
LOCAL_CLASSPATH := $(HOST_JDK_TOOLS_JAR)

include $(BUILD_HOST_JAVA_LIBRARY)

$(LOCAL_BUILT_MODULE): PRIVATE_CLASS_INTERMEDIATES_DIR := $(intermediates)/classes

TF_BUILD_UTIL_INTERMEDIATES_CLASSES := $(intermediates)/classes

include $(CLEAR_VARS)

LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := vm-tests-tf
LOCAL_MODULE_TAGS := optional

include $(BUILD_SYSTEM)/base_rules.mk

GENERATED_FILES:=$(intermediates)/tests

$(LOCAL_BUILT_MODULE): $(GENERATED_FILES)

colon:= :
empty:=
space:= $(empty) $(empty)


$(GENERATED_FILES): PRIVATE_SRC_FOLDER := $(LOCAL_PATH)/src
$(GENERATED_FILES): PRIVATE_LIB_FOLDER := $(LOCAL_PATH)/lib
$(GENERATED_FILES): PRIVATE_INTERMEDIATES := $(intermediates)/tests
$(GENERATED_FILES): PRIVATE_INTERMEDIATES_MAIN_FILES := $(intermediates)/main_files
$(GENERATED_FILES): PRIVATE_INTERMEDIATES_HOSTJUNIT_FILES := $(intermediates)/hostjunit_files
$(GENERATED_FILES): $(HOST_OUT_JAVA_LIBRARIES)/cts-tf-dalvik-buildutil.jar $(HOST_OUT_JAVA_LIBRARIES)/dasm.jar $(HOST_OUT_JAVA_LIBRARIES)/dx.jar $(HOST_OUT_JAVA_LIBRARIES)/cfassembler.jar $(HOST_OUT_JAVA_LIBRARIES)/junit.jar

	$(hide) mkdir -p $@
	$(hide) mkdir -p $(PRIVATE_INTERMEDIATES_HOSTJUNIT_FILES)/dot/junit
# generated and compile the host side junit tests
	$(hide) java -cp $(subst $(space),$(colon),$^):$(HOST_JDK_TOOLS_JAR) util.build.BuildDalvikSuite $(PRIVATE_SRC_FOLDER) $(PRIVATE_INTERMEDIATES) $<:$(PRIVATE_LIB_FOLDER)/junit.jar:$(HOST_OUT_JAVA_LIBRARIES)/tradefed-prebuilt.jar $(PRIVATE_INTERMEDIATES_MAIN_FILES) $(TF_BUILD_UTIL_INTERMEDIATES_CLASSES) $(PRIVATE_INTERMEDIATES_HOSTJUNIT_FILES) $$RUN_VM_TESTS_RTO
	@echo "wrote generated Main_*.java files to $(PRIVATE_INTERMEDIATES_MAIN_FILES)"
INSTALLED_TESTS := $(dir $(LOCAL_INSTALLED_MODULE))../cts_dalviktests_tf/timestamp

$(LOCAL_BUILT_MODULE):  $(INSTALLED_TESTS)

$(INSTALLED_TESTS): PRIVATE_INTERMEDIATES := $(intermediates)/tests
$(INSTALLED_TESTS): $(GENERATED_FILES) $(GENERATED_FILES)/dot/junit/dexcore.jar
	$(hide) mkdir -p $(dir $@)tests
	$(hide) $(ACP) -r $(PRIVATE_INTERMEDIATES)/dot $(dir $@)tests
	@touch $@

$(intermediates)/android.core.vm-tests-tf.jar: PRIVATE_INTERMEDIATES := $(intermediates)
$(intermediates)/android.core.vm-tests-tf.jar: $(INSTALLED_TESTS)
	$(hide) cd $(PRIVATE_INTERMEDIATES)/hostjunit_files/classes && \
	zip -q -r ../../android.core.vm-tests-tf.jar . && \
	cd -
	$(hide) cd $(PRIVATE_INTERMEDIATES) && \
	zip -q -r android.core.vm-tests-tf.jar tests && \
	cd -
	

define get-class-path
	$(TF_BUILD_UTIL_INTERMEDIATES_CLASSES)/$(strip $(1))
endef

define dex-classes
	@mkdir -p $(dir $@)
	@jar -cf $(dir $@)/$(notdir $@).jar $(addprefix -C $(1) ,$(2))
	$(hide) $(DX) -JXms16M -JXmx768M \
    --dex --output=$@ \
    $(if $(NO_OPTIMIZE_DX), \
        --no-optimize) \
    $(dir $@)/$(notdir $@).jar
    @rm -f $(dir $@)/$(notdir $@).jar
endef

$(call get-class-path,dot/junit/DxUtil.class) $(call get-class-path,dot/junit/DxAbstractMain.class):  $(HOST_OUT_JAVA_LIBRARIES)/cts-tf-dalvik-buildutil.jar $(DX)

$(GENERATED_FILES)/dot/junit/dexcore.jar: $(call get-class-path,dot/junit/DxUtil.class) $(call get-class-path,dot/junit/DxAbstractMain.class)
	$(call dex-classes,$(TF_BUILD_UTIL_INTERMEDIATES_CLASSES),dot/junit/DxUtil.class dot/junit/DxAbstractMain.class)
