# Build Rules for Extracting Configuration from Android.mk
intermediates := $(local-intermediates-dir)

GEN := $(intermediates)/ConfigFromMk.h

$(GEN): PRIVATE_PATH := $(LIBBCC_ROOT_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = \
        $(PRIVATE_PATH)/tools/build/gen-config-from-mk.py < $< > $@
$(GEN): $(LIBBCC_ROOT_PATH)/libbcc-config.mk \
        $(LIBBCC_ROOT_PATH)/tools/build/gen-config-from-mk.py
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES += $(GEN)
