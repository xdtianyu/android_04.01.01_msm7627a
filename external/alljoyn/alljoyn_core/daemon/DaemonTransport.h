/**
 * @file
 * DaemonTransport is a specialization of class Transport for communication between an AllJoyn
 * client application and the daemon. This is the daemon's counterpart to ClientTransport.
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
#ifndef _ALLJOYN_DAEMONTRANSPORT_H
#define _ALLJOYN_DAEMONTRANSPORT_H

#ifndef __cplusplus
#error Only include DaemonTransport.h in C++ code.
#endif

#include <Status.h>

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Mutex.h>
#include <qcc/Thread.h>
#include <qcc/time.h>

#include "Transport.h"
#include "RemoteEndpoint.h"

namespace ajn {

/**
 * @brief A class for the daemon end of the client transport
 *
 * The DaemonTransport class has different incarnations depending on the platform.
 */
class DaemonTransport : public Transport, public RemoteEndpoint::EndpointListener, public qcc::Thread {

  public:
    /**
     * Create a transport to receive incoming connections from AllJoyn application.
     *
     * @param bus  The bus associated with this transport.
     */
    DaemonTransport(BusAttachment& bus);

    /**
     * Destructor
     */
    ~DaemonTransport();

    /**
     * Start the transport and associate it with the router.
     *
     * @return ER_OK if successful.
     */
    QStatus Start();

    /**
     * Stop the transport.
     *
     * @return ER_OK if successful.
     */
    QStatus Stop();

    /**
     * Pend the caller until the transport stops.
     * @return ER_OK if successful.
     */
    QStatus Join();

    /**
     * Determine if this transport is running. Running means Start() has been called.
     *
     * @return  Returns true if the transport is running.
     */
    bool IsRunning() { return Thread::IsRunning(); }

    /**
     * Get the transport mask for this transport
     *
     * @return the TransportMask for this transport.
     */
    TransportMask GetTransportMask() const { return TRANSPORT_LOCAL; }

    /**
     * @internal
     * @brief Normalize a transport specification.
     *
     * Given a transport specification, convert it into a form which is guaranteed to
     * have a one-to-one relationship with a connection instance.
     *
     * @param inSpec    Input transport connect spec.
     * @param outSpec   Output transport connect spec.
     * @param argMap    Parsed parameter map.
     *
     * @return ER_OK if successful.
     */
    QStatus NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, std::map<qcc::String, qcc::String>& argMap) const;

    /**
     * Start listening for incoming connections on a specified bus address.
     *
     * @param listenSpec  Transport specific key/value arguments that specify the physical interface to listen on.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus StartListen(const char* listenSpec);

    /**
     * @brief Stop listening for incoming connections on a specified bus address.
     *
     * This method cancels a StartListen request. Therefore, the listenSpec must
     * match previous call to StartListen().
     *
     * @param listenSpec  Transport specific key/value arguments that specify the physical interface to listen on.
     *
     * @return ER_OK if successful.
     */
    QStatus StopListen(const char* listenSpec);

    /**
     * Returns the name of this transport
     */
    const char* GetTransportName() const { return TransportName; }

    /**
     * Name of transport used in transport specs.
     */
    static const char* TransportName;

    /**
     * Indicates whether this transport is used for client-to-bus or bus-to-bus connections.
     *
     * @return  Always returns false, DaemonTransports are only used to accept connections from a local client.
     */
    bool IsBusToBus() const { return false; }

    /**
     * Callback for Daemon RemoteEndpoint exit.
     *
     * @param endpoint   Daemon RemoteEndpoint instance that has exited.
     */
    void EndpointExit(RemoteEndpoint* endpoint);

  private:
    BusAttachment& bus;                       /**< The message bus for this transport */
    bool stopping;                            /**< True if Stop() has been called but endpoints still exist */
    std::list<RemoteEndpoint*> endpointList;  /**< List of active endpoints */
    qcc::Mutex endpointListLock;              /**< Mutex that protects the endpoint list */

    /**
     * @internal
     * @brief Thread entry point.
     *
     * @param arg  Thread entry arg.
     */
    qcc::ThreadReturn STDCALL Run(void* arg);
};

} // namespace ajn

#endif // _ALLJOYN_DAEMONTRANSPORT_H
