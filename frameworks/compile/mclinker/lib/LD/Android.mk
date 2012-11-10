LOCAL_PATH:= $(call my-dir)

# =====================================================
# Static library: libmcldLD
# =====================================================

mcld_ld_SRC_FILES := \
  ArchiveReader.cpp \
  BranchIsland.cpp  \
  DynObjReader.cpp  \
  DynObjWriter.cpp  \
  ELFSegment.cpp  \
  ELFSegmentFactory.cpp \
  Layout.cpp  \
  LDContext.cpp \
  LDFileFormat.cpp  \
  LDReader.cpp  \
  LDSection.cpp \
  LDSectionFactory.cpp  \
  LDSymbol.cpp  \
  LDWriter.cpp  \
  ObjectWriter.cpp  \
  Relocation.cpp  \
  RelocationFactory.cpp \
  ResolveInfo.cpp \
  ResolveInfoFactory.cpp  \
  Resolver.cpp  \
  SectionMap.cpp  \
  SectionMerger.cpp \
  StaticResolver.cpp  \
  StrSymPool.cpp

# For the host
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(mcld_ld_SRC_FILES)
LOCAL_MODULE:= libmcldLD

LOCAL_MODULE_TAGS := optional

include $(MCLD_HOST_BUILD_MK)
include $(BUILD_HOST_STATIC_LIBRARY)

# For the device
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(mcld_ld_SRC_FILES)
LOCAL_MODULE:= libmcldLD

LOCAL_MODULE_TAGS := optional

include $(MCLD_DEVICE_BUILD_MK)
include $(BUILD_STATIC_LIBRARY)

# =====================================================
# Static library: libmcldLDVariant
# =====================================================

mcld_ld_variant_SRC_FILES := \
  BSDArchiveReader.cpp  \
  GNUArchiveReader.cpp  \
  ELFDynObjFileFormat.cpp \
  ELFDynObjReader.cpp \
  ELFDynObjWriter.cpp \
  ELFExecFileFormat.cpp \
  ELFFileFormat.cpp \
  ELFObjectReader.cpp \
  ELFObjectWriter.cpp \
  ELFReader.cpp \
  ELFWriter.cpp

# For the host
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(mcld_ld_variant_SRC_FILES)
LOCAL_MODULE:= libmcldLDVariant

LOCAL_MODULE_TAGS := optional

include $(MCLD_HOST_BUILD_MK)
include $(BUILD_HOST_STATIC_LIBRARY)

# For the device
# =====================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(mcld_ld_variant_SRC_FILES)
LOCAL_MODULE:= libmcldLDVariant

LOCAL_MODULE_TAGS := optional

include $(MCLD_DEVICE_BUILD_MK)
include $(BUILD_STATIC_LIBRARY)
