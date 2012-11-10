/**
 * @file
 * Router is responsible for routing Bus messages between one or more AllJoynTransports(s)
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
#ifndef _ALLJOYN_ROUTER_H
#define _ALLJOYN_ROUTER_H

#include <qcc/platform.h>
#include <qcc/String.h>
#include "Transport.h"
#include "BusEndpoint.h"


namespace ajn {

/**
 * %Router defines an interface that describes how to route messages between two
 * or more endpoints.
 */
class Router {
  public:

    /**
     * Destructor
     */
    virtual ~Router() { }

    /**
     * Route an incoming Message Bus Message from an endpoint.
     *
     * @param sender  Endpoint that is sending the message
     * @param msg     Message to be processed.
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise.
     */
    virtual QStatus PushMessage(Message& msg, BusEndpoint& sender) = 0;

    /**
     * Register an endpoint.
     * This method must be called by an endpoint before attempting to use the router.
     *
     * @param endpoint   Endpoint being registered.
     * @param isLocal    true if endpoint is local.
     * @return ER_OK if successful.
     */
    virtual QStatus RegisterEndpoint(BusEndpoint& endpoint, bool isLocal) = 0;

    /**
     * Un-register an endpoint.
     * This method must be called by an endpoint before the endpoint is deallocted.
     *
     * @param endpoint   Endpoint being registered.
     */
    virtual void UnregisterEndpoint(BusEndpoint& endpoint) = 0;

    /**
     * Find the endpoint that owns the given unique or well-known name.
     *
     * @param busname    Unique or well-known bus name
     * @return
     *      - The matching endpoint
     *      - NULL if none exists.
     */
    virtual BusEndpoint* FindEndpoint(const qcc::String& busname) = 0;

    /**
     * Generate a unique endpoint name.
     * This method is not used by non-daemon instnces of the router.
     * An empty string is returned if called in this case.
     *
     * @return A unique bus name that can be assigned to a (server-side) endpoint.
     */
    virtual qcc::String GenerateUniqueName(void) = 0;

    /**
     * Return true if this router is in contact with a bus (either locally or remotely)
     * This method can be used to determine whether messages sent to "the bus" will be routed.
     *
     * @return @b true if the messages can be routed currently.
     */
    virtual bool IsBusRunning(void) const = 0;

    /**
     * Determine whether this is an AllJoyn daemon process.
     *
     * @return true iff this bus instance is an AllJoyn daemon process.
     */
    virtual bool IsDaemon() const = 0;

    /**
     * Set the global GUID of the bus.
     *
     * @param guid   GUID of bus associated with this router.
     */
    virtual void SetGlobalGUID(const qcc::GUID128& guid) = 0;
};

}

#endif
