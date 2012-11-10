/**
 * @file
 * Message Bus Client
 */

/******************************************************************************
 * Copyright 2009-2012, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/
#include <qcc/platform.h>
#include <qcc/Debug.h>

#include <signal.h>
#include <stdio.h>
#include <vector>

#include <qcc/String.h>
#include <qcc/Environ.h>
#include <qcc/Logger.h>
#include <qcc/StringSource.h>
#include <qcc/Util.h>

#include <alljoyn/DBusStd.h>
#include <alljoyn/version.h>

#include <Status.h>

#include "TCPTransport.h"
#include "DaemonTransport.h"

#if defined(QCC_OS_DARWIN)
#warning "Bluetooth transport not implemented on Darwin"
#elif defined(QCC_OS_WINDOWS)
//warning "Bluetooth transport currently not supported on Windows"
#else
#include "BTTransport.h"
#endif

#include "Bus.h"
#include "BusController.h"
#include "DaemonConfig.h"
#include "Transport.h"
#include "TransportList.h"

#define QCC_MODULE "ALLJOYN"

using namespace qcc;
using namespace std;
using namespace ajn;

/*
 * Simple config to
 * to provide some non-default limits for the daemon tcp transport.
 */
static const char daemonConfig[] =
    "<busconfig>"
    "  <type>alljoyn</type>"
    "  <limit name=\"auth_timeout\">5000</limit>"
    "  <limit name=\"max_incomplete_connections_tcp\">16</limit>"
    "  <limit name=\"max_completed_connections_tcp\">64</limit>"
    "  <ip_name_service>"
    "    <property interfaces=\"*\"/>"
    "    <property disable_directed_broadcast=\"false\"/>"
    "    <property enable_ipv4=\"true\"/>"
    "    <property enable_ipv6=\"true\"/>"
    "  </ip_name_service>"
    "  <ice>"
    "    <limit name=\"max_incomplete_connections\">16</limit>"
    "    <limit name=\"max_completed_connections\">64</limit>"
    "  </ice>"
    "  <ice_discovery_manager>"
    "    <property interfaces=\"*\"/>"
    "    <property server=\"rdvs.alljoyn.org\"/>"
    "    <property protocol=\"HTTPS\"/>"
    "    <property enable_ipv6=\"false\"/>"
    "  </ice_discovery_manager>"
    "</busconfig>";

/** Static top level message bus object */
static qcc::String serverArgs;

static volatile sig_atomic_t g_interrupt = false;

static void SigIntHandler(int sig)
{
    g_interrupt = true;
}

namespace org {
namespace alljoyn {
namespace alljoyn_test {
const char* InterfaceName = "org.alljoyn.alljoyn_test";
const char* WellKnownName = "org.alljoyn.alljoyn_test";
const char* ObjectPath = "/org/alljoyn/alljoyn_test";
namespace values {
const char* InterfaceName = "org.alljoyn.alljoyn_test.values";
}

}
}
}

class LocalTestObject : public BusObject {
  public:

    LocalTestObject(BusAttachment& bus, const char* path, unsigned long reportInterval)
        : BusObject(bus, path),
        reportInterval(reportInterval),
        prop_str_val("hello world"),
        prop_ro_str("I cannot be written"),
        prop_int_val(100)
    {
        QStatus status;

        /* Add the test interface to this object */
        const InterfaceDescription* regTestIntf = bus.GetInterface(::org::alljoyn::alljoyn_test::InterfaceName);
        AddInterface(*regTestIntf);
        /* Add the values interface to this object */
        const InterfaceDescription* valuesIntf = bus.GetInterface(::org::alljoyn::alljoyn_test::values::InterfaceName);
        AddInterface(*valuesIntf);

        /* Register the signal handler with the bus */
        const InterfaceDescription::Member* member = regTestIntf->GetMember("my_signal");
        status = bus.RegisterSignalHandler(this,
                                           static_cast<MessageReceiver::SignalHandler>(&LocalTestObject::SignalHandler),
                                           member,
                                           NULL);
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to register signal handler"));
        }

