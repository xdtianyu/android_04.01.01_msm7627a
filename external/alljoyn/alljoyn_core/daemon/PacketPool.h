/**
 * @file
 * Implements a pool for Packet objects
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
#ifndef _ALLJOYN_PACKETPOOL_H
#define _ALLJOYN_PACKETPOOL_H

#include <qcc/platform.h>

#include <vector>

#include "Packet.h"

namespace ajn {

class PacketPool {
  public:
    PacketPool();

    QStatus Start(size_t mtu);

    QStatus Stop();

    ~PacketPool();

    Packet* GetPacket();

    void ReturnPacket(Packet* p);

    uint32_t GetMTU() const { return mtu; }

  private:
    size_t mtu;
    qcc::Mutex lock;
    std::vector<Packet*> freeList;
    size_t usedCount;
};

}

#endif
