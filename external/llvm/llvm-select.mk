define get_llvm_root_path
$(if $(filter true,$(BOARD_USE_QCOM_LLVM_CLANG_RS)),vendor/qcom/proprietary/llvm-rs,external/llvm)
endef

define get_clang_root_path
$(if $(filter true,$(BOARD_USE_QCOM_LLVM_CLANG_RS)),vendor/qcom/proprietary/clang-rs,external/clang)
endef

define get_clang
$(HOST_OUT_EXECUTABLES)/$(if $(filter true,$(BOARD_USE_QCOM_LLVM_CLANG_RS)),RS,)clang$(HOST_EXECUTABLE_SUFFIX)
endef

define get_clang_cxx
$(HOST_OUT_EXECUTABLES)/$(if $(filter true,$(BOARD_USE_QCOM_LLVM_CLANG_RS)),RS,)clang++$(HOST_EXECUTABLE_SUFFIX)
endef

define get_llvm_as
$(HOST_OUT_EXECUTABLES)/$(if $(filter true,$(BOARD_USE_QCOM_LLVM_CLANG_RS)),RS,)llvm-as$(HOST_EXECUTABLE_SUFFIX)
endef

define get_llvm_link
$(HOST_OUT_EXECUTABLES)/$(if $(filter true,$(BOARD_USE_QCOM_LLVM_CLANG_RS)),RS,)llvm-link$(HOST_EXECUTABLE_SUFFIX)
endef

define set_llvm_targets
CLANG := $(call get_clang)
CLANG_CXX := $(call get_clang_cxx)
LLVM_AS := $(call get_llvm_as)
LLVM_LINK := $(call get_llvm_link)
endef
