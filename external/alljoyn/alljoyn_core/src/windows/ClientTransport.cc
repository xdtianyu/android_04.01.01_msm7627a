/**
 * @file
 * ClientTransportBase is a partial specialization of Transport that listens
 * on a TCP socket.
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

#include <qcc/IPAddress.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include <alljoyn/BusAttachment.h>

#include "BusInternal.h"
#include "RemoteEndpoint.h"
#include "Router.h"
#include "ClientTransport.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

const char* ClientTransport::TransportName = "tcp";

class ClientEndpoint : public RemoteEndpoint {
  public:
    ClientEndpoint(ClientTransport* transport, BusAttachment& bus, const qcc::String connectSpec,
                   qcc::SocketFd sock, const qcc::IPAddress& ipAddr, uint16_t port)
        :
        RemoteEndpoint(bus, false, connectSpec, &m_stream, ClientTransport::TransportName),
        m_transport(transport),
        m_stream(sock),
        m_ipAddr(ipAddr),
        m_port(port)
    { }

    ~ClientEndpoint() { }

    const qcc::IPAddress& GetIPAddress() { return m_ipAddr; }
    uint16_t GetPort() { return m_port; }

  protected:
    ClientTransport* m_transport;
    qcc::SocketStream m_stream;
    qcc::IPAddress m_ipAddr;
    uint16_t m_port;
};

QStatus ClientTransport::NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, map<qcc::String, qcc::String>& argMap) const
{
    /*
     * Take the string in inSpec, which must start with "tcp:" and parse it,
     * looking for comma-separated "key=value" pairs and initialize the
     * argMap with those pairs.
     */
    QStatus status = ParseArguments("tcp", inSpec, argMap);
    if (status != ER_OK) {
        return status;
    }

    /*
     * We need to return a map with all of the configuration items set to
     * valid values and a normalized string with the same.  For a client or
     * service TCP, we need a valid "addr" key.
     */
    map<qcc::String, qcc::String>::iterator i = argMap.find("addr");
    if (i == argMap.end()) {
        return ER_FAIL;
    } else {
        /*
         * We have a value associated with the "addr" key.  Run it through
         * a conversion function to make sure it's a valid value.
         */
        IPAddress addr;
        status = addr.SetAddress(i->second);
        if (status == ER_OK) {
            i->second = addr.ToString();
            outSpec = "tcp:addr=" + i->second;
        } else {
            return ER_BUS_BAD_TRANSPORT_ARGS;
        }
    }

    /*
     * For a client or service TCP, we need a valid "port" key.
     */
    i = argMap.find("port");
    if (i == argMap.end()) {
        return ER_FAIL;
    } else {
        /*
         * We have a value associated with the "port" key.  Run it through
         * a conversion function to make sure it's a valid value.
         */
        uint32_t port = StringToU32(i->second);
        if (port > 0 && port <= 0xffff) {
            i->second = U32ToString(port);
            outSpec += ",port=" + i->second;
        } else {
            return ER_BUS_BAD_TRANSPORT_ARGS;
        }
    }

    return ER_OK;
}

QStatus ClientTransport::Connect(const char* connectSpec, const SessionOpts& opts, BusEndpoint** newep)
{
    QCC_DbgHLPrintf(("ClientTransport::Connect(): %s", connectSpec));

    /*
     * Don't bother trying to create a new endpoint if the state precludes them.
     */
    if (m_running == false || m_stopping == true) {
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }
    if (m_endpoint) {
        return ER_BUS_ALREADY_CONNECTED;
    }

    /*
     * Parse and normalize the connectArgs.  For a client or service, there are
     * no reasonable defaults and so the addr and port keys MUST be present or
     * an error is returned.
     */
    QStatus status;
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    status = NormalizeTransportSpec(connectSpec, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("ClientTransport::Connect(): Invalid TCP connect spec \"%s\"", connectSpec));
        return status;
    }

    IPAddress ipAddr(argMap["addr"]);            // Guaranteed to be there.
    uint16_t port = StringToU32(argMap["port"]); // Guaranteed to be there.

    /*
     * Attempt to connect to the remote TCP address and port specified in the connectSpec.
     */
    SocketFd sockFd = -1;
    status = Socket(QCC_AF_INET, QCC_SOCK_STREAM, sockFd);
    if (status != ER_OK) {
        QCC_LogError(status, ("ClientTransport(): socket Create() failed"));
        return status;
    }
    /*
     * Got a socket, now Connect() to the remote address and port.
     */
    status = qcc::Connect(sockFd, ipAddr, port);
    if (status != ER_OK) {
        QCC_DbgHLPrintf(("ClientTransport(): socket Connect() failed %s", QCC_StatusText(status)));
        qcc::Close(sockFd);
        return status;
    }
    /*
     * We have a connection established, but DBus wire protocol requires that every connection,
     * irrespective of transport, start with a single zero byte. This is so that the Unix-domain
     * socket transport used by DBus can pass SCM_RIGHTS out-of-band when that byte is sent.
     */
    uint8_t nul = 0;
    size_t sent;

    status = Send(sockFd, &nul, 1, sent);
    if (status != ER_OK) {
        QCC_LogError(status, ("ClientTransport::Connect(): Failed to send initial NUL byte"));
        qcc::Close(sockFd);
        return status;
    }
    /*
     * The underlying transport mechanism is started, but we need to create a
     * ClientEndpoint object that will orchestrate the movement of data across the
     * transport.
     */
    if (m_stopping) {
        status = ER_BUS_TRANSPORT_NOT_STARTED;
    } else {
        m_endpoint = new ClientEndpoint(this, m_bus, normSpec, sockFd, ipAddr, port);

        /* Initialized the features for this endpoint */
        m_endpoint->GetFeatures().isBusToBus = false;
        m_endpoint->GetFeatures().allowRemote = m_bus.GetInternal().AllowRemoteMessages();
        m_endpoint->GetFeatures().handlePassing = true;

        qcc::String authName;
        qcc::String redirection;
        status = m_endpoint->Establish("ANONYMOUS", authName, redirection);
        if (status == ER_OK) {
            m_endpoint->SetListener(this);
            status = m_endpoint->Start();
            if (status != ER_OK) {
                QCC_LogError(status, ("ClientTransport::Connect(): Start ClientEndpoint failed"));
            }
        }
    }
    /*
     * If we got an error, we need to cleanup the socket and zero out the
     * returned endpoint.  If we got this done without a problem, we return
     * a pointer to the new endpoint.
     */
    if (status != ER_OK) {
        m_stopping = true;
        if (m_endpoint) {
            delete m_endpoint;
            m_endpoint = NULL;
        }
        qcc::Shutdown(sockFd);
        qcc::Close(sockFd);
    }
    if (newep) {
        *newep = m_endpoint;
    }
    return status;
}

} // namespace ajn
