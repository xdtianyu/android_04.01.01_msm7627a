/**
 * @file
 * ClientTransport is the transport mechanism between a client and the daemon
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

#ifndef _ALLJOYN_CLIENTTRANSPORT_H
#define _ALLJOYN_CLIENTTRANSPORT_H

#ifndef __cplusplus
#error Only include ClientTransport.h in C++ code.
#endif

#include <Status.h>

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Mutex.h>
#include <qcc/Socket.h>
#include <qcc/Thread.h>
#include <qcc/SocketStream.h>
#include <qcc/time.h>

#include "Transport.h"
#include "RemoteEndpoint.h"

namespace ajn {

/**
 * @brief A class for Client Transports used in AllJoyn clients and services.
 *
 * The ClientTransport class has different incarnations depending on the platform.
 */
class ClientTransport : public Transport, public RemoteEndpoint::EndpointListener {

  public:
    /**
     * Create a Client based transport for use by clients and services.
     *
     * @param bus The BusAttachment associated with this endpoint
     */
    ClientTransport(BusAttachment& bus);

    /**
     * Destructor
     */
    virtual ~ClientTransport();

    /**
     * Start the transport and associate it with a router.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Start();

    /**
     * Stop the transport.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Stop();

    /**
     * Pend the caller until the transport stops.
     *
     * @return
     *      - ER_OK if successful
     *      - an error status otherwise.
     */
    QStatus Join();

    /**
     * Determine if this transport is running. Running means Start() has been called.
     *
     * @return  Returns true if the transport is running.
     */
    bool IsRunning() { return m_running; }

    /**
     * Get the transport mask for this transport
     *
     * @return the TransportMask for this transport.
     */
    TransportMask GetTransportMask() const { return TRANSPORT_LOCAL; }

    /**
     * Normalize a transport specification.
     * Given a transport specification, convert it into a form which is guaranteed to have a one-to-one
     * relationship with a transport.
     *
     * @param inSpec    Input transport connect spec.
     * @param outSpec   Output transport connect spec.
     * @param argMap    Parsed parameter map.
     *
     * @return ER_OK if successful.
     */
    QStatus NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, std::map<qcc::String, qcc::String>& argMap) const;

    /**
     * Connect to a specified remote AllJoyn/DBus address.
     *
     * @param connectSpec    Transport specific key/value args used to configure the client-side endpoint.
     *                       The form of this string is @c "<transport>:<key1>=<val1>,<key2>=<val2>..."
     * @param opts           Requested sessions opts.
     * @param newep          [OUT] Endpoint created as a result of successful connect.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Connect(const char* connectSpec, const SessionOpts& opts, BusEndpoint** newep);

    /**
     * Disconnect from a specified AllJoyn/DBus address.
     *
     * @param connectSpec    The connectSpec used in Connect.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Disconnect(const char* connectSpec);

    /**
     * Set a listener for transport related events.  There can only be one
     * listener set at a time. Setting a listener implicitly removes any
     * previously set listener.
     *
     * @param listener  Listener for transport related events.
     */
    void SetListener(TransportListener* listener) { m_listener = listener; }

    /**
     * Returns the name of this transport
     */
    const char* GetTransportName() const { return TransportName; }

    /**
     * Indicates whether this transport is used for client-to-bus or bus-to-bus connections.
     *
     * @return  Always returns false, ClientTransports are only used to connect to a local daemon.
     */
    bool IsBusToBus() const { return false; }

    /**
     * Name of transport used in transport specs.
     */
    static const char* TransportName;

    /**
     * Returns true if a client transport is available on this platform. Some platforms only support
     * a bundled daemon so don't have a client transport. Transports must have names so if the
     * transport has no name it is not available.
     */
    static bool IsAvailable() { return TransportName != NULL; }

    /**
     * Callback for ClientEndpoint exit.
     *
     * @param endpoint   ClientEndpoint instance that has exited.
     */
    void EndpointExit(RemoteEndpoint* endpoint);

  private:
    BusAttachment& m_bus;           /**< The message bus for this transport */
    bool m_running;                 /**< True after Start() has been called, before Stop() */
    bool m_stopping;                /**< True if Stop() has been called but endpoints still exist */
    TransportListener* m_listener;  /**< Registered TransportListener */
    RemoteEndpoint* m_endpoint;     /**< The active endpoint */
    qcc::Mutex m_epLock;            /**< Lock to prevent the endpoint from being destroyed while it is being stopped */
};

} // namespace ajn

#endif // _ALLJOYN_CLIENTTRANSPORT_H
