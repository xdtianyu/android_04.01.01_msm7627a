/**
 * @file
 * BTAccessor declaration for BlueZ
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
#ifndef _ALLJOYN_BLUEZBTENDPOINT_H
#define _ALLJOYN_BLUEZBTENDPOINT_H

#include <qcc/platform.h>

#include <alljoyn/BusAttachment.h>

#include "BlueZUtils.h"
#include "BTEndpoint.h"
#include "BTNodeInfo.h"

namespace ajn {

class BlueZBTEndpoint : public BTEndpoint {
  public:

    /**
     * Bluetooth endpoint constructor
     */
    BlueZBTEndpoint(BusAttachment& bus,
                    bool incoming,
                    qcc::SocketFd sockFd,
                    const BTNodeInfo& node,
                    const BTBusAddress& redirect) :
        BTEndpoint(bus, incoming, sockStream, node, redirect),
        sockStream(sockFd)
    { }

  private:
    bluez::BTSocketStream sockStream;
};

} // namespace ajn

#endif
