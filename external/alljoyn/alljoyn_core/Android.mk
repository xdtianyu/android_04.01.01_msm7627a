LOCAL_PATH := $(call my-dir)

# Rules to build liballjoyn.a

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SDK_VERSION := 8
LOCAL_NDK_VERSION := 6
LOCAL_NDK_STL_VARIANT := gnustl_static
#LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

LOCAL_CFLAGS += \
	-DQCC_CPU_ARM \
	-DQCC_OS_ANDROID \
	-DQCC_OS_GROUP_POSIX \
	-D_GLIBCXX_PERMIT_BACKWARD_HASH

alljoyn_ndk_source_root := $(HISTORICAL_NDK_VERSIONS_ROOT)/android-ndk-r$(LOCAL_NDK_VERSION)/sources
#alljoyn_ndk_source_root := $(HISTORICAL_NDK_VERSIONS_ROOT)/android-ndk-r5/sources

LOCAL_C_INCLUDES := \
	external/alljoyn/common/inc \
	external/alljoyn/alljoyn_core/inc \
	external/alljoyn/alljoyn_core/src \
	external/alljoyn/alljoyn_core/autogen \
	external/openssl/include \
	$(alljoyn_ndk_source_root)/cxx-stl/gnu-libstdc++/libs/$(TARGET_CPU_ABI)/include \
	$(alljoyn_ndk_source_root)/cxx-stl/gnu-libstdc++/include

LOCAL_SRC_FILES := \
	../common/src/ASN1.cc \
	../common/src/BigNum.cc \
	../common/src/BufferedSink.cc \
	../common/src/BufferedSource.cc \
	../common/src/Config.cc \
	../common/src/Crypto.cc \
	../common/src/CryptoSRP.cc \
	../common/src/Debug.cc \
	../common/src/GUID.cc \
	../common/src/IPAddress.cc \
	../common/src/KeyBlob.cc \
	../common/src/LockTrace.cc \
	../common/src/Logger.cc \
	../common/src/Pipe.cc \
	../common/src/ScatterGatherList.cc \
	../common/src/SocketStream.cc \
	../common/src/Stream.cc \
	../common/src/StreamPump.cc \
	../common/src/String.cc \
	../common/src/StringSource.cc \
	../common/src/StringUtil.cc \
	../common/src/ThreadPool.cc \
	../common/src/Timer.cc \
	../common/src/Util.cc \
	../common/src/XmlElement.cc \
	../common/os/posix/AdapterUtil.cc \
	../common/os/posix/Environ.cc \
	../common/os/posix/Event.cc \
	../common/os/posix/FileStream.cc \
	../common/os/posix/IfConfigLinux.cc \
	../common/os/posix/Mutex.cc \
	../common/os/posix/Socket.cc \
	../common/os/posix/SslSocket.cc \
	../common/os/posix/Thread.cc \
	../common/os/posix/atomic.cc \
	../common/os/posix/osUtil.cc \
	../common/os/posix/time.cc \
	../common/crypto/openssl/CryptoAES.cc \
	../common/crypto/openssl/CryptoHash.cc \
	../common/crypto/openssl/CryptoRSA.cc \
	../common/crypto/openssl/CryptoRand.cc

LOCAL_SRC_FILES += \
	src/AllJoynCrypto.cc \
	src/AllJoynPeerObj.cc \
	src/AllJoynStd.cc \
	src/AuthMechLogon.cc \
	src/AuthMechRSA.cc \
	src/AuthMechSRP.cc \
	src/BusAttachment.cc \
	src/BusEndpoint.cc \
	src/BusObject.cc \
	src/BusUtil.cc \
	src/ClientRouter.cc \
	src/ClientTransport.cc \
	src/CompressionRules.cc \
	src/DBusCookieSHA1.cc \
	src/DBusStd.cc \
	src/EndpointAuth.cc \
	src/InterfaceDescription.cc \
	src/KeyStore.cc \
	src/LocalTransport.cc \
	src/Message.cc \
	src/Message_Gen.cc \
	src/Message_Parse.cc \
	src/MethodTable.cc \
	src/MsgArg.cc \
	src/NullTransport.cc \
	src/PeerState.cc \
	src/ProxyBusObject.cc \
	src/RemoteEndpoint.cc \
	src/SASLEngine.cc \
	src/SessionOpts.cc \
	src/SignalTable.cc \
	src/SignatureUtils.cc \
	src/SimpleBusListener.cc \
	src/Transport.cc \
	src/TransportList.cc \
	src/XmlHelper.cc \
	src/posix/ClientTransport.cc \
	src/posix/android/PermissionDB.cc


