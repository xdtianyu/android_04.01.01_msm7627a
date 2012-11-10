/**
 * @file
 * @brief  Sample implementation of an AllJoyn client.
 *
 * This is a simple client that will run and change the 'name' property of the
 * 'org.alljoyn.Bus.signal_sample' service then exit.
 */

/******************************************************************************
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
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
#include <alljoyn/version.h>
#include <alljoyn/AllJoynStd.h>
#include <Status.h>

using namespace std;
using namespace qcc;
using namespace ajn;

/** Static top level message bus object */
static BusAttachment* g_msgBus = NULL;

static const char* SERVICE_NAME = "org.alljoyn.Bus.signal_sample";
static const char* SERVICE_PATH = "/";
static const SessionPort SERVICE_PORT = 25;

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
            SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
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

    /* Create message bus */
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

    /* Register a bus listener in order to get discovery indications */
    if (ER_OK == status) {
        g_msgBus->RegisterBusListener(g_busListener);
        printf("BusListener Registered.\n");
    }

    /* Begin discovery on the well-known name of the service to be called */
    if (ER_OK == status) {
        status = g_msgBus->FindAdvertisedName(SERVICE_NAME);
        if (status != ER_OK) {
            printf("org.alljoyn.Bus.FindAdvertisedName failed (%s)\n", QCC_StatusText(status));
        }
    }

    /* Wait for join session to complete */
    while (!s_joinComplete && !g_interrupt) {
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10 * 1000);
#endif
    }

    if (status == ER_OK && g_interrupt == false) {
        ProxyBusObject remoteObj(*g_msgBus, SERVICE_NAME, SERVICE_PATH, s_sessionId);
        status = remoteObj.IntrospectRemoteObject();
        if (ER_OK != status) {
            printf("Introspection of %s (path=%s) failed\n", SERVICE_NAME, SERVICE_PATH);
            printf("Make sure the service is running before launching the client.\n");
        } else {
            if (argc > 1) {
                status = remoteObj.SetProperty(SERVICE_NAME, "name", argv[1]);
                if (status != ER_OK) {
                    printf("Error calling SetProperty to change the 'name' property.\n");
                }
            } else {
                printf("Error new name not given: nameChange_client [new name]\n");
            }
        }
    }

    /* Deallocate bus */
    BusAttachment* deleteMe = g_msgBus;
    g_msgBus = NULL;
    delete deleteMe;

    printf("name Change client exiting with status %d (%s)\n", status, QCC_StatusText(status));

    return (int) status;
}



