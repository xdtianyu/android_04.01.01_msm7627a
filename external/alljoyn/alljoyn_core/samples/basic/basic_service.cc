/**
 * @file
 * @brief Sample implementation of an AllJoyn service.
 *
 * This sample will show how to set up an AllJoyn service that will registered with the
 * wellknown name 'org.alljoyn.Bus.method_sample'.  The service will register a method call
 * with the name 'cat'  this method will take two input strings and return a
 * Concatenated version of the two strings.
 *
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
static const char* INTERFACE_NAME = "org.alljoyn.Bus.sample";
static const char* SERVICE_NAME = "org.alljoyn.Bus.sample";
static const char* SERVICE_PATH = "/sample";
static const SessionPort SERVICE_PORT = 25;

static volatile sig_atomic_t g_interrupt = false;

static void SigIntHandler(int sig)
{
    g_interrupt = true;
}

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
            { exampleIntf->GetMember("cat"), static_cast<MessageReceiver::MethodHandler>(&BasicSampleObject::Cat) }
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


    void Cat(const InterfaceDescription::Member* member, Message& msg)
    {
        /* Concatenate the two input strings and reply with the result. */
        qcc::String inStr1 = msg->GetArg(0)->v_string.str;
        qcc::String inStr2 = msg->GetArg(1)->v_string.str;
        qcc::String outStr = inStr1 + inStr2;

        MsgArg outArg("s", outStr.c_str());
        QStatus status = MethodReply(msg, &outArg, 1);
        if (ER_OK != status) {
            printf("Ping: Error sending reply\n");
        }
    }
};

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
    g_msgBus = new BusAttachment("myApp", true);


    /* Add org.alljoyn.Bus.method_sample interface */
    InterfaceDescription* testIntf = NULL;
    status = g_msgBus->CreateInterface(INTERFACE_NAME, testIntf);
    if (status == ER_OK) {
        printf("Interface Created.\n");
        testIntf->AddMethod("cat", "ss",  "s", "inStr1,inStr2,outStr", 0);
        testIntf->Activate();
    } else {
        printf("Failed to create interface 'org.alljoyn.Bus.method_sample'\n");
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
     * 1) Request a well-known name that will be used by the client to discover
     *    this service
     * 2) Create a session
     * 3) Advertise the well-known name
     */
    /* Request name */
    if (ER_OK == status) {
        uint32_t flags = DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE;
        QStatus status = g_msgBus->RequestName(SERVICE_NAME, flags);
        if (ER_OK != status) {
            printf("RequestName(%s) failed (status=%s)\n", SERVICE_NAME, QCC_StatusText(status));
        }
    }

    /* Create session */
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
    if (ER_OK == status) {
        SessionPort sp = SERVICE_PORT;
        status = g_msgBus->BindSessionPort(sp, opts, *s_busListener);
        if (ER_OK != status) {
            printf("BindSessionPort failed (%s)\n", QCC_StatusText(status));
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
