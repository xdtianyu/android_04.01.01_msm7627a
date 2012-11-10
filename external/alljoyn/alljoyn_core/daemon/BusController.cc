/**
 * @file
 *
 * BusController is responsible for responding to standard DBus messages
 * directed at the bus itself.
 */

/******************************************************************************
 *
 *
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

#include "BusController.h"
#include "DaemonRouter.h"
#include "BusInternal.h"

#define QCC_MODULE "ALLJOYN_DAEMON"

using namespace std;
using namespace qcc;
using namespace ajn;

BusController::BusController(Bus& alljoynBus) :
    bus(alljoynBus),
    dbusObj(bus, this),
    alljoynObj(bus, this),
#ifndef NDEBUG
    alljoynDebugObj(bus, this),
#endif
    initComplete(NULL)

{
    DaemonRouter& router(reinterpret_cast<DaemonRouter&>(bus.GetInternal().GetRouter()));
    router.SetBusController(this);
}

BusController::~BusController()
{
    DaemonRouter& router(reinterpret_cast<DaemonRouter&>(bus.GetInternal().GetRouter()));
    router.SetBusController(NULL);
}

QStatus BusController::Init(const qcc::String& listenSpecs)
{
    QStatus status;
    qcc::Event initEvent;

    initComplete = &initEvent;

    /*
     * Start the object initialization chain (see ObjectRegistered callback below)
     */
    status = dbusObj.Init();
    if (ER_OK != status) {
        QCC_LogError(status, ("DBusObj::Init failed"));
    } else {
        if (status == ER_OK) {
            status = bus.Start();
        }
        if (status == ER_OK) {
            status = Event::Wait(initEvent);
        }
        if (status == ER_OK) {
            status = bus.StartListen(listenSpecs.c_str());
            if (status != ER_OK) {
                bus.Stop();
                bus.Join();
            }
        }
    }

    initComplete = NULL;
    return status;
}

/*
 * The curious code below is to force various bus objects to be registered in order:
 *
 * /org/freedesktop/DBus
 * /org/alljoyn/Bus
 * /org/alljoyn/Debug
 *
 * The last one is optional and only registered for debug builds
 */
void BusController::ObjectRegistered(BusObject* obj)
{
    QStatus status;

    if (obj == &dbusObj) {
        status = alljoynObj.Init();
        if (status == ER_OK) {
            return;
        }
        QCC_LogError(status, ("alljoynObj::Init failed"));
    }
#ifndef NDEBUG
    if (obj == &alljoynObj) {
        status = alljoynDebugObj.Init();
        if (status == ER_OK) {
            return;
        }
        QCC_LogError(status, ("alljoynDebugObj::Init failed"));
    }
#endif
    if (initComplete) {
        initComplete->SetEvent();
    }
}
