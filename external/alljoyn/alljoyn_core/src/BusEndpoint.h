#ifndef _ALLJOYN_BUSENDPOINT_H
#define _ALLJOYN_BUSENDPOINT_H
/**
 * @file This file defines the class for handling the client and server
 * endpoints for the message bus wire protocol
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

#include <qcc/GUID.h>
#include <qcc/String.h>

#include <alljoyn/Message.h>
#include <alljoyn/MessageSink.h>

#include <Status.h>

namespace ajn {

/**
 * Base class for all types of Bus endpoints
 */
class BusEndpoint : public MessageSink {
  public:

    /**
     * BusEndpoint type.
     */
    typedef enum {
        ENDPOINT_TYPE_NULL,
        ENDPOINT_TYPE_LOCAL,
        ENDPOINT_TYPE_REMOTE,
        ENDPOINT_TYPE_BUS2BUS,
        ENDPOINT_TYPE_VIRTUAL
    } EndpointType;

    /**
     * Constructor.
     *
     * @param type    BusEndpoint type.
     */
    BusEndpoint(EndpointType type) : endpointType(type), disconnectStatus(ER_OK) { }

    /**
     * Virtual destructor for derivable class.
     */
    virtual ~BusEndpoint() { }

    /**
     * Push a message into the endpoint
     *
     * @param msg   Message to send.
     *
     * @return ER_OK if successful
     */
    virtual QStatus PushMessage(Message& msg) = 0;

    /**
     * Get the endpoint's unique name.
     *
     * @return  Unique name for endpoint.
     */
    virtual const qcc::String& GetUniqueName() const = 0;

    /**
     * Get the unique name of the endpoint's local controller object.
     *
     * @return  Unique name for endpoint's controller.
     */
    qcc::String GetControllerUniqueName() const;

    /**
     * Return the user id of the endpoint.
     *
     * @return  User ID number.
     */
    virtual uint32_t GetUserId() const = 0;

    /**
     * Return the group id of the endpoint.
     *
     * @return  Group ID number.
     */
    virtual uint32_t GetGroupId() const = 0;

    /**
     * Return the process id of the endpoint.
     *
     * @return  Process ID number.
     */
    virtual uint32_t GetProcessId() const = 0;

    /**
     * Indicates if the endpoint supports reporting UNIX style user, group, and process IDs.
     *
     * @return  'true' if UNIX IDs supported, 'false' if not supported.
     */
    virtual bool SupportsUnixIDs() const = 0;

    /**
     * Get endpoint type.
     *
     * @return EndpointType
     */
    EndpointType GetEndpointType() { return endpointType; }

    /**
     * Return true if this endpoint is allowed to receive messages from remote (bus-to-bus) endpoints.
     *
     * @return  true iff endpoint is allowed to receive messages from remote (bus-to-bus) endpoints.
     */
    virtual bool AllowRemoteMessages() = 0;

    /**
     * Return true if the endpoint was disconnected due to an error rather than a clean shutdown.
     */
    bool SurpriseDisconnect() { return disconnectStatus != ER_OK; }

  protected:

    EndpointType endpointType;   /**< Type of endpoint */
    QStatus disconnectStatus;    /**< Reason for the disconnect */
};

}

#endif
