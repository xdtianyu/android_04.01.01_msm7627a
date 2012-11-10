/**
 * @file
 *
 * SSL stream-based socket interface
 *
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
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

#ifndef _SSLSOCKET_H
#define _SSLSOCKET_H

#include <qcc/platform.h>
#include <qcc/Stream.h>
#include <qcc/String.h>
#include <qcc/Event.h>

#include "Status.h"

namespace qcc {
/**
 * SSL Socket
 */
class SslSocket : public Stream {

  public:

    /** Construct an SSL socket. */
    SslSocket(String host);

    /** Destroy SSL socket */
    ~SslSocket();

    /**
     * Connect an SSL socket to a remote host on a specified port
     *
     * @param hostname     Destination IP address or hostname.
     * @param port         IP Port on remote host.
     *
     * @return  ER_OK if successful.
     */
    QStatus Connect(const qcc::String hostname, uint16_t port);

    /**
     * Close an SSL socket.
     */
    void Close();

    /**
     * Pull bytes from the source.
     * The source is exhausted when ER_NONE is returned.
     *
     * @param buf          Buffer to store pulled bytes
     * @param reqBytes     Number of bytes requested to be pulled from source.
     * @param actualBytes  Actual number of bytes retrieved from source.
     * @return   OI_OK if successful. ER_NONE if source is exhausted. Otherwise an error.
     */
    QStatus PullBytes(void*buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout = Event::WAIT_FOREVER);

    /**
     * Push bytes into the sink.
     *
     * @param buf          Buffer to store pulled bytes
     * @param numBytes     Number of bytes from buf to send to sink.
     * @param numSent      Number of bytes actually consumed by sink.
     * @return   ER_OK if successful.
     */
    QStatus PushBytes(const void* buf, size_t numBytes, size_t& numSent);

    /**
     * Get the Event indicating that data is available.
     *
     * @return Event that is set when data is available.
     */
    qcc::Event& GetSourceEvent() { return *sourceEvent; }

    /**
     * Get the Event indicating that sink can accept data.
     *
     * @return Event set when socket can accept more data via PushBytes
     */
    qcc::Event& GetSinkEvent() { return *sinkEvent; }

    /**
     * Import a PEM-encoded public key.
     */
    QStatus ImportPEM(void);

    /**
     * Return the socketFd for this SslSocket.
     *
     * @return SocketFd
     */
    SocketFd GetSocketFd() { return sock; }

  private:

    /** SslSockets cannot be copied or assigned */
    SslSocket(const SslSocket& other);

    SslSocket operator=(const SslSocket& other);

    /** Forward declaration of OS specific (hidden) internal state for SslSocket */
    struct Internal;

    Internal* internal;        /**< Inernal (OS specific) state for SslSocket instance */
    qcc::Event*sourceEvent;    /**< Event signaled when data is available */
    qcc::Event*sinkEvent;      /**< Event signaled when sink can accept data */
    String Host;               /**< Host to connect to */
    SocketFd sock;             /**< Socket file descriptor */
};

}  /* namespace */

#endif
