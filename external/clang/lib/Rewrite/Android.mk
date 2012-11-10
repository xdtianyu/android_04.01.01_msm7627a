LOCAL_PATH:= $(call my-dir)

# For the host only
# =====================================================
include $(CLEAR_VARS)
include $(CLEAR_TBLGEN_VARS)

LOCAL_MODULE:= libclangRewrite

LOCAL_MODULE_TAGS := optional

TBLGEN_TABLES := \
  AttrList.inc \
  Attrs.inc \
  AttrParsedAttrList.inc \
  DiagnosticCommonKinds.inc \
  DiagnosticFrontendKinds.inc \
  DeclNodes.inc \
  StmtNodes.inc

clang_rewrite_SRC_FILES := \
  DeltaTree.cpp \
  FixItRewriter.cpp \
  FrontendActions.cpp \
  HTMLPrint.cpp \
  HTMLRewrite.cpp \
  RewriteMacros.cpp \
  RewriteModernObjC.cpp \
  RewriteObjC.cpp \
  RewriteRope.cpp \
  RewriteTest.cpp \
  Rewriter.cpp \
  TokenRewriter.cpp

LOCAL_SRC_FILES := $(clang_rewrite_SRC_FILES)


include $(CLANG_HOST_BUILD_MK)
include $(CLANG_TBLGEN_RULES_MK)
include $(BUILD_HOST_STATIC_LIBRARY)