LOCAL_SRC_FILES += \
	autogen/Status.c \
	autogen/version.cc

LOCAL_SHARED_LIBRARIES := \
	libcrypto \
	libssl \
	liblog

LOCAL_PRELINK_MODULE := false

LOCAL_REQUIRED_MODULES := \
	external/openssl/crypto/libcrypto \
	external/openssl/ssl/libssl

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := liballjoyn

include $(BUILD_SHARED_LIBRARY)

# Rules to build alljoyn-daemon

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

#LOCAL_SDK_VERSION := 8
#LOCAL_NDK_VERSION := 5

LOCAL_CFLAGS += \
	-DQCC_CPU_ARM \
	-DQCC_OS_ANDROID \
	-DQCC_OS_GROUP_POSIX \
	-D_GLIBCXX_PERMIT_BACKWARD_HASH

#alljoyn_ndk_source_root := $(HISTORICAL_NDK_VERSIONS_ROOT)/android-ndk-r$(LOCAL_NDK_VERSION)/sources
alljoyn_ndk_source_root := $(HISTORICAL_NDK_VERSIONS_ROOT)/android-ndk-r7/sources

LOCAL_C_INCLUDES := \
	external/alljoyn/alljoyn_core/autogen \
	external/alljoyn/alljoyn_core/daemon \
	external/alljoyn/alljoyn_core/daemon/ice \
	external/alljoyn/alljoyn_core/daemon/posix \
	external/alljoyn/alljoyn_core/inc \
	external/alljoyn/alljoyn_core/JSON \
	external/alljoyn/alljoyn_core/src \
	external/alljoyn/common/inc \
	external/openssl/include \
	$(alljoyn_ndk_source_root)/cxx-stl/gnu-libstdc++/libs/$(TARGET_CPU_ABI)/include \
	$(alljoyn_ndk_source_root)/cxx-stl/gnu-libstdc++/include

LOCAL_SRC_FILES := \
	daemon/AllJoynDebugObj.cc \
	daemon/AllJoynObj.cc \
	daemon/BTController.cc \
	daemon/BTNodeDB.cc \
	daemon/BTTransport.cc \
	daemon/Bus.cc \
	daemon/BusController.cc \
	daemon/DBusObj.cc \
	daemon/DaemonConfig.cc \
	daemon/DaemonRouter.cc \
	daemon/DaemonTransport.cc \
	daemon/NameService.cc \
	daemon/NameTable.cc \
	daemon/NetworkInterface.cc \
	daemon/NsProtocol.cc \
	daemon/Packet.cc \
	daemon/PacketEngine.cc \
	daemon/PacketEngineStream.cc \
	daemon/PacketPool.cc \
	daemon/RuleTable.cc \
	daemon/TCPTransport.cc \
	daemon/VirtualEndpoint.cc \
	daemon/bt_bluez/AdapterObject.cc \
	daemon/bt_bluez/BlueZHCIUtils.cc \
	daemon/bt_bluez/BlueZIfc.cc \
	daemon/bt_bluez/BlueZUtils.cc \
	daemon/bt_bluez/BTAccessor.cc \
	daemon/ice/Component.cc \
	daemon/ice/DaemonICETransport.cc \
	daemon/ice/DiscoveryManager.cc \
	daemon/ice/HttpConnection.cc \
	daemon/ice/ICECandidate.cc \
	daemon/ice/ICECandidatePair.cc \
	daemon/ice/ICEManager.cc \
	daemon/ice/ICESession.cc \
	daemon/ice/ICEStream.cc \
	daemon/ice/PersistGUID.cc \
	daemon/ice/ProximityScanEngine.cc \
	daemon/ice/RendezvousServerConnection.cc \
	daemon/ice/RendezvousServerInterface.cc \
	daemon/ice/SCRAM_SHA1.cc \
	daemon/ice/STUNSocketStream.cc \
	daemon/ice/Stun.cc \
	daemon/ice/StunActivity.cc \
	daemon/ice/StunAttributeBase.cc \
	daemon/ice/StunAttributeChannelNumber.cc \
	daemon/ice/StunAttributeData.cc \
	daemon/ice/StunAttributeErrorCode.cc \
	daemon/ice/StunAttributeEvenPort.cc \
	daemon/ice/StunAttributeFingerprint.cc \
	daemon/ice/StunAttributeMappedAddress.cc \
	daemon/ice/StunAttributeMessageIntegrity.cc \
	daemon/ice/StunAttributeRequestedTransport.cc \
	daemon/ice/StunAttributeStringBase.cc \
	daemon/ice/StunAttributeUnknownAttributes.cc \
	daemon/ice/StunAttributeXorMappedAddress.cc \
	daemon/ice/StunCredential.cc \
	daemon/ice/StunMessage.cc \
	daemon/ice/StunRetry.cc \
	daemon/ice/StunTransactionID.cc \
	daemon/JSON/json_reader.cc \
	daemon/JSON/json_value.cc \
	daemon/JSON/json_writer.cc \
	daemon/posix/daemon-main.cc \
	daemon/posix/DaemonTransport.cc \
	daemon/posix/ICEPacketStream.cc \
	daemon/posix/ProximityScanner.cc \
	daemon/posix/UDPPacketStream.cc

