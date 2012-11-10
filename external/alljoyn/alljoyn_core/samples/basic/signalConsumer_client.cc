/**
 * @file
 * @brief Sample implementation of an AllJoyn client.
 *
 * This client will subscribe to the nameChanged signal sent from the 'org.alljoyn.Bus.signal_sample'
 * service.  When a name change signal is sent this will print out the new value for the
 * 'name' property that was sent by the service.
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

#include <signal.h>
#include <stdio.h>
#include <assert.h>
#include <vector>

#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/version.h>
#include <alljoyn/AllJoynStd.h>

#include <Status.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;
using namespace ajn;

/*constants*/
static const char* INTERFACE_NAME = "org.alljoyn.Bus.signal_sample";
static const char* SERVICE_NAME = "org.alljoyn.Bus.signal_sample";
static const char* SERVICE_PATH = "/";
static const SessionPort SERVICE_PORT = 25;

/** Static top level message bus object */
static BusAttachment* g_msgBus = NULL;

static bool s_joinComplete = false;
static SessionId s_sessionId = 0;

static volatile sig_atomic_t g_interrupt = false;

static void SigIntHandler(int sig)
{
    g_interrupt = true;
}

/** AlljounListener receives discovery events from AllJoyn */
class MyBusListener : public BusListener {
  public:
    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        if (0 == strcmp(name, SERVICE_NAME)) {
            printf("FoundAdvertisedName(name=%s, prefix=%s)\n", name, namePrefix);
            /* We found a remote bus that is advertising basic sercice's  well-known name so connect to it */
            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
            QStatus status = g_msgBus->JoinSession(name, SERVICE_PORT, NULL, s_sessionId, opts);
            if (ER_OK != status) {
                printf("JoinSession failed (status=%s)\n", QCC_StatusText(status));
            } else {
                printf("JoinSession SUCCESS (Session id=%d)\n", s_sessionId);
            }
        }
        s_joinComplete = true;
    }

    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        if (newOwner && (0 == strcmp(busName, SERVICE_NAME))) {
            printf("NameOwnerChanged: name=%s, oldOwner=%s, newOwner=%s\n",
                   busName,
                   previousOwner ? previousOwner : "<none>",
                   newOwner ? newOwner : "<none>");
        }
    }
};

/** Static bus listener */
static MyBusListener g_busListener;

class SignalListeningObject : public BusObject {
  public:
    SignalListeningObject(BusAttachment& bus, const char* path) :
        BusObject(bus, path),
        nameChangedMember(NULL)
    {
        /* Add org.alljoyn.Bus.signal_sample interface */
        InterfaceDescription* intf = NULL;
        QStatus status = bus.CreateInterface(INTERFACE_NAME, intf);
        if (status == ER_OK) {
            printf("Interface created successfully.\n");
            intf->AddSignal("nameChanged", "s", "newName", 0);
            intf->AddProperty("name", "s", PROP_ACCESS_RW);
            intf->Activate();
        } else {
            printf("Failed to create interface %s\n", INTERFACE_NAME);
        }

        status = AddInterface(*intf);

        if (status == ER_OK) {
            printf("Interface successfully added to the bus.\n");
            /* Register the signal handler 'nameChanged' with the bus*/
            nameChangedMember = intf->GetMember("nameChanged");
            assert(nameChangedMember);
        } else {
            printf("Failed to Add interface: %s", INTERFACE_NAME);
        }

        /* register the signal handler for the the 'nameChanged' signal */
        status =  bus.RegisterSignalHandler(this,
                                            static_cast<MessageReceiver::SignalHandler>(&SignalListeningObject::NameChangedSignalHandler),
                                            nameChangedMember,
                                            NULL);
        if (status != ER_OK) {
            printf("Failed to register signal handler for %s.nameChanged\n", SERVICE_NAME);
        } else {
            printf("Registered signal handler for %s.nameChanged\n", SERVICE_NAME);
        }
        /* Empty constructor */
    }

    QStatus SubscribeNameChangedSignal(void) {
        return bus.AddMatch("type='signal',interface='org.alljoyn.Bus.signal_sample',member='nameChanged'");
    }

    void NameChangedSignalHandler(const InterfaceDescription::Member* member,
                                  const char* sourcePath,
                                  Message& msg)
    {
        printf("--==## signalConsumer: Name Changed signal Received ##==--\n");
        printf("\tNew name: %s\n", msg->GetArg(0)->v_string.str);

    }

  private:
    const InterfaceDescription::Member* nameChangedMember;

};

/** Main entry point */
int main(int argc, char** argv, char** envArg)
{
    QStatus status = ER_OK;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());

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

    g_msgBus = new BusAttachment("myApp", true);

    /* Start the msg bus */
    status = g_msgBus->Start();
    if (ER_OK != status) {
        printf("BusAttachment::Start failed\n");
    } else {
        printf("BusAttachment started.\n");
    }

    /* Connect to the bus */
    if (ER_OK == status) {
        status = g_msgBus->Connect(connectArgs);
        if (ER_OK != status) {
            printf("BusAttachment::Connect(\"%s\") failed\n", connectArgs);
        } else {
            printf("BusAttchement connected to %s\n", connectArgs);
        }
    }

    SignalListeningObject object(*g_msgBus, SERVICE_PATH);
    /* Register object */
    g_msgBus->RegisterBusObject(object);


    /* Register a bus listener in order to get discovery indications */
    g_msgBus->RegisterBusListener(g_busListener);
    printf("BusListener Registered.\n");
    if (ER_OK == status) {
        status = g_msgBus->FindAdvertisedName(SERVICE_NAME);
        if (status != ER_OK) {
            printf("org.alljoyn.Bus.FindAdvertisedName failed (%s))\n", QCC_StatusText(status));
        }
    }

    /* Wait for join session to complete */
    while (!s_joinComplete && !g_interrupt) {
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100 * 1000);
#endif
    }

    if (status == ER_OK && g_interrupt == false) {
        status = object.SubscribeNameChangedSignal();
        if (status != ER_OK) {
            printf("Failed to Subscribe to the Name Changed Signal.\n");
        } else {
            printf("Successfully Subscribed to the Name Changed Signal.\n");
        }
    }

    if (status == ER_OK) {
        while (g_interrupt == false) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100 * 1000);
#endif
        }
    } else {
        printf("BusAttachment::Start failed\n");
    }

    /* Deallocate bus */
    BusAttachment* deleteMe = g_msgBus;
    g_msgBus = NULL;
    delete deleteMe;

    printf("Exiting with status %d (%s)\n", status, QCC_StatusText(status));

    return (int) status;
}
