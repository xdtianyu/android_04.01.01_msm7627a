/**
 * @file
 * @brief Sample implementation of an AllJoyn service.
 *
 * This sample has an implementation of a sercure sample that is setup to use a
 * shared keystore.
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <vector>
#include <time.h>

#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/version.h>

#include <Status.h>

using namespace std;
using namespace qcc;
using namespace ajn;

class MyBusListener;

/** Static top level message bus object */
static BusAttachment* g_msgBus = NULL;

static MyBusListener* s_busListener = NULL;

/*constants*/
static const char* INTERFACE_NAME = "org.alljoyn.bus.samples.secure.SecureInterface";
static const char* SERVICE_NAME = "org.alljoyn.bus.samples.secure";
static const char* SERVICE_PATH = "/SecureService";
static const SessionPort SERVICE_PORT = 42;

static volatile sig_atomic_t g_interrupt = false;

/**
 * Control-C signal handler
 */
static void SigIntHandler(int sig)
{
    g_interrupt = true;
}

/*
 *  Implementation of a BusObject
 *  This class contains the implementation of the secure interface.
 *  The Ping method is the code that will be called when the a remode process
 *  makes a remote method call to Ping.
 *
 *
 */
class BasicSampleObject : public BusObject {
  public:
    BasicSampleObject(BusAttachment& bus, const char* path) :
        BusObject(bus, path)
    {
        /** Add the test interface to this object */
        const InterfaceDescription* exampleIntf = bus.GetInterface(INTERFACE_NAME);
        assert(exampleIntf);
        AddInterface(*exampleIntf);

        /** Register the method handlers with the object */
        const MethodEntry methodEntries[] = {
            { exampleIntf->GetMember("Ping"), static_cast<MessageReceiver::MethodHandler>(&BasicSampleObject::Ping) }
        };
        QStatus status = AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
        if (ER_OK != status) {
            printf("Failed to register method handlers for BasicSampleObject");
        }
    }

    void ObjectRegistered()
    {
        BusObject::ObjectRegistered();
        printf("ObjectRegistered has been called\n");
    }


    void Ping(const InterfaceDescription::Member* member, Message& msg)
    {
        qcc::String outStr = msg->GetArg(0)->v_string.str;
        printf("Ping : %s\n", outStr.c_str());
        printf("Reply : %s\n", outStr.c_str());
        MsgArg outArg("s", outStr.c_str());
        QStatus status = MethodReply(msg, &outArg, 1);
        if (ER_OK != status) {
            printf("Ping: Error sending reply\n");
        }
    }
};

/*
 * The MyBusListener class implements the public methods for two classes
 * BusListener and SessionPortListener
 * The BusListener class is responsible for providing the NameOwnerChanged call
 * back method
 * The SessionPortListener is responsible for providing the AcceptSessionJoiner
 * call back method
 */
class MyBusListener : public BusListener, public SessionPortListener {
    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        if (newOwner && (0 == strcmp(busName, SERVICE_NAME))) {
            printf("NameOwnerChanged: name=%s, oldOwner=%s, newOwner=%s\n",
                   busName,
                   previousOwner ? previousOwner : "<none>",
                   newOwner ? newOwner : "<none>");
        }
    }
    bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
    {
        if (sessionPort != SERVICE_PORT) {
            printf("Rejecting join attempt on unexpected session port %d\n", sessionPort);
            return false;
        }
        printf("Accepting join session request from %s (opts.proximity=%x, opts.traffic=%x, opts.transports=%x)\n",
               joiner, opts.proximity, opts.traffic, opts.transports);
        return true;
    }
};

/*
 * This is the local implementation of the an AuthListener.  SrpKeyXListener is
 * designed to only handle SRP Key Exchange Authentication requests.
 *
 * When a Password request (CRED_PASSWORD) comes in using ALLJOYN_SRP_KEYX the
 * code will generate a 6 digit random pin code.  The client must enter the same
 * pin code into his AuthListener for the Authentication to be successful.
 *
 * If any other authMechanism is used other than SRP Key Exchange authentication
 * will fail.
 */
class SrpKeyXListener : public AuthListener {
    bool RequestCredentials(const char* authMechanism, const char* authPeer, uint16_t authCount, const char* userId, uint16_t credMask, Credentials& creds) {
        printf("RequestCredentials for authenticating %s using mechanism %s\n", authPeer, authMechanism);
        if (strcmp(authMechanism, "ALLJOYN_SRP_KEYX") == 0) {
            if (credMask & AuthListener::CRED_PASSWORD) {
                if (authCount <= 3) {
                    /* seed the random number */
                    srand(time(NULL));
                    int pin = rand() % 1000000;
                    char pinStr[7];
                    snprintf(pinStr, 7, "%06d", pin);
                    printf("One Time Password : %s\n", pinStr);
                    fflush(stdout);
                    creds.SetPassword(pinStr);
                    return true;
                } else {
                    return false;
                }
            }
        }
        return false;
    }

