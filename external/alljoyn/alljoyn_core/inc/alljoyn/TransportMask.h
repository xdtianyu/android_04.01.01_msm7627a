#ifndef _ALLJOYN_TRANPORTMASK_H
#define _ALLJOYN_TRANPORTMASK_H
/**
 * @file
 * Transport type definitions
 */

/******************************************************************************
 * Copyright 2009-2010,2012 Qualcomm Innovation Center, Inc.
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

namespace ajn {

/** Bitmask of all transport types */
typedef uint16_t TransportMask;

const TransportMask TRANSPORT_NONE      = 0x0000;   /**< no transports */
const TransportMask TRANSPORT_ANY       = 0xFFFF;   /**< ANY transport */
const TransportMask TRANSPORT_LOCAL     = 0x0001;   /**< Local (same device) transport */
const TransportMask TRANSPORT_BLUETOOTH = 0x0002;   /**< Bluetooth transport */
const TransportMask TRANSPORT_WLAN      = 0x0004;   /**< Wireless local-area network transport */
const TransportMask TRANSPORT_WWAN      = 0x0008;   /**< Wireless wide-area network transport */
const TransportMask TRANSPORT_LAN       = 0x0010;   /**< Wired local-area network transport */
const TransportMask TRANSPORT_ICE       = 0x0020;   /**< Transport using ICE protocol */

}

#endif
