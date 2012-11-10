/**
 * @file NetworkNetworkInterface.h
 *
 * This file defines a class that are used to perform some network interface related operations
 * that are required by the ICE transport.
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <algorithm>

#if defined(QCC_OS_GROUP_POSIX)

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#if defined(QCC_OS_ANDROID) || defined(QCC_OS_LINUX)
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#endif // defined(QCC_OS_GROUP_POSIX)

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/IfConfig.h>

#include "NetworkInterface.h"

using namespace std;
using namespace qcc;

#define QCC_MODULE "NETWORK_INTERFACE"

namespace ajn {

NetworkInterface::NetworkInterface(bool enableIPV6)
    : EnableIPV6(enableIPV6)
{
    liveInterfaces.clear();
}

NetworkInterface::~NetworkInterface()
{
    liveInterfaces.clear();
}


String NetworkInterface::PrintNetworkInterfaceType(uint8_t type)
{
    String retStr = String("NONE");

    switch (type) {

    case ANY:
        retStr = String("ANY");
        break;

    case NONE:
    default:
        break;

    }

    return retStr;
}

QStatus NetworkInterface::UpdateNetworkInterfaces(void)
{
    QCC_DbgPrintf(("NetworkInterface::UpdateNetworkInterfaces()\n"));

    /* Call IfConfig to get the list of interfaces currently configured in the
       system.  This also pulls out interface flags, addresses and MTU. */
    QCC_DbgPrintf(("NetworkInterface::UpdateNetworkInterfaces(): IfConfig()\n"));
    std::vector<qcc::IfConfigEntry> entries;
    QStatus status = qcc::IfConfig(entries);

    /* Filter out the unwanted entries and populate valid entries into liveInterfaces */
    for (std::vector<IfConfigEntry>::const_iterator j = entries.begin(); j != entries.end(); ++j) {

        if (((*j).m_family == QCC_AF_UNSPEC) ||
            ((!EnableIPV6) && ((*j).m_family == QCC_AF_INET6)) ||
            (((*j).m_flags & qcc::IfConfigEntry::UP) == 0) ||
            (((*j).m_flags & qcc::IfConfigEntry::LOOPBACK) != 0)) {
            continue;
        }


        liveInterfaces.push_back((*j));

        QCC_DbgPrintf(("NetworkInterface::UpdateNetworkInterfaces(): Entry %s with address %s\n", (*j).m_name.c_str(), (*j).m_addr.c_str()));
    }

    return status;
}

bool NetworkInterface::IsAnyNetworkInterfaceUp(void)
{
    if (liveInterfaces.empty()) {
        return false;
    }

    return true;
}

} // namespace ajn
