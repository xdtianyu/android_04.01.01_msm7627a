/**
 * @file AdapterUtil.cc
 *
 *
 */

/******************************************************************************
 *
 *
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

#include <arpa/inet.h>
#include <errno.h>
#include <list>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/ioctl.h>

#if defined(QCC_OS_DARWIN)
// Note that this include must be _after_ net/if.h
#include <ifaddrs.h>
#include <net/if_dl.h>
#endif

#include <qcc/AdapterUtil.h>
#include <qcc/Debug.h>
#include <qcc/NetInfo.h>
#include <qcc/String.h>

#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "NETWORK"

qcc::AdapterUtil* qcc::AdapterUtil::singleton = NULL;

AdapterUtil::~AdapterUtil(void) {
}

#if defined(QCC_OS_DARWIN)
#define IFHWADDRLEN 6
static QStatus GetMacAddress(unsigned char* mac_dest, const struct ifreq* item)
{
    ifaddrs* iflist = NULL;
    QStatus status = ER_OK;

    if (getifaddrs(&iflist) < 0) {
        status = ER_OS_ERROR;
        goto exit;
    }

    for (ifaddrs* cur = iflist; cur; cur = cur->ifa_next) {
        if ((cur->ifa_addr->sa_family == AF_LINK) &&
            (strcmp(cur->ifa_name, item->ifr_name) == 0) &&
            cur->ifa_addr) {

            sockaddr_dl* sdl = (sockaddr_dl*)cur->ifa_addr;
            memcpy(mac_dest, LLADDR(sdl), IFHWADDRLEN);
            break;
        }
    }
exit:
    if (iflist) {
        freeifaddrs(iflist);
    }
    return status;
}

#else // defined(QCC_OS_LINUX)

static QStatus GetMacAddress(unsigned char* mac_dest, const struct ifreq* item)
{
    QStatus status = ER_OK;
    memcpy(mac_dest, item->ifr_hwaddr.sa_data, IFHWADDRLEN);
    return status;
}

#endif

QStatus AdapterUtil::ForceUpdate()
{
    char buf[1024];
    struct ifconf ifc;
    struct ifreq* ifr;
    int sck;
    int nInterfaces;
    int i;
    unsigned char savedPhysicalAddress[IFHWADDRLEN];
    uint8_t adapterCount = 1;

    QStatus status = ER_OK;

    lock.Lock();

    interfaces.clear();
    isMultihomed = false;
    memset(savedPhysicalAddress, 0, IFHWADDRLEN);

    // Get a socket handle.
    sck = socket(AF_INET, SOCK_DGRAM, 0);
    if (sck < 0) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Opening socket: %s", strerror(errno)));
        goto exit;
    }

    // Query available interfaces.
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sck, SIOCGIFCONF, &ifc) < 0) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Calling IOCtl: %s", strerror(errno)));
        goto exit;
    }

    // Iterate through the list of interfaces.
    ifr         = ifc.ifc_req;
    nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

    for (i = 0; i < nInterfaces; i++) {
        struct ifreq* item = &ifr[i];
        struct sockaddr* addr = &item->ifr_addr;
        NetInfo netInfo;

        qcc::String aname(item->ifr_name);
        unsigned char physicalAddress[IFHWADDRLEN];
        status = GetMacAddress(physicalAddress, item);
        if (status != ER_OK) {
            goto exit;
        }

        if (qcc::String::npos != aname.find("lo")) {
            continue;
        }

        if (adapterCount > 1 &&
            memcmp(&physicalAddress, &savedPhysicalAddress, IFHWADDRLEN)) {
            isMultihomed = true;
        }

        netInfo.name = aname;
        if (addr->sa_family == AF_INET) {
            struct sockaddr_in* ipv4addr = reinterpret_cast<struct sockaddr_in*>(addr);
            netInfo.addr = IPAddress(ntohl(ipv4addr->sin_addr.s_addr));
        } else if (addr->sa_family == AF_INET6) {
            struct sockaddr_in6* ipv6addr = reinterpret_cast<struct sockaddr_in6*>(addr);
            netInfo.addr = IPAddress(reinterpret_cast<uint8_t*>(ipv6addr->sin6_addr.s6_addr),
                                     IPAddress::IPv6_SIZE);
        } else {
            netInfo.addr = IPAddress();
        }

        // We're done with IP Address  so we can overwrite the memory with the MTU.
        if (ioctl(sck, SIOCGIFMTU, item) < 0) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Calling IOCtl: %s", strerror(errno)));
            goto exit;
        }

        netInfo.mtu = item->ifr_mtu;
        netInfo.isVPN = false; // ToDo how to determine this?

        QCC_DbgPrintf(("Interface[%u]: name=%s  addr=%s  MTU=%u", i,
                       netInfo.name.c_str(), netInfo.addr.ToString().c_str(), netInfo.mtu));

        interfaces.push_back(netInfo);

        adapterCount++;
        memcpy(&savedPhysicalAddress, &physicalAddress, IFHWADDRLEN);
    }

exit:
    lock.Unlock();
    return status;
}


bool AdapterUtil::IsVPN(IPAddress addr)
{
    bool isVPN = false;
    AdapterUtil::const_iterator networkInterfaceIter;

    lock.Lock();

    // find record corresponding to 'addr'
    for (networkInterfaceIter = interfaces.begin(); networkInterfaceIter != interfaces.end(); ++networkInterfaceIter) {
        if (networkInterfaceIter->addr != addr) {
            continue;
        }
        isVPN = networkInterfaceIter->isVPN;
        break;
    }

    lock.Unlock();

    return isVPN;

}