        /* Register the method handlers with the object */
        const MethodEntry methodEntries[] = {
            { regTestIntf->GetMember("my_ping"), static_cast<MessageReceiver::MethodHandler>(&LocalTestObject::Ping) }
        };
        status = AddMethodHandlers(methodEntries, ArraySize(methodEntries));
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to register method handlers for LocalTestObject"));
        }
    }

    void ObjectRegistered(void) {
        BusObject::ObjectRegistered();

        /* Request a well-known name */
        /* Note that you cannot make a blocking method call here */
        const ProxyBusObject& dbusObj = bus.GetDBusProxyObj();
        MsgArg args[2];
        args[0].Set("s", ::org::alljoyn::alljoyn_test::WellKnownName);
        args[1].Set("u", 6);
        QStatus status = dbusObj.MethodCallAsync(::ajn::org::freedesktop::DBus::InterfaceName,
                                                 "RequestName",
                                                 this,
                                                 static_cast<MessageReceiver::ReplyHandler>(&LocalTestObject::NameAcquiredCB),
                                                 args,
                                                 ArraySize(args));
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to request name %s", ::org::alljoyn::alljoyn_test::WellKnownName));
        }
    }

    void NameAcquiredCB(Message& msg, void* context)
    {
        /* Advertise the new name */

    }

    void SignalHandler(const InterfaceDescription::Member* member,
                       const char* sourcePath,
                       Message& msg)
    {
        map<qcc::String, size_t>::const_iterator it;

        ++rxCounts[sourcePath];

        if ((rxCounts[sourcePath] % reportInterval) == 0) {
            for (it = rxCounts.begin(); it != rxCounts.end(); ++it) {
                QCC_SyncPrintf("RxSignal: %s - %u\n", it->first.c_str(), it->second);
            }
        }
    }

    void Ping(const InterfaceDescription::Member* member, Message& msg)
    {
        /* Reply with same string that was sent to us */
        MsgArg arg(*(msg->GetArg(0)));
        printf("Pinged with: %s\n", msg->GetArg(0)->ToString().c_str());
        QStatus status = MethodReply(msg, &arg, 1);
        if (ER_OK != status) {
            QCC_LogError(status, ("Ping: Error sending reply"));
        }
    }

    QStatus Get(const char* ifcName, const char* propName, MsgArg& val)
    {
        QStatus status = ER_OK;
        if (0 == strcmp("int_val", propName)) {
            // val.Set("i", prop_int_val);
            val.typeId = ALLJOYN_INT32;
            val.v_int32 = prop_int_val;
        } else if (0 == strcmp("str_val", propName)) {
            // val.Set("s", prop_str_val.c_str());
            val.typeId = ALLJOYN_STRING;
            val.v_string.str = prop_str_val.c_str();
            val.v_string.len = prop_str_val.size();
        } else if (0 == strcmp("ro_str", propName)) {
            // val.Set("s", prop_ro_str_val.c_str());
            val.typeId = ALLJOYN_STRING;
            val.v_string.str = prop_ro_str.c_str();
            val.v_string.len = prop_ro_str.size();
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
        return status;
    }

    QStatus Set(const char* ifcName, const char* propName, MsgArg& val)
    {
        QStatus status = ER_OK;
        if ((0 == strcmp("int_val", propName)) && (val.typeId == ALLJOYN_INT32)) {
            prop_int_val = val.v_int32;
        } else if ((0 == strcmp("str_val", propName)) && (val.typeId == ALLJOYN_STRING)) {
            prop_str_val = val.v_string.str;
        } else if (0 == strcmp("ro_str", propName)) {
            status = ER_BUS_PROPERTY_ACCESS_DENIED;
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
        return status;
    }

    map<qcc::String, size_t> rxCounts;

    unsigned long signalDelay;
    unsigned long reportInterval;

    qcc::String prop_str_val;
    qcc::String prop_ro_str;
    int32_t prop_int_val;
};


static const char x509cert[] = {
    "-----BEGIN CERTIFICATE-----\n"
    "MIIB7TCCAZegAwIBAgIJAKSCIxJABMPWMA0GCSqGSIb3DQEBBQUAMFIxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApXYXNoaW5ndG9uMRAwDgYDVQQHDAdTZWF0dGxlMQ0w\n"
    "CwYDVQQKDARRdUlDMQ0wCwYDVQQDDARHcmVnMB4XDTEwMDgwMzIzNTYzOVoXDTEx\n"
    "MDgwMzIzNTYzOVowUjELMAkGA1UEBhMCVVMxEzARBgNVBAgMCldhc2hpbmd0b24x\n"
    "EDAOBgNVBAcMB1NlYXR0bGUxDTALBgNVBAoMBFF1SUMxDTALBgNVBAMMBEdyZWcw\n"
    "XDANBgkqhkiG9w0BAQEFAANLADBIAkEA3b+TpTkJD03LlgKKA9phSeA+5owwM/jj\n"
    "PrRFcrH0mrFrHRujyPCuWRwOZojXgxVFU/jaTOyQ5sA5df7nEMgf/wIDAQABo1Aw\n"
    "TjAdBgNVHQ4EFgQUr6/4jRv/8qYIAtu/x9wSHllToxgwHwYDVR0jBBgwFoAUr6/4\n"
    "jRv/8qYIAtu/x9wSHllToxgwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAANB\n"
    "ABJSIipYXtLymiidV3J6cOlurPvEM/mXey9FMjvAjrNrrhuOBP1SFrcW+ubWsmWi\n"
    "EeP1srLyLDXtE5AogwPcaVc=\n"
    "-----END CERTIFICATE-----"
};

static const char privKey[] = {
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-128-CBC,1B43B2A4AE39BF6CECCA363FC9D02237\n"
    "\n"
    "zEMSBXr4Up+C5ZeWVZw5LPZHColZ8+ZhgkNHdqSfgyjri7Ij6nb1ABcbWeJBeqtF\n"
    "9fsijcTqUACVOhrAFi3d+F9HYP6taqDDwCJj638cTnYGM9j+WAspNOm05FlFmgvs\n"
    "guwpqc98RAj29C72zYb3GWoW0xIOhPF84OWKppweMSV6UFpLqnpFmo0zGT4ItMhV\n"
    "/tOdXyrTzhyjwFWhOBM1GZSKl1AtmIgDW88fFfGyPxIQSS/30ur0/dgUinVODBLP\n"
    "kNP73tpiBCeSHWqLlHV/bTer7TE5dsbyvvbFKftns/wP4Eri3V4SsldkURUJTrG7\n"
    "oGvwY4hwV0iZjSUcX1aBrfXE6oc8LAaJrZzNDUvNLjM2jHzIvMTwWIa3R1z9yjWl\n"
    "Rk5RScL4+i2JPll9SzrkhIGvh0ElYRdzbfkrUIY2anGwxM5Ihcv8Z3kpYJyvhdJu\n"
    "-----END RSA PRIVATE KEY-----\n"
};

class MyAuthListener : public AuthListener {
    bool RequestCredentials(const char* authMechanism, const char* authPeer, uint16_t authCount, const char* userId, uint16_t credMask, Credentials& creds) {

        if (strcmp(authMechanism, "ALLJOYN_SRP_KEYX") == 0) {
            if (credMask & AuthListener::CRED_PASSWORD) {
                creds.SetPassword("123456");
                printf("AuthListener returning fixed pin \"%s\" for %s\n", creds.GetPassword().c_str(), authMechanism);
            }
            return true;
        }

        if (strcmp(authMechanism, "ALLJOYN_RSA_KEYX") == 0) {
            if (credMask & AuthListener::CRED_CERT_CHAIN) {
                creds.SetCertChain(x509cert);
            }
            if (credMask & AuthListener::CRED_PRIVATE_KEY) {
                creds.SetPrivateKey(privKey);
            }
            if (credMask & AuthListener::CRED_PASSWORD) {
                creds.SetPassword("123456");
            }
            return true;
        }

        if (strcmp(authMechanism, "ALLJOYN_SRP_LOGON") == 0) {
            if (!userId) {
                return false;
            }
            printf("Attemping to logon user %s\n", userId);
            if (strcmp(userId, "happy") == 0) {
                if (credMask & AuthListener::CRED_PASSWORD) {
                    creds.SetPassword("123456");
                    return true;
                }
            }
            if (strcmp(userId, "sleepy") == 0) {
                if (credMask & AuthListener::CRED_PASSWORD) {
                    creds.SetPassword("123456");
                    return true;
                }
            }
            if (strcmp(userId, "sneezy") == 0) {
                if (credMask & AuthListener::CRED_PASSWORD) {
                    creds.SetPassword("123456");
                    return true;
                }
            }
        }
        return false;
    }

    bool VerifyCredentials(const char* authMechanism, const char* authPeer, const Credentials& creds) {
        if (strcmp(authMechanism, "ALLJOYN_RSA_KEYX") == 0) {
            if (creds.IsSet(AuthListener::CRED_CERT_CHAIN)) {
                printf("Verify\n%s\n", creds.GetCertChain().c_str());
                return true;
            }
        }
        return false;
    }

    void AuthenticationComplete(const char* authMechanism, const char* authPeer, bool success) {
        printf("Authentication %s %s\n", authMechanism, success ? "succesful" : "failed");
    }

    void SecurityViolation(const char* error) {
        printf("Security violation %s\n", error);
    }
};


static void usage(void)
{
    printf("Usage: bbdaemon [-h] [-m] [-b]\n\n");
    printf("Options:\n");
    printf("   -h   = Print this help message\n");
    printf("   -b   = Disable Bluetooth transport\n");
    printf("   -m   = Mimic behavior of bbservice within daemon\n");
    printf("   -be  = Send messages as big endian\n");
    printf("   -le  = Send messages as little endian\n");
}

//
// This code can be run as a native executable, in which case the linker arranges to
// call main(), or it can be run as an Android Service.  In this case, the daemon
// is implemented as a static library which is linked into a JNI dynamic library and
// called from the service code.
//
#if defined(DAEMON_LIB)
extern "C" int DaemonMain(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
#if defined(NDEBUG) && defined(QCC_OS_ANDROID)
    LoggerSetting::GetLoggerSetting("bbdaemon", LOG_ERR, true, NULL);
#else
    LoggerSetting::GetLoggerSetting("bbdaemon", LOG_DEBUG, false, stdout);
#endif

    QStatus status = ER_OK;
    qcc::GUID128 guid;
    bool mimicBbservice = false;
    bool noBT = false;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    /* Parse command line args */
    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp("-h", argv[i])) {
            usage();
            exit(0);
        } else if (0 == strcmp("-m", argv[i])) {
            mimicBbservice = true;
        } else if (0 == strcmp("-b", argv[i])) {
            noBT = true;
        } else if (0 == strcmp("-le", argv[i])) {
            _Message::SetEndianess(ALLJOYN_LITTLE_ENDIAN);
        } else if (0 == strcmp("-be", argv[i])) {
            _Message::SetEndianess(ALLJOYN_BIG_ENDIAN);
        } else {
            status = ER_FAIL;
            printf("Unknown option %s\n", argv[i]);
            usage();
            exit(1);
        }
    }

    /* Get env vars */
    Environ* env = Environ::GetAppEnviron();