    void AuthenticationComplete(const char* authMechanism, const char* authPeer, bool success) {
        printf("Authentication %s %s\n", authMechanism, success ? "succesful" : "failed");
    }
};

/** Main entry point */
int main(int argc, char** argv, char** envArg)
{
    QStatus status = ER_OK;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    const char* connectArgs = getenv("BUS_ADDRESS");
    if (connectArgs == NULL) {
#ifdef _WIN32
        connectArgs = "tcp:addr=127.0.0.1,port=9956";
#else
        connectArgs = "unix:abstract=alljoyn";
#endif
    }

    /* Create message bus */
    g_msgBus = new BusAttachment("SRPSecurityServiceA", true);


    /* Add org.alljoyn.bus.samples.secure.SecureInterface interface */
    InterfaceDescription* testIntf = NULL;
    status = g_msgBus->CreateInterface(INTERFACE_NAME, testIntf, true);
    if (status == ER_OK) {
        testIntf->AddMethod("Ping", "s",  "s", "inStr,outStr", 0);
        testIntf->Activate();
    } else {
        printf("Failed to create interface %s\n", INTERFACE_NAME);
    }

    /* Register a bus listener */
    if (ER_OK == status) {
        s_busListener = new MyBusListener();
        g_msgBus->RegisterBusListener(*s_busListener);
    }

    BasicSampleObject testObj(*g_msgBus, SERVICE_PATH);

    /* Start the msg bus */
    status = g_msgBus->Start();
    if (ER_OK == status) {
        printf("BusAttachement started.\n");
        /* Register  local objects and connect to the daemon */
        g_msgBus->RegisterBusObject(testObj);

        /*
         * enable security
         * note the location of the keystore file has been specified and the
         * isShared parameter is being set to true. So this keystore file can
         * be used by multiple applications
         */
        status = g_msgBus->EnablePeerSecurity("ALLJOYN_SRP_KEYX", new SrpKeyXListener(), "/.alljoyn_keystore/s_central.ks", true);
        if (ER_OK != status) {
            printf("BusAttachment::EnablePeerSecurity failed (%s)\n", QCC_StatusText(status));
        } else {
            printf("BusAttachment::EnablePeerSecurity succesful\n");
        }

        /* Create the client-side endpoint */
        status = g_msgBus->Connect(connectArgs);
        if (ER_OK != status) {
            printf("Failed to connect to \"%s\"\n", connectArgs);
            exit(1);
        } else {
            printf("Connected to '%s'\n", connectArgs);
        }
    } else {
        printf("BusAttachment::Start failed\n");
    }

    /*
     * Advertise this service on the bus
     * There are three steps to advertising this service on the bus
     * 1) Create a session
     * 2) Request a well-known name that will be used by the client to discover
     *    this service
     * 3) Advertise the well-known name
     */
    /* Create session */
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
    if (ER_OK == status) {
        SessionPort sp = SERVICE_PORT;
        status = g_msgBus->BindSessionPort(sp, opts, *s_busListener);
        if (ER_OK != status) {
            printf("BindSessionPort failed (%s)\n", QCC_StatusText(status));
        }
    }

    /* Request name */
    if (ER_OK == status) {
        uint32_t flags = DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE;
        QStatus status = g_msgBus->RequestName(SERVICE_NAME, flags);
        if (ER_OK != status) {
            printf("RequestName(%s) failed (status=%s)\n", SERVICE_NAME, QCC_StatusText(status));
        }
    }

    /* Advertise name */
    if (ER_OK == status) {
        status = g_msgBus->AdvertiseName(SERVICE_NAME, opts.transports);
        if (status != ER_OK) {
            printf("Failed to advertise name %s (%s)\n", SERVICE_NAME, QCC_StatusText(status));
        }
    }

    if (ER_OK == status) {
        while (g_interrupt == false) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100 * 1000);
#endif
        }
    }

    /* Clean up msg bus */
    if (g_msgBus) {
        BusAttachment* deleteMe = g_msgBus;
        g_msgBus = NULL;
        delete deleteMe;
    }
    return (int) status;
}
