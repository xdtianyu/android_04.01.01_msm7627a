# =============================================================================
#
# MC driver server files
#
# =============================================================================

# This is not a separate module.
# Only for inclusion by other modules.

MY_MCDRV_SERVER_PATH := $(call my-dir)
MY_MCDRV_SERVER_PATH_REL := Server

# Add new folders with header files here
LOCAL_C_INCLUDES += $(MY_MCDRV_SERVER_PATH)/public

# Add new source files here
LOCAL_SRC_FILES += $(MY_MCDRV_SERVER_PATH_REL)/Server.cpp \
		$(MY_MCDRV_SERVER_PATH_REL)/NetlinkServer.cpp
