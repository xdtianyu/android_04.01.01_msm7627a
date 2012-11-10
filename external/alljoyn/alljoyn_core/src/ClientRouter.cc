/**
 * @file
 * ClientRouter is a simplified ("client-side only") router that is capable
 * of routing messages between a single remote and a single local endpoint.
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

#include <qcc/Debug.h>
#include <qcc/Util.h>
#include <qcc/String.h>

#include <Status.h>

#include "Transport.h"
#include "BusEndpoint.h"
#include "LocalTransport.h"
#include "ClientRouter.h"
#include "BusInternal.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

void ClientRouter::AlarmTriggered(const qcc::Alarm& alarm, QStatus reason)
{
    if (localEndpoint && nonLocalEndpoint) {
        localEndpoint->BusIsConnected();
    }
}

QStatus ClientRouter::PushMessage(Message& msg, BusEndpoint& sender)
{
    QStatus status = ER_OK;

    if (!localEndpoint || !nonLocalEndpoint) {
        status = ER_BUS_NO_ENDPOINT;
    } else {
        if (&sender == localEndpoint) {
            status = nonLocalEndpoint->PushMessage(msg);
        } else {
            status = localEndpoint->PushMessage(msg);
        }
    }

    if (ER_OK != status) {
        QCC_DbgHLPrintf(("ClientRouter::PushMessage failed: %s", QCC_StatusText(status)));
    }
    return status;
}

QStatus ClientRouter::RegisterEndpoint(BusEndpoint& endpoint, bool isLocal)
{
    bool hadNonLocal = (NULL != nonLocalEndpoint);

    QCC_DbgHLPrintf(("ClientRouter::RegisterEndpoint"));

    /* Keep track of local and (at least one) non-local endpoint */
    if (isLocal) {
        localEndpoint = static_cast<LocalEndpoint*>(&endpoint);
    } else {
        nonLocalEndpoint = &endpoint;
    }

    /* Local and non-local endpoints must have the same unique name */
    if ((isLocal && nonLocalEndpoint) || (!isLocal && localEndpoint && !hadNonLocal)) {
        localEndpoint->SetUniqueName(nonLocalEndpoint->GetUniqueName());
    }

    /* Notify local endpoint via an alarm if we have both a local and at least one non-local endpoint */
    if (localEndpoint && nonLocalEndpoint && (isLocal || !hadNonLocal)) {
        Alarm connectAlarm(0, this, 0, NULL);
        localEndpoint->GetBus().GetInternal().GetTimer().AddAlarm(connectAlarm);
    }
    return ER_OK;
}

void ClientRouter::UnregisterEndpoint(BusEndpoint& endpoint)
{
    QCC_DbgHLPrintf(("ClientRouter::UnregisterEndpoint"));

    /* Unregister static endpoints */
    if (&endpoint == localEndpoint) {
        /*
         * Let the bus know that the local endpoint disconnected
         */
        localEndpoint->GetBus().GetInternal().LocalEndpointDisconnected();
        localEndpoint = NULL;
    } else if (&endpoint == nonLocalEndpoint) {
        nonLocalEndpoint = NULL;
    }
}

BusEndpoint* ClientRouter::FindEndpoint(const qcc::String& busName)
{
    return nonLocalEndpoint ? nonLocalEndpoint : NULL;
}

ClientRouter::~ClientRouter()
{
    QCC_DbgHLPrintf(("ClientRouter::~ClientRouter()"));
}


}
