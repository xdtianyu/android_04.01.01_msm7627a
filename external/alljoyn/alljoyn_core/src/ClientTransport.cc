/**
 * @file
 * ClientTransport methods that are common to all implementations
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

#include <list>

#include <errno.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>

#include "BusInternal.h"
#include "RemoteEndpoint.h"
#include "Router.h"
#include "ClientTransport.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

ClientTransport::ClientTransport(BusAttachment& bus) : m_bus(bus), m_running(false), m_stopping(false), m_listener(0), m_endpoint(NULL)
{
}

ClientTransport::~ClientTransport()
{
    Stop();
    Join();
}

QStatus ClientTransport::Start()
{
    /*
     * Start() is defined in the underlying class Transport as a placeholder
     * for a method used to crank up a server accept loop.  We have no need
     * for such a loop, so we don't need to do anything but make a couple of
     * notes to ourselves that this method has been called.
     */
    m_running = true;
    m_stopping = false;

    return ER_OK;
}

QStatus ClientTransport::Stop(void)
{
    m_epLock.Lock();
    m_running = false;

    if (!m_stopping) {
        m_stopping = true;
        if (m_endpoint) {
            m_endpoint->Stop();
        }
    }
    m_epLock.Unlock();
    return ER_OK;
}

QStatus ClientTransport::Join(void)
{
    assert(m_stopping);
    /*
     * A call to Stop() above will ask all of the endpoint to stop.  We still need to wait here
     * until the endpoint actually stops running.  When the underlying remote endpoint stops it will
     * call back into EndpointExit() and remove itself from the list.  We poll until the end point
     * is removed.
     */
    while (m_endpoint) {
        qcc::Sleep(50);
    }
    return ER_OK;
}

void ClientTransport::EndpointExit(RemoteEndpoint* ep)
{
    assert(ep == m_endpoint);

    /*
     * This is a callback driven from the remote endpoint thread exit function.
     * Our ClientEndpoint inherits from class RemoteEndpoint and so when either of
     * the threads (transmit or receive) of one of our endpoints exits for some
     * reason, we get called back here.
     */
    QCC_DbgTrace(("ClientTransport::EndpointExit()"));

    /*
     * Grab the lock so we don't delete the endpoint while someone is calling Stop or Disconnect
     */
    m_epLock.Lock();
    m_endpoint = NULL;
    delete ep;
    m_epLock.Unlock();
}

QStatus ClientTransport::Disconnect(const char* connectSpec)
{
    QCC_DbgHLPrintf(("ClientTransport::Disconnect(): %s", connectSpec));

    if (!m_endpoint) {
        return ER_BUS_NOT_CONNECTED;
    }
    /*
     * Higher level code tells us which connection is refers to by giving us the
     * same connect spec it used in the Connect() call.  We have to determine the
     * address and port in exactly the same way
     */
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    QStatus status = ClientTransport::NormalizeTransportSpec(connectSpec, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("ClientTransport::Disconnect(): Invalid connect spec \"%s\"", connectSpec));
        return status;
    }
    /*
     * Stop the endpoint if it is not already being stopped
     */
    m_epLock.Lock();
    if (!m_stopping && m_endpoint) {
        m_endpoint->Stop();
    }
    m_epLock.Unlock();
    return status;
}

} // namespace ajn
