/**
 * @file
 * UDPPacketStream is a UDP based implementation of the PacketStream interface.
 */

/******************************************************************************
 * Copyright 2011-2012, Qualcomm Innovation Center, Inc.
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
#include <qcc/Util.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

#include <qcc/Event.h>
#include <qcc/Debug.h>
#include "UDPPacketStream.h"

#define QCC_MODULE "PACKET"

using namespace std;
using namespace qcc;

namespace ajn {

PacketDest UDPPacketStream::GetPacketDest(const qcc::String& addr, uint16_t port)
{
    PacketDest pd;
    struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&pd.data);
    ::memset(&pd.data, 0, sizeof(pd.data));
    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = inet_addr(addr.c_str());
    sa->sin_port = htons(port);
    return pd;
}

UDPPacketStream::UDPPacketStream(const char* ifaceName, uint16_t port) :
    ifaceName(ifaceName),
    port(port),
    sock(-1),
    sourceEvent(&Event::neverSet),
    sinkEvent(&Event::alwaysSet),
    mtu(0)
{
    ::memset(&sa, 0, sizeof(sa));
}

UDPPacketStream::~UDPPacketStream()
{
    if (sourceEvent != &Event::neverSet) {
        delete sourceEvent;
        sourceEvent = &Event::neverSet;
    }
    if (sinkEvent != &Event::alwaysSet) {
        delete sinkEvent;
        sinkEvent = &Event::alwaysSet;
    }
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }
}

QStatus UDPPacketStream::Start()
{
    QStatus status = ER_OK;
    struct ifreq ifr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("socket() failed with %s", ::strerror(errno)));
    }

    if (status == ER_OK) {
        /* Get the mtu for ifaceName */
        ifr.ifr_addr.sa_family = AF_INET;
        strcpy(ifr.ifr_name, ifaceName.c_str());
        if (ioctl(sock, SIOCGIFMTU, (caddr_t)&ifr) >= 0) {
            mtu = (ifr.ifr_mtu - sizeof(struct iphdr) - sizeof(struct udphdr)) & ~0x03;
        } else {
            status = ER_BUS_BAD_INTERFACE_NAME;
            QCC_LogError(status, ("ioctl(SIOCFIFMTU) failed for iface=%s: %s", ifaceName.c_str(), ::strerror(errno)));
        }

        /* Get the interface addr */
        if (ioctl(sock, SIOCGIFADDR, (caddr_t)&ifr) >= 0) {
            sa = ifr.ifr_addr;
        } else {
            status = ER_BUS_BAD_INTERFACE_NAME;
            QCC_LogError(status, ("ioctl(SIOCGIFADDR) failed: %s", ::strerror(errno)));
        }

    }

    /* Bind socket */
    if (status == ER_OK) {
        ((sockaddr_in*)&sa)->sin_port = htons(port);
        if (::bind(sock, &sa, sizeof(struct sockaddr_in)) >= 0) {
            sourceEvent = new qcc::Event(sock, qcc::Event::IO_READ, false);
            sinkEvent = new qcc::Event(sock, qcc::Event::IO_WRITE, false);
        } else {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("bind failed for %s (%s): %s", inet_ntoa(((struct sockaddr_in*)&sa)->sin_addr), ifaceName.c_str(), ::strerror(errno)));
        }
    }

    if (status != ER_OK) {
        close(sock);
        sock = -1;
    }
    return status;
}

String UDPPacketStream::GetIPAddr() const
{
    return inet_ntoa(((struct sockaddr_in*)&sa)->sin_addr);
}

QStatus UDPPacketStream::Stop()
{
    return ER_OK;
}

QStatus UDPPacketStream::PushPacketBytes(const void* buf, size_t numBytes, PacketDest& dest)
{
#if 0
    if (rand() < (RAND_MAX / 100)) {
        printf("Skipping packet with seqNum=0x%x\n", letoh16(*reinterpret_cast<const uint16_t*>((const char*)buf + 4)));
        return ER_OK;
    }
#endif
    assert(numBytes <= mtu);
    const struct sockaddr* sa = reinterpret_cast<const struct sockaddr*>(dest.data);
    size_t sent = sendto(sock, buf, numBytes, 0, sa, sizeof(struct sockaddr_in));
    QStatus status = (sent == numBytes) ? ER_OK : ER_OS_ERROR;
    if (status != ER_OK) {
        if (sent == (size_t) -1) {
            QCC_LogError(status, ("sendto failed: %s (%d)", ::strerror(errno), errno));
        } else {
            QCC_LogError(status, ("Short udp send: exp=%d, act=%d", numBytes, sent));
        }
    }
    return status;
}

QStatus UDPPacketStream::PullPacketBytes(void* buf, size_t reqBytes, size_t& actualBytes,
                                         PacketDest& sender, uint32_t timeout)
{
    QStatus status = ER_OK;
    assert(reqBytes >= mtu);
    struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&sender.data);
    socklen_t saLen = sizeof(PacketDest);
    size_t rcv = recvfrom(sock, buf, reqBytes, 0, sa, &saLen);
    if (rcv != (size_t) -1) {
        actualBytes = rcv;
    } else {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("recvfrom failed: %s", ::strerror(errno)));
    }
    return status;
}

String UDPPacketStream::ToString(const PacketDest& dest) const
{
    const struct sockaddr_in* sa = reinterpret_cast<const struct sockaddr_in*>(dest.data);
    String ret = inet_ntoa(sa->sin_addr);
    ret += " (";
    ret += U32ToString(ntohs(sa->sin_port));
    ret += ")";
    return ret;
}

}
