/**
 * @file
 * Platform independent methods for DaemonTransport.
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

#include <errno.h>

#include <qcc/platform.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>

#include "BusInternal.h"
#include "RemoteEndpoint.h"
#include "Router.h"
#include "DaemonTransport.h"

#define QCC_MODULE "DAEMON_TRANSPORT"

using namespace std;
using namespace qcc;

namespace ajn {


DaemonTransport::DaemonTransport(BusAttachment& bus)
    : Thread("DaemonTransport"), bus(bus), stopping(false)
{
    /*
     * We know we are daemon code, so we'd better be running with a daemon
     * router.  This is assumed elsewhere.
     */
    assert(bus.GetInternal().GetRouter().IsDaemon());
}

DaemonTransport::~DaemonTransport()
{
    Stop();
    Join();
}

QStatus DaemonTransport::Start()
{
    stopping = false;
    return ER_OK;
}

QStatus DaemonTransport::Stop(void)
{
    stopping = true;

    /*
     * Tell the server accept loop thread to shut down through the thread
     * base class.
     */
    QStatus status = Thread::Stop();
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonDaemonTransport::Stop(): Failed to Stop() server thread"));
        return status;
    }

    endpointListLock.Lock(MUTEX_CONTEXT);

    /*
     * Ask any running endpoints to shut down and exit their threads.
     */
    for (list<RemoteEndpoint*>::iterator i = endpointList.begin(); i != endpointList.end(); ++i) {
        (*i)->Stop();
    }

    endpointListLock.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

QStatus DaemonTransport::Join(void)
{
    /*
     * Wait for the server accept loop thread to exit.
     */
    QStatus status = Thread::Join();
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonTransport::Join(): Failed to Join() server thread"));
        return status;
    }

    /*
     * A call to Stop() above will ask all of the endpoints to stop.  We still
     * need to wait here until all of the threads running in those endpoints
     * actually stop running.  When a remote endpoint thead exits the endpoint
     * will call back into our EndpointExit() and have itself removed from the
     * list.  We poll for the all-exited condition, yielding the CPU to let
     * the endpoint therad wake and exit.
     */
    endpointListLock.Lock(MUTEX_CONTEXT);
    while (endpointList.size() > 0) {
        endpointListLock.Unlock(MUTEX_CONTEXT);
        qcc::Sleep(50);
        endpointListLock.Lock(MUTEX_CONTEXT);
    }
    endpointListLock.Unlock(MUTEX_CONTEXT);

    stopping = false;

    return ER_OK;
}

void DaemonTransport::EndpointExit(RemoteEndpoint* ep)
{
    /*
     * This is a callback driven from the remote endpoint thread exit function.
     * Our DaemonEndpoint inherits from class RemoteEndpoint and so when
     * either of the threads (transmit or receive) of one of our endpoints exits
     * for some reason, we get called back here.
     */
    QCC_DbgTrace(("DaemonTransport::EndpointExit()"));

    /* Remove the dead endpoint from the live endpoint list */
    endpointListLock.Lock(MUTEX_CONTEXT);
    list<RemoteEndpoint*>::iterator i = find(endpointList.begin(), endpointList.end(), ep);
    if (i != endpointList.end()) {
        endpointList.erase(i);
    } else {
        QCC_LogError(ER_FAIL, ("DaemonTransport::EndpointExit() endpoint missing from endpointList"));
    }
    endpointListLock.Unlock(MUTEX_CONTEXT);

    delete ep;
}

} // namespace ajn
