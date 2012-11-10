/**
 * @file RendezvousServerConnection.h
 *
 * This file defines a class that handles the connection with the Rendezvous Server.
 *
 */

/******************************************************************************
 * Copyright 2012 Qualcomm Innovation Center, Inc.
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

#ifndef _RENDEZVOUSSERVERCONNECTION_H
#define _RENDEZVOUSSERVERCONNECTION_H

#include <qcc/platform.h>
#include <qcc/Socket.h>
#include <qcc/SocketTypes.h>
#include <qcc/String.h>
#include <qcc/Event.h>

#include "HttpConnection.h"
#include "NetworkInterface.h"

using namespace std;
using namespace qcc;

namespace ajn {
/*This class handles the connection with the Rendezvous Server.*/
class RendezvousServerConnection {

  public:

    /**
     * @internal
     * @brief Enum specifying the connections that need to be
     * established with the Rendezvous Server.
     */
    typedef enum _ConnectionFlag {

        /* Do not establish any connection */
        NONE = 0,

        /* Establish only the on demand connection */
        ON_DEMAND_CONNECTION,

        /* Establish only the persistent connection */
        PERSISTENT_CONNECTION,

        /* Establish both the connections */
        BOTH

    } ConnectionFlag;

    /**
     * @internal
     * @brief Constructor.
     */
    RendezvousServerConnection(String rdvzServer, bool enableIPv6, bool useHttp);

    /**
     * @internal
     * @brief Destructor.
     */
    ~RendezvousServerConnection();

    /**
     * @internal
     * @brief Connect to the Rendezvous Server after gathering the
     * latest interface details.
     *
     * @param connFlag - Flag specifying the type of connection to be set up
     * @param interfaceFlags - Interface flags specifying the permissible interface types for the connection
     *
     * @return ER_OK or ER_FAIL
     */
    QStatus Connect(uint8_t interfaceFlags, ConnectionFlag connFlag);

    /**
     * @internal
     * @brief Disconnect from the Rendezvous Server
     *
     * @return None
     */
    void Disconnect(void);

    /**
     * @internal
     * @brief Returns if the interface with the specified IPAddress is still live
     *
     * @return None
     */
    bool IsInterfaceLive(IPAddress interfaceAddr);

    /**
     * @internal
     * @brief Update the connection details
     *
     * @return None
     */
    void UpdateConnectionDetails(HttpConnection** oldHttpConn, HttpConnection* newHttpConn,
                                 bool* isConnected, bool* connectionChangedFlag);

    /**
     * @internal
     * @brief Clean up an HTTP connection
     *
     * @return None
     */
    void CleanConnection(HttpConnection* httpConn, bool* isConnected);

    /**
     * @internal
     * @brief Set up a HTTP connection
     *
     * @return ER_OK or ER_FAIL
     */
    QStatus SetupNewConnection(SocketFd& sockFd, HttpConnection** httpConn);

    /**
     * @internal
     * @brief Set up a HTTP connection with the Rendezvous Server
     *
     * @return ER_OK or ER_FAIL
     */
    QStatus SetupHTTPConn(SocketFd sockFd, HttpConnection** httpConn);

    /**
     * @internal
     * @brief Set up a socket for HTTP connection with the Rendezvous Server
     *
     * @return ER_OK or ER_FAIL
     */
    QStatus SetupSockForConn(SocketFd& sockFd);

    /**
     * @internal
     * @brief Set up a HTTP connection with the Rendezvous Server
     *
     * @param connFlag - Flag specifying the type of connection to be set up
     *
     * @return ER_OK or ER_FAIL
     */
    QStatus SetupConnection(ConnectionFlag connFlag);

    /**
     * @internal
     * @brief Function indicating if the on demand connection is up with the
     * Rendezvous Server.
     */
    bool IsOnDemandConnUp() { return onDemandIsConnected; };

    /**
     * @internal
     * @brief Function indicating if the persistent connection is up with the
     * Rendezvous Server.
     */
    bool IsPersistentConnUp() { return persistentIsConnected; };

    /**
     * @internal
     * @brief Function indicating if either or both the persistent and on demand connections are up with the
     * Rendezvous Server.
     */
    bool IsConnectedToServer() { return (onDemandIsConnected | persistentIsConnected); };

    /**
     * @internal
     * @brief Send a message to the Server
     */
    QStatus SendMessage(bool sendOverPersistentConn, HttpConnection::Method httpMethod, String uri, bool payloadPresent, String payload);

    /**
     * @internal
     * @brief Receive a response from the Server
     */
    QStatus FetchResponse(bool isOnDemandConnection, HttpConnection::HTTPResponse& response);

    /**
     * @internal
     * @brief Reset the persistentConnectionChanged flag
     */
    void ResetPersistentConnectionChanged(void) { persistentConnectionChanged = false; };

    /**
     * @internal
     * @brief Reset the onDemandConnectionChanged flag
     */
    void ResetOnDemandConnectionChanged(void) { onDemandConnectionChanged = false; };

    /**
     * @internal
     * @brief Return the value of persistentConnectionChanged flag
     */
    bool GetPersistentConnectionChanged(void) { return persistentConnectionChanged; };

    /**
     * @internal
     * @brief Return the value of onDemandConnectionChanged flag
     */
    bool GetOnDemandConnectionChanged(void) { return onDemandConnectionChanged; };

    /**
     * @internal
     * @brief Return onDemandConn
     */
    Event& GetOnDemandSourceEvent() { return onDemandConn->GetResponseSource().GetSourceEvent(); };

    /**
     * @internal
     * @brief Return persistentConn
     */
    Event& GetPersistentSourceEvent() { return persistentConn->GetResponseSource().GetSourceEvent(); };

    /**
     * @internal
     * @brief Return IPAddresses of the interfaces
     * over which the Persistent and the On Demand connections have
     * been setup with the Rendezvous Server.
     */
    void GetRendezvousConnIPAddresses(IPAddress& onDemandAddress, IPAddress& persistentAddress);

  private:

    /**
     * @internal
     * @brief Boolean indicating if the on demand connection is up.
     */
    bool onDemandIsConnected;

    /**
     * @internal
     * @brief The HTTP connection that is used to send messages to the
     * Rendezvous Server.
     */
    HttpConnection* onDemandConn;

    /**
     * @internal
     * @brief Boolean indicating if the persistent connection is up.
     */
    bool persistentIsConnected;

    /**
     * @internal
     * @brief Boolean indicating if the persistent connection has changed from
     * what it was previously.
     */
    bool persistentConnectionChanged;

    /**
     * @internal
     * @brief Boolean indicating if the On Demand connection has changed from
     * what it was previously.
     */
    bool onDemandConnectionChanged;

    /**
     * @internal
     * @brief The HTTP connection that is used to send GET messages to the
     * Rendezvous Server and receive responses from the same.
     */
    HttpConnection* persistentConn;

    /**
     * @internal
     * @brief Interface object used to get the network information from the kernel.
     */
    NetworkInterface* networkInterface;

    /**
     * @internal
     * @brief Rendezvous Server address.
     */
    String RendezvousServer;

    /**
     * @internal
     * @brief Boolean indicating if IPv6 addressing mode is supported.
     */
    bool EnableIPv6;

    /**
     * @internal
     * @brief Boolean indicating if HTTP protocol needs to be used for connection.
     */
    bool UseHTTP;
};

} //namespace ajn


#endif //_RENDEZVOUSSERVERCONNECTION_H
