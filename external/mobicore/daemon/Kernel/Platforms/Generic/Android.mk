# =============================================================================
#
# Generic  TrustZone device includes
#
# =============================================================================

# This is not a separate module.
# Only for inclusion by other modules.

GENERIC_MODULE_PATH		:= $(call my-dir)
GENERIC_MODULE_PATH_REL	:= Platforms/Generic

# Add new source files here
LOCAL_SRC_FILES +=\
	$(GENERIC_MODULE_PATH_REL)/CMcKMod.cpp

# Add new folders with header files here
LOCAL_C_INCLUDES +=\
	$(GENERIC_MODULE_PATH)\
	$(COMP_PATH_MobiCore)/inc\
	$(COMP_PATH_MobiCoreDriverMod)/Public

# Header files for components including this module
LOCAL_EXPORT_C_INCLUDES	+=\
	$(GENERIC_MODULE_PATH)\
	$(COMP_PATH_MobiCoreDriverMod)/Public
	