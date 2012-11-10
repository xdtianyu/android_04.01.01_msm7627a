# =============================================================================
#
# Generic TrustZone device includes
#
# =============================================================================

# This is not a separate module.
# Only for inclusion by other modules.

GENERIC_PATH		:= $(call my-dir)
GENERIC_PATH_REL	:= Device/Platforms/Generic

# Add new source files here
LOCAL_SRC_FILES +=$(GENERIC_PATH_REL)/TrustZoneDevice.cpp

# Header files for components including this module
LOCAL_C_INCLUDES += $(call my-dir)