#ifdef QCC_OS_GROUP_WINDOWS
    serverArgs = env->Find("BUS_SERVER_ADDRESSES", "localhost:port=9956;tcp:addr=0.0.0.0,port=9955,family=ipv4;bluetooth:");
#else

#if defined(DAEMON_LIB)
    /* Android Applications don't have enough privilege to start a bluetooth
     * transport.  This is because the daemon needs to access /dev/socket/dbus
     * to do so.  The socket is owned by (user, group) bluetooth:bluetooth
     * but privilege BLUETOOTH or BLUETOOTH_ADMIN don't help since the socket
     * is apparently considered part of DBus.  Permissions on the dbus need
     * to be relaxed to 666 to make this work for everyday apps.  We also let
     * the tcp listen spec default to listening and multicasting on all
     * adapters. */
    serverArgs = env->Find("BUS_SERVER_ADDRESSES", "unix:abstract=alljoyn;tcp:family=ipv4");
#else
    if (noBT) {
        serverArgs = env->Find("BUS_SERVER_ADDRESSES", "unix:abstract=alljoyn;tcp:addr=0.0.0.0,port=9955,family=ipv4");
    } else {
        serverArgs = env->Find("BUS_SERVER_ADDRESSES", "unix:abstract=alljoyn;tcp:addr=0.0.0.0,port=9955,family=ipv4;bluetooth:");
    }

