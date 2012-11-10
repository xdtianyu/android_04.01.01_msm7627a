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
#include <qcc/platform.h>
#include <qcc/Mutex.h>

#include "PacketPool.h"

using namespace std;
using namespace qcc;

#define QCC_MODULE "PACKET"

namespace ajn {

PacketPool::PacketPool() : mtu(0), usedCount(0)
{
}

QStatus PacketPool::Start(size_t mtu)
{
    this->mtu = mtu;
    return ER_OK;
}

QStatus PacketPool::Stop()
{
    return ER_OK;
}

PacketPool::~PacketPool()
{
    lock.Lock();
    std::vector<Packet*>::iterator it = freeList.begin();
    while (it != freeList.end()) {
        delete *it++;
    }
    freeList.clear();
    lock.Unlock();
}

Packet* PacketPool::GetPacket() {
    Packet* p = NULL;
    lock.Lock();
    usedCount++;
    if (freeList.size() > 0) {
        p = freeList.back();
        freeList.pop_back();
        lock.Unlock();
    } else {
        lock.Unlock();
        p = new Packet(mtu);
    }
    return p;
}

void PacketPool::ReturnPacket(Packet* p) {
    lock.Lock();
    --usedCount;
    if ((freeList.size() * 2) > usedCount) {
        lock.Unlock();
        delete p;
    } else {
        p->Clean();
        freeList.push_back(p);
        lock.Unlock();
    }
}

}
