/**
 * @file IceManager.cc
 *
 * IceManager is responsible for executing and coordinating ICE related network operations.
 *
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
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

#include <list>
#include <qcc/Mutex.h>
#include <qcc/Debug.h>
#include <ICESession.h>
#include <ICESessionListener.h>
#include <ICEManager.h>
#include "RendezvousServerInterface.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "ICEMANAGER"

namespace ajn {

ICEManager::ICEManager()
{
}

ICEManager::~ICEManager(void)
{
    // Reclaim memory consumed by the sessions.
    lock.Lock();
    while (!sessions.empty()) {
        ICESession* session = sessions.front();
        delete session;
        sessions.pop_front();
    }
    lock.Unlock();
}



QStatus ICEManager::AllocateSession(bool addHostCandidates,
                                    bool addRelayedCandidates,
                                    bool enableIpv6,
                                    ICESessionListener* listener,
                                    ICESession*& session,
                                    STUNServerInfo stunInfo,
                                    IPAddress onDemandAddress,
                                    IPAddress persistentAddress)
{
    QStatus status = ER_OK;

    session = new ICESession(addHostCandidates, addRelayedCandidates, listener, stunInfo, onDemandAddress, persistentAddress);

    status = session->Init(enableIpv6);

    if (ER_OK == status) {
        lock.Lock();                // Synch with another thread potentially calling destructor.
                                    // Not likely because this is a singleton, but...
        sessions.push_back(session);
        lock.Unlock();
    } else {
        QCC_LogError(status, ("session->Init"));
        delete session;
        session = NULL;
    }

    return status;
}



QStatus ICEManager::DeallocateSession(ICESession*& session)
{
    QStatus status = ER_OK;

    assert(session != NULL);

    // remove from list
    lock.Lock();                // Synch with another thread potentially calling destructor.
                                // Not likely because this is a singleton, but...
    sessions.remove(session);
    lock.Unlock();

    delete session;

    return status;
}

} //namespace ajn
