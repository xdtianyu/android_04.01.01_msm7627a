LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(BOARD_USES_ALSA_AUDIO),true)
    ifeq ($(call is-chipset-in-board-platform,msm8960),true)
        LOCAL_CFLAGS += -DUSE_TUNNEL_MODE
    endif
endif

include frameworks/av/media/libstagefright/codecs/common/Config.mk

LOCAL_SRC_FILES:=                         \
        ACodec.cpp                        \
        AACExtractor.cpp                  \
        AACWriter.cpp                     \
        AMRExtractor.cpp                  \
        AMRWriter.cpp                     \
        AudioPlayer.cpp                   \
        AudioSource.cpp                   \
        AwesomePlayer.cpp                 \
        CameraSource.cpp                  \
        CameraSourceTimeLapse.cpp         \
        DataSource.cpp                    \
        DRMExtractor.cpp                  \
        ESDS.cpp                          \
        FileSource.cpp                    \
        FLACExtractor.cpp                 \
        HTTPBase.cpp                      \
        JPEGSource.cpp                    \
        MP3Extractor.cpp                  \
        MPEG2TSWriter.cpp                 \
        MPEG4Extractor.cpp                \
        MPEG4Writer.cpp                   \
        MediaBuffer.cpp                   \
        MediaBufferGroup.cpp              \
        MediaCodec.cpp                    \
        MediaCodecList.cpp                \
        MediaDefs.cpp                     \
        MediaExtractor.cpp                \
        MediaSource.cpp                   \
        MetaData.cpp                      \
        NuCachedSource2.cpp               \
        NuMediaExtractor.cpp              \
        OMXClient.cpp                     \
        OMXCodec.cpp                      \
        OggExtractor.cpp                  \
        SampleIterator.cpp                \
        SampleTable.cpp                   \
        SkipCutBuffer.cpp                 \
        StagefrightMediaScanner.cpp       \
        StagefrightMetadataRetriever.cpp  \
        SurfaceMediaSource.cpp            \
        ThrottledSource.cpp               \
        TimeSource.cpp                    \
        TimedEventQueue.cpp               \
        TunnelPlayer.cpp                  \
        Utils.cpp                         \
        VBRISeeker.cpp                    \
        WAVExtractor.cpp                  \
        WAVEWriter.cpp                    \
        WVMExtractor.cpp                  \
        XINGSeeker.cpp                    \
        avc_utils.cpp                     \
        ExtendedExtractor.cpp             \
        ExtendedWriter.cpp                \
        FMA2DPWriter.cpp                  \



ifneq ($(call is-vendor-board-platform,QCOM),true)
LOCAL_CFLAGS += -DNON_QCOM_TARGET
endif

ifeq ($(call is-vendor-board-platform,QCOM),true)
    ifeq ($(call is-board-platform-in-list,msm8660 msm7627a msm7630),true)
        LOCAL_SRC_FILES += LPAPlayer.cpp
    else
        LOCAL_SRC_FILES += LPAPlayerALSA.cpp
    endif
endif


LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/include/media/stagefright/timedtext \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/external/expat/lib \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
        $(TOP)/external/openssl/include \
        $(TOP)/hardware/qcom/display/libgralloc \
	$(TOP)/hardware/qcom/media/mm-core/inc \
	$(TOP)/system/core/include \
	$(TOP)/frameworks/av/media/libmediaplayerservice \
	$(TOP)/frameworks/native/include/binder


LOCAL_SHARED_LIBRARIES := \
        libbinder \
        libcamera_client \
        libcrypto \
        libcutils \
        libdl \
        libdrmframework \
        libexpat \
        libgui \
        libicui18n \
        libicuuc \
        liblog \
        libmedia \
        libmedia_native \
        libsonivox \
        libssl \
        libstagefright_omx \
        libstagefright_yuv \
        libui \
        libutils \
        libvorbisidec \
        libz \

LOCAL_STATIC_LIBRARIES := \
        libstagefright_color_conversion \
        libstagefright_mp3dec \
        libstagefright_aacenc \
        libstagefright_matroska \
        libstagefright_timedtext \
        libvpx \
        libstagefright_mpeg2ts \
        libstagefright_httplive \
        libstagefright_id3 \
        libFLAC \

ifneq ($(TARGET_BUILD_PDK), true)
LOCAL_STATIC_LIBRARIES += \
	libstagefright_chromium_http
LOCAL_SHARED_LIBRARIES += \
        libchromium_net
LOCAL_CPPFLAGS += -DCHROMIUM_AVAILABLE=1
endif

LOCAL_SHARED_LIBRARIES += libstlport
include external/stlport/libstlport.mk

LOCAL_SHARED_LIBRARIES += \
        libstagefright_enc_common \
        libstagefright_avc_common \
        libstagefright_foundation \
        libdl

LOCAL_CFLAGS += -Wno-multichar
LOCAL_CFLAGS += -Werror

LOCAL_MODULE:= libstagefright

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
