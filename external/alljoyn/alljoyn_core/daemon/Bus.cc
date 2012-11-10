/**
 * @file
 *
 * Bus is the top-level object responsible for implementing the message bus.
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
#include <qcc/Debug.h>
#include <qcc/String.h>

#include <assert.h>

#include "Bus.h"
#include "DaemonRouter.h"
#include "TransportList.h"

#define QCC_MODULE "ALLJOYN_DAEMON"

using namespace ajn;
using namespace qcc;
using namespace std;

/*
 * Set the number of concurrent method and signal handlers on our local endpoint
 * to four.
 */
const uint32_t EP_CONCURRENCY = 4;

Bus::Bus(const char* applicationName, TransportFactoryContainer& factories, const char* listenSpecs) :
    BusAttachment(new Internal(applicationName, *this, factories, new DaemonRouter, true, listenSpecs), EP_CONCURRENCY),
    busListener(NULL)
{
    GetInternal().GetRouter().SetGlobalGUID(GetInternal().GetGlobalGUID());
}

QStatus Bus::StartListen(const qcc::String& listenSpec, bool& listening)
{
    QStatus status;

    /** Get the transport */
    Transport* trans = GetInternal().GetTransportList().GetTransport(listenSpec);
    if (trans) {
        status = trans->StartListen(listenSpec.c_str());
        if (ER_OK == status) {
            if (trans->IsBusToBus()) {
                if (!externalAddrs.empty()) {
                    externalAddrs += ';';
                }
                externalAddrs += listenSpec + ",guid=" + GetInternal().GetGlobalGUID().ToString();
            } else {
                if (!localAddrs.empty()) {
                    localAddrs += ';';
                }
                localAddrs += listenSpec + ",guid=" + GetInternal().GetGlobalGUID().ToString();
            }
            listening = true;
        }
    } else {
        status = ER_BUS_TRANSPORT_NOT_AVAILABLE;
    }
    return status;
}

QStatus Bus::StartListen(const char* listenSpecs)
{
    QStatus status(ER_OK);
    qcc::String specs = listenSpecs;
    bool listening = false;

    if (!IsStarted()) {
        status = ER_BUS_BUS_NOT_STARTED;
    } else {
        size_t pos = 0;
        while (qcc::String::npos != pos) {
            size_t endPos = specs.find_first_of(';', pos);
            qcc::String spec((qcc::String::npos == endPos) ? specs.substr(pos) : specs.substr(pos, endPos - pos));
            QStatus s(StartListen(spec, listening));
            if (status == ER_OK) {
                status = s;
            }

            pos = ((qcc::String::npos == endPos) || (specs.size() <= endPos + 1)) ? qcc::String::npos : endPos + 1;
        }
        /*
         * BusAttachment needs to be listening on at least one transport
         */
        if (listening) {
            status = ER_OK;
        } else {
            status = ER_BUS_NO_TRANSPORTS;
        }
    }
    if (status != ER_OK) {
        QCC_LogError(status, ("BusAttachment::StartListen failed"));
    }
    return status;
}

QStatus Bus::StopListen(const char* listenSpecs)
{
    QStatus status(ER_OK);
    qcc::String specs = listenSpecs;

    if (!IsStarted()) {
        status = ER_BUS_BUS_NOT_STARTED;
        QCC_LogError(status, ("BusAttachment::StopListen() failed"));
    } else {
        size_t pos = 0;
        while (qcc::String::npos != pos) {
            size_t endPos = specs.find_first_of(';', pos);
            qcc::String spec = (qcc::String::npos == endPos) ? specs.substr(pos) : specs.substr(pos, endPos - pos);

            QStatus s = ER_BUS_TRANSPORT_NOT_AVAILABLE;
            Transport* trans = GetInternal().GetTransportList().GetTransport(spec);
            if (trans) {
                s = trans->StopListen(spec.c_str());
                if (ER_OK != s) {
                    QCC_LogError(s, ("Transport::StopListen failed"));
                }
                // TODO - remove spec from localAddrs and externalAddrs???
            }

            if (status == ER_OK) {
                status = s;
            }

            pos = ((qcc::String::npos == endPos) || (specs.size() <= endPos + 1)) ? qcc::String::npos : endPos + 1;
        }
    }
    return status;
}

void Bus::RegisterBusListener(BusListener& listener)
{
    busListener = &listener;
    /*
     * The bus gets name changed callbacks from the daemon router.
     */
    reinterpret_cast<DaemonRouter&>(GetInternal().GetRouter()).AddBusNameListener(this);
}

void Bus::UnregisterBusListener(BusListener& listener) {
    if (&listener == busListener) {
        busListener = NULL;
        reinterpret_cast<DaemonRouter&>(GetInternal().GetRouter()).RemoveBusNameListener(this);
    }
}

void Bus::NameOwnerChanged(const qcc::String& alias, const qcc::String* oldOwner, const qcc::String* newOwner)
{
    BusListener* listener = busListener;
    if (listener) {
        listener->NameOwnerChanged(alias.c_str(), oldOwner ? oldOwner->c_str() : NULL, newOwner ? newOwner->c_str() : NULL);
    }
}

