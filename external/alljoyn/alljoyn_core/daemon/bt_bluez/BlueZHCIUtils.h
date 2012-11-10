/**
 * @file
 * Utility functions for tweaking Bluetooth behavior via BlueZ.
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

#ifndef _ALLJOYN_BLUEZHCIUTILS_H
#define _ALLJOYN_BLUEZHCIUTILS_H

#include <qcc/platform.h>

#include <qcc/Socket.h>

#include "BDAddress.h"
#include "BTTransportConsts.h"

#include <Status.h>

namespace ajn {
namespace bluez {

/**
 * Set the L2CAP mtu to something better than the BT 1.0 default value.
 */
void ConfigL2capMTU(qcc::SocketFd sockFd);

void ConfigL2capMaster(qcc::SocketFd sockFd);

} // namespace bluez
} // namespace ajn

#endif
