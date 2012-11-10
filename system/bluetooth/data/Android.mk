ifneq ($(call is-board-platform,msm8960),true)
LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

dest_dir := $(TARGET_OUT)/etc/bluetooth

#These files are a part of GRANDFATHERED_ALL_PREBUILT in
#build/core/legacy_prebuilts.mk
files := \
        audio.conf \
        main.conf \
        blacklist.conf \
        auto_pairing.conf

copy_to := $(addprefix $(dest_dir)/,$(files))

$(copy_to): PRIVATE_MODULE := bluetooth_etcdir
$(copy_to): $(dest_dir)/%: $(LOCAL_PATH)/% | $(ACP)
        $(transform-prebuilt-to-target)

ALL_PREBUILT += $(copy_to)
endif
