/**
 * @file
 *
 * This file defines the Network Interface information class.
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

#ifndef _QCC_NETINFO_H
#define _QCC_NETINFO_H

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/IPAddress.h>

namespace qcc {

/**
 * Network information data structure that describes various attributes of a
 * given network interface.
 */
struct NetInfo {
    qcc::String name;        ///< OS defined interface name.
    IPAddress addr;          ///< IP Address for the interface.
    size_t mtu;              ///< MTU size of the interface.
    bool isVPN;              ///< True iff the interface is a VPN
};

}

#endif
