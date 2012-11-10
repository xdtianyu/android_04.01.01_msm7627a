/**
 * @file
 * ClientRouter is responsible for routing Bus messages between a single remote
 * endpoint and a single local endpoint
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
#ifndef _ALLJOYN_CLIENTROUTER_H
#define _ALLJOYN_CLIENTROUTER_H

#include <qcc/platform.h>

#include <qcc/Thread.h>
#include <qcc/String.h>

#include "Router.h"
#include "LocalTransport.h"

#include <Status.h>

namespace ajn {

/**
 * %ClientRouter is responsible for routing Bus messages between a single remote
 * endpoint and a single local endpoint
 */
class ClientRouter : public Router, public qcc::AlarmListener {
    friend class TCPEndpoint;
    friend class UnixEndpoint;
    friend class LocalEndpoint;

  public:
    /**
     * Constructor
     */
    ClientRouter() : localEndpoint(NULL), nonLocalEndpoint(NULL) { }

    /**
     * Route an incoming Message Bus Message from an endpoint.
     *
     * @param sender  Endpoint that is sending the message
     * @param msg     Message to be processed.
     * @return
     *      - ER_OK if successful
     *      - ER_BUS_NO_ENDPOINT if unable to find endpoint
     *      - An error status otherwise
     */
    QStatus PushMessage(Message& msg, BusEndpoint& sender);

    /**
     * Register an endpoint.
     *
     * This method must be called by an endpoint before attempting to use the router.
     *
     * @param endpoint   Endpoint being registered.
     * @param isLocal    true if endpoint is local.
     * @return ER_OK if successful.
     */
    QStatus RegisterEndpoint(BusEndpoint& endpoint, bool isLocal);

    /**
     * Un-register an endpoint.
     *
     * This method must be called by an endpoint before the endpoint is deallocted.
     *
     * @param endpoint   Endpoint being un-registered.
     */
    void UnregisterEndpoint(BusEndpoint& endpoint);

    /**
     * Find the endpoint that owns the given unique or well-known name.
     *
     * @param busname    Unique or well-known bus name
     * @return
     *      - Matching endpoint
     *      - NULL if none exists.
     */
    BusEndpoint* FindEndpoint(const qcc::String& busname);

    /**
     * Generate a unique endpoint name.
     *
     * This method is not used for client-side Bus instances. An empty string is returned.
     *
     * @return An empty string.
     */
    qcc::String GenerateUniqueName(void) { return ""; }

    /**
     * Indicate that this is not a daemon bus instance.
     *
     * @return false since the client router is never part of a daemon.
     */
    bool IsDaemon() const { return false; }

    /**
     * Return true if this router is in contact with a bus (either locally or remotely)
     *
     * This method can be used to determine whether messages sent to "the bus" will be routed.
     *
     * @return true iff the messages can be routed currently.
     */
    bool IsBusRunning(void) const { return localEndpoint && nonLocalEndpoint; }

    /**
     * Set the global GUID of the bus.
     * The global GUID is not used/needed for client-side routing.
     *
     * @param guid   GUID of bus associated with this router.
     */
    void SetGlobalGUID(const qcc::GUID128& guid) { }

    /**
     * Destructor
     */
    ~ClientRouter();

  private:
    LocalEndpoint* localEndpoint;   /**< Local endpoint */
    BusEndpoint* nonLocalEndpoint;  /**< Last non-local enpoint to register */

    /*
     * Used to report connection is up.
     */
    void AlarmTriggered(const qcc::Alarm& alarm, QStatus reason);

};

}

#endif