#endif /* DAEMON_LIB */

#endif /* !QCC_OS_GROUP_WINDOWS */

    /*
     * Teach the transport list how to make transports it may see referred to
     * in the serverArgs above.  The daemon transport is created by default because
     * it is always required. The other transports are only created if specified in
     * the environment.
     */
    TransportFactoryContainer cntr;
    cntr.Add(new TransportFactory<DaemonTransport>(DaemonTransport::TransportName, true));
    cntr.Add(new TransportFactory<TCPTransport>("tcp", false));

#if !defined(QCC_OS_DARWIN)
#if !defined(QCC_OS_WINDOWS)
    if (!noBT) {
        cntr.Add(new TransportFactory<BTTransport>("bluetooth", false));
    }
#endif
#endif

    /* Create message bus with support for alternate transports */
    Bus bus("bbdaemon", cntr, serverArgs.c_str());
    BusController controller(bus);

    if (mimicBbservice) {
        /* Add org.alljoyn.alljoyn_test interface */
        InterfaceDescription* testIntf = NULL;
        status = bus.CreateInterface(::org::alljoyn::alljoyn_test::InterfaceName, testIntf);
        if (ER_OK == status) {
            testIntf->AddSignal("my_signal", NULL, NULL, 0);
            testIntf->AddMethod("my_ping", "s", "s", "outStr,inStr", 0);
            testIntf->Activate();
        } else {
            QCC_LogError(status, ("Failed to create interface %s", ::org::alljoyn::alljoyn_test::InterfaceName));
        }

        /* Add org.alljoyn.alljoyn_test.values interface */
        if (ER_OK == status) {
            InterfaceDescription* valuesIntf = NULL;
            status = bus.CreateInterface(::org::alljoyn::alljoyn_test::values::InterfaceName, valuesIntf);
            if (ER_OK == status) {
                valuesIntf->AddProperty("int_val", "i", PROP_ACCESS_RW);
                valuesIntf->AddProperty("str_val", "s", PROP_ACCESS_RW);
                valuesIntf->AddProperty("ro_str", "s", PROP_ACCESS_READ);
                valuesIntf->Activate();
            } else {
                QCC_LogError(status, ("Failed to create interface %s", ::org::alljoyn::alljoyn_test::values::InterfaceName));
            }
        }
    }

    if (ER_OK == status) {
        /* Start the bus controller */
        status = controller.Init(serverArgs);
        if (ER_OK == status) {
            LocalTestObject* testObj = NULL;

            if (mimicBbservice) {
                bus.EnablePeerSecurity("ALLJOYN_RSA_KEYX ALLJOYN_SRP_KEYX ALLJOYN_SRP_LOGON", new MyAuthListener());
                testObj = new LocalTestObject(bus, ::org::alljoyn::alljoyn_test::ObjectPath, 10);
                bus.RegisterBusObject(*testObj);
            }

            printf("AllJoyn Daemon PID = %d\n", GetPid());
            fflush(stdout);

            if (ER_OK == status) {
                while (g_interrupt == false) {
                    qcc::Sleep(100);
                }
                bus.StopListen(serverArgs.c_str());
            }

            if (mimicBbservice) {
                bus.UnregisterBusObject(*testObj);
                delete testObj;
            }
        } else {
            QCC_LogError(status, ("Bus::Start failed"));
        }
    } else {
        QCC_LogError(status, ("BusController initialization failed"));
    }

    return (int) status;
}
