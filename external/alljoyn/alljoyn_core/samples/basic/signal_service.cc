/**
 * @file
 * @brief Sample implementation of an AllJoyn service.
 *
 * This sample will show how to set up an AllJoyn service that will registered with the
 * well-known name 'org.alljoyn.Bus.signal_sample'.  The service will register a signal method 'nameChanged'
 * as well as a property 'name'.
 *
 * When the property 'sampleName' is changed by any client this service will emit the new name using
 * the 'nameChanged' signal.
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
#include <alljoyn/BusObject.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/version.h>
#include <Status.h>

using namespace std;
using namespace qcc;
using namespace ajn;

class MyBusListener;

/** Static top level message bus object */
static BusAttachment* g_msgBus = NULL;

static SessionId s_sessionId = 0;
static MyBusListener* s_busListener = NULL;

static const char* INTERFACE_NAME = "org.alljoyn.Bus.signal_sample";
static const char* SERVICE_NAME = "org.alljoyn.Bus.signal_sample";
static const char* SERVICE_PATH = "/";
static const SessionPort SERVICE_PORT = 25;

static volatile sig_atomic_t g_interrupt = false;

static void SigIntHandler(int sig)
{
    g_interrupt = true;
}

class BasicSampleObject : public BusObject {
  public:
    BasicSampleObject(BusAttachment& bus, const char* path) :
        BusObject(bus, path),
        nameChangedMember(NULL),
        prop_name("Default name")
    {
        /* Add org.alljoyn.Bus.signal_sample interface */
        InterfaceDescription* intf = NULL;
        QStatus status = bus.CreateInterface(INTERFACE_NAME, intf);
        if (status == ER_OK) {
            intf->AddSignal("nameChanged", "s", "newName", 0);
            intf->AddProperty("name", "s", PROP_ACCESS_RW);
            intf->Activate();
        } else {
            printf("Failed to create interface %s\n", INTERFACE_NAME);
        }

        status = AddInterface(*intf);

        if (status == ER_OK) {
            /* Register the signal handler 'nameChanged' with the bus*/
            nameChangedMember = intf->GetMember("nameChanged");
            assert(nameChangedMember);
        } else {
            printf("Failed to Add interface: %s", INTERFACE_NAME);
        }
    }

    QStatus EmitNameChangedSignal(qcc::String newName)
    {
        printf("Emiting Name Changed Signal.\n");
        assert(nameChangedMember);
        if (0 == s_sessionId) {
            printf("Sending NameChanged signal without a session id\n");
        }
        MsgArg arg("s", newName.c_str());
        uint8_t flags = ALLJOYN_FLAG_GLOBAL_BROADCAST;
        QStatus status = Signal(NULL, 0, *nameChangedMember, &arg, 1, 0, flags);

        return status;
    }

    QStatus Get(const char* ifcName, const char* propName, MsgArg& val)
    {
        printf("Get 'name' property was called returning: %s\n", prop_name.c_str());
        QStatus status = ER_OK;
        if (0 == strcmp("name", propName)) {
            val.typeId = ALLJOYN_STRING;
            val.v_string.str = prop_name.c_str();
            val.v_string.len = prop_name.length();
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
        return status;
    }

    QStatus Set(const char* ifcName, const char* propName, MsgArg& val)
    {
        QStatus status = ER_OK;
        if ((0 == strcmp("name", propName)) && (val.typeId == ALLJOYN_STRING)) {
            printf("Set 'name' property was called changing name to %s\n", val.v_string.str);
            prop_name = val.v_string.str;
            EmitNameChangedSignal(prop_name);
        } else {
            status = ER_BUS_NO_SUCH_PROPERTY;
        }
        return status;
    }
  private:
    const InterfaceDescription::Member* nameChangedMember;
    qcc::String prop_name;
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
int main(int argc, char** argv, char** envArg) {
    QStatus status = ER_OK;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    /* Create message bus */
    g_msgBus = new BusAttachment("myApp", true);

    const char* connectArgs = getenv("BUS_ADDRESS");
    if (connectArgs == NULL) {
#ifdef _WIN32
        connectArgs = "tcp:addr=127.0.0.1,port=9956";
#else
        connectArgs = "unix:abstract=alljoyn";
#endif
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

        /* Register objects */

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
            sleep(100 * 1000);
#endif
        }
    }

    /* Clean up msg bus */
    BusAttachment* deleteMe = g_msgBus;
    g_msgBus = NULL;
    delete deleteMe;

    return (int) status;
}
