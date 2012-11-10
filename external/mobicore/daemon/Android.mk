# =============================================================================
#
# Makefile pointing to all makefiles within the project.
#
# =============================================================================
APP_PROJECT_PATH := $(call my-dir)
# Including all Android.mk files from subdirectories
include $(call all-subdir-makefiles)
