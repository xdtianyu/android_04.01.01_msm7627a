/**
 * @file
 *
 * Define the abstracted socket interface for Windows.
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
#ifndef _OS_QCC_SOCKETTYPES_H
#define _OS_QCC_SOCKETTYPES_H

#include <qcc/platform.h>

#include <Winsock2.h>
#include <Mswsock.h>
#include <ws2tcpip.h>

namespace qcc {

/**
 * CL definition of IOVec matches the Visual Studio definition of
 * WSABUF for direct casting.
 */
struct IOVec {
    u_long len;         ///< Length of the buffer.
    char FAR* buf;      ///< Pointer to a buffer to be included in a
                        //   scatter-gather list.
};

#define ER_MAX_SG_ENTRIES (IOV_MAX)  /**< Maximum number of scatter-gather list entries. */

typedef int SockAddrSize;  /**< Abstraction of the socket address length type. */

/**
 * Enumeration of address families.
 */
typedef enum {
    QCC_AF_UNSPEC = AF_UNSPEC,  /**< unspecified address family */
    QCC_AF_INET  = AF_INET,     /**< IPv4 address family */
    QCC_AF_INET6 = AF_INET6,    /**< IPv6 address family */
    QCC_AF_UNIX  = -1           /**< Not implemented on windows */
} AddressFamily;

/**
 * Enumeration of socket types.
 */
typedef enum {
    QCC_SOCK_STREAM =    SOCK_STREAM,    /**< TCP */
    QCC_SOCK_DGRAM =     SOCK_DGRAM,     /**< UDP */
    QCC_SOCK_SEQPACKET = SOCK_SEQPACKET, /**< Sequenced data transmission */
    QCC_SOCK_RAW =       SOCK_RAW,       /**< Raw IP packet */
    QCC_SOCK_RDM =       SOCK_RDM        /**< Reliable datagram */
} SocketType;


/**
 * The abstract message header structure defined to match the Linux definition of struct msghdr.
 */
struct MsgHdr {
    SOCKADDR* name;         /**< IP Address. */
    int nameLen;            /**< IP Address length. */
    struct IOVec* iov;      /**< Array of scatter-gather entries. */
    DWORD iovLen;           /**< Number of elements in iov. */
    WSABUF control;         /**< Ancillary data buffer. */
    DWORD flags;            /**< Flags on received message. */
};


#define INET_NTOP qcc::InetNtoP
#define INET_PTON qcc::InetPtoN

/**
 * Windows versions prior to Vista don't provide this API.
 */
const char* InetNtoP(int af, const void* src, char* dst, socklen_t size);

/**
 * Windows versions prior to Vista don't provide this API.
 */
int InetPtoN(int af, const char* src, void* dst);

}

#endif
