/**
 * @file
 *
 * A collection of wrapper functions that abstract the underlying platform socket APIs.
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
#ifndef _QCC_SOCKET_WRAPPER_H
#define _QCC_SOCKET_WRAPPER_H

#include <qcc/platform.h>
#include <Status.h>

namespace qcc {

/**
 * Platform dependent value for an invalid SocketFd
 */
extern const SocketFd INVALID_SOCKET_FD;

/**
 * Close a socket descriptor.
 *
 * @param sockfd        Socket descriptor.
 */
void Close(SocketFd sockfd);

/**
 * Shutdown a connection.
 *
 * @param sockfd        Socket descriptor.
 *
 * @return  Indication of success of failure.
 */
QStatus Shutdown(SocketFd sockfd);

/**
 * Duplicate a socket descriptor.
 *
 * @param sockfd   The socket descriptor to duplicate
 * @param dupSock  [OUT] The duplicated socket descriptor.
 *
 * @return  #ER_OK if the socket was successfully duplicated.
 *          #Other errors indicating the operation failed.
 */
QStatus SocketDup(SocketFd sockfd, SocketFd& dupSock);

/**
 * Send a buffer of data over a socket.
 *
 * @param sockfd        Socket descriptor.
 * @param buf           Pointer to the buffer containing the data to send.
 * @param len           Number of octets in the buffer to be sent.
 * @param sent          OUT: Number of octets sent.
 * @param timeout       Max ms to wait for send to complete or 0 for infinite.
 *
 * @return  #ER_OK if the send succeeded
 *          #ER_WOULDBLOCK if the socket is non-blocking and data cannot be sent at this time.
 *          #ER_OS_ERROR if the send failed
 *          #Other errors indicating the operation failed.
 */
QStatus Send(SocketFd sockfd, const void* buf, size_t len, size_t& sent, uint32_t timeout = 0);

/**
 * Receive a buffer of data over a socket.
 *
 * @param sockfd        Socket descriptor.
 * @param buf           Pointer to the buffer where received data will be stored.
 * @param len           Size of the buffer in octets.
 * @param received      OUT: Number of octets received.
 *
 * @return  #ER_OK if the receive succeeded
 *          #ER_WOULDBLOCK if the socket is non-blocking and there is no data to receive at this time.
 *          #ER_OS_ERROR if the receive failed
 *          #Other errors indicating the operation failed.
 */
QStatus Recv(SocketFd sockfd, void* buf, size_t len, size_t& received);

}

#endif