LOCAL_SRC_FILES += \
	autogen/Status.c \
	autogen/version.cc

LOCAL_SHARED_LIBRARIES := \
	liballjoyn \
	libcrypto \
	libssl \
	liblog

LOCAL_REQUIRED_MODULES := \
	external/openssl/crypto/libcrypto \
	external/openssl/ssl/libssl

#LOCAL_LDLIBS := \
#	-L$(alljoyn_ndk_source_root)/cxx-stl/gnu-libstdc++/libs/$(TARGET_CPU_ABI) \
#	-lgnustl_static

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := alljoyn-daemon

include $(BUILD_EXECUTABLE)

# Rules to build libAllJoynAndroidExt.so
include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp

# NOTE1: flag "-Wno-psabi" removes warning about GCC 4.4 va_list warning
# NOTE2: flag "-Wno-write-strings" removes warning about deprecated conversion
#        from string constant to char*
#
LOCAL_CFLAGS := \
	-Wno-psabi \
	-Wno-write-strings \
	-DQCC_CPU_ARM \
	-DQCC_OS_ANDROID \
	-DQCC_OS_GROUP_POSIX

LOCAL_C_INCLUDES := \
	external/alljoyn/alljoyn_core/alljoyn_android/alljoyn_android_ext/jni \
	external/alljoyn/alljoyn_core/autogen \
	external/alljoyn/alljoyn_core/inc \
	external/alljoyn/alljoyn_core/JSON \
	external/alljoyn/common/inc \
	external/alljoyn/common/inc/qcc \
	external/connectivity/stlport/stlport \
	external/openssl/include \

LOCAL_SRC_FILES := \
	alljoyn_android/alljoyn_android_ext/jni/AllJoynAndroidExt.cpp

LOCAL_SHARED_LIBRARIES := \
	liballjoyn \
	libcrypto \
	libssl \
	liblog

LOCAL_REQUIRED_MODULES := \
	external/alljoyn/alljoyn_core/liballjoyn \
	external/openssl/crypto/libcrypto \
	external/openssl/ssl/libssl \
	external/connectivity/stlport

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libAllJoynAndroidExt

LOCAL_ARM_MODE := arm

include $(BUILD_SHARED_LIBRARY)

# Rules to build AllJoynAndroidExt.apk
include $(CLEAR_VARS)
include $(CLEAR_VARS)
LOCAL_PATH := $(LOCAL_PATH)/alljoyn_android/alljoyn_android_ext
LOCAL_MODULE_TAGS := optional

#LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_SRC_FILES := \
	src/org/alljoyn/jni/AllJoynAndroidExt.java \
	src/org/alljoyn/jni/ScanResultMessage.java \
	src/org/alljoyn/jni/ScanResultsReceiver.java



LOCAL_PACKAGE_NAME := AllJoynAndroidExt
LOCAL_CERTIFICATE := platform
LOCAL_JNI_SHARED_LIBRARIES := libAllJoynAndroidExt

include $(BUILD_PACKAGE)

