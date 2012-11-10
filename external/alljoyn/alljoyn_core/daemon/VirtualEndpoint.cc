/**
 * @file
 * A VirtualEndpoint is a representation of an AllJoyn endpoint that exists behind a remote
 * AllJoyn daemon.
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

#include <qcc/platform.h>
#include <vector>
#include "VirtualEndpoint.h"
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <alljoyn/Message.h>
#include <Status.h>

#define QCC_MODULE "ALLJOYN_OBJ"

using namespace std;
using namespace qcc;

namespace ajn {

VirtualEndpoint::VirtualEndpoint(const char* uniqueName, RemoteEndpoint& b2bEp)
    : BusEndpoint(BusEndpoint::ENDPOINT_TYPE_VIRTUAL),
    m_uniqueName(uniqueName),
    m_hasRefs(false)
{
    m_b2bEndpoints.insert(pair<SessionId, RemoteEndpoint*>(0, &b2bEp));
}

QStatus VirtualEndpoint::PushMessage(Message& msg)
{
    return PushMessage(msg, msg->GetSessionId());
}

QStatus VirtualEndpoint::PushMessage(Message& msg, SessionId id)
{
    QStatus status = ER_BUS_NO_ROUTE;

    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::iterator it = (id == 0) ? m_b2bEndpoints.begin() : m_b2bEndpoints.lower_bound(id);
    while ((it != m_b2bEndpoints.end()) && (id == it->first)) {
        RemoteEndpoint* ep = it->second;
        ep->IncrementWaiters();
        m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
        status = ep->PushMessage(msg);
        m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
        it = m_b2bEndpoints.lower_bound(id);
        while ((it != m_b2bEndpoints.end()) && (it->first == id)) {
            if (it->second == ep) {
                it->second->DecrementWaiters();
                ++it;
                break;
            } else {
                ++it;
            }
        }
        if (status != ER_BUS_ENDPOINT_CLOSING) {
            break;
        }
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);

    return status;
}

RemoteEndpoint* VirtualEndpoint::GetBusToBusEndpoint(SessionId sessionId, int* b2bCount) const
{
    RemoteEndpoint* ret = NULL;
    if (b2bCount) {
        *b2bCount = 0;
    }
    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.lower_bound(sessionId);
    while ((it != m_b2bEndpoints.end()) && (it->first == sessionId)) {
        if (!ret) {
            ret = it->second;
        }
        if (b2bCount) {
            (*b2bCount)++;
        }
        ++it;
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return ret;
}

bool VirtualEndpoint::AddBusToBusEndpoint(RemoteEndpoint& endpoint)
{
    QCC_DbgTrace(("VirtualEndpoint::AddBusToBusEndpoint(this=%s, b2b=%s)", GetUniqueName().c_str(), endpoint.GetUniqueName().c_str()));

    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::iterator it = m_b2bEndpoints.begin();
    bool found = false;
    while ((it != m_b2bEndpoints.end()) && (it->first == 0)) {
        if (it->second == &endpoint) {
            found = true;
            break;
        }
        ++it;
    }
    if (!found) {
        m_b2bEndpoints.insert(pair<SessionId, RemoteEndpoint*>(0, &endpoint));
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return !found;
}

void VirtualEndpoint::GetSessionIdsForB2B(RemoteEndpoint& endpoint, set<SessionId>& sessionIds)
{
    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::iterator it = m_b2bEndpoints.begin();
    while (it != m_b2bEndpoints.end()) {
        if (it->first && (it->second == &endpoint)) {
            sessionIds.insert(it->first);
        }
        ++it;
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
}

bool VirtualEndpoint::RemoveBusToBusEndpoint(RemoteEndpoint& endpoint)
{
    QCC_DbgTrace(("VirtualEndpoint::RemoveBusToBusEndpoint(this=%s, b2b=%s)", GetUniqueName().c_str(), endpoint.GetUniqueName().c_str()));

    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::iterator it = m_b2bEndpoints.begin();
    while (it != m_b2bEndpoints.end()) {
        if (it->second == &endpoint) {
            /* A non-zero session means that the b2b has one less ref */
            if (it->first != 0) {
                it->second->DecrementRef();
            }
            m_b2bEndpoints.erase(it++);
        } else {
            ++it;
        }
    }

    /*
     * This Virtual endpoint reports itself as empty (of b2b endpoints) when any of the following are true:
     * 1) The last b2b ep is being removed.
     * 2) A last session route through this vep is being removed and the b2bEp being removed doesnt connect to the same
     *    remote daemon as a different b2bEp in the vep.
     *
     * This algorithm allows for cleanup of the following triangular routing problem:
     * - Device A connects to device B
     * - Device A connects to device C
     * - Device B connects to device C
     * - At this point, each device has a vep for A with 2 b2bEps.
     * - Now device A leaves the bus.
     * - B knows to remove the direct B2BEp to A but it (would otherwise) think it can still reach A through C
     * - C knows to remove the direct B2bEp to A but it (would otherwise) think it can still reach A through B
     * This algorithm solves this problem by removing the veps when they no longer route for any session AND
     * when they are suseptible to the triangular route problem.
     */
    bool isEmpty;
    if (m_hasRefs) {
        isEmpty = (m_b2bEndpoints.lower_bound(1) == m_b2bEndpoints.end());
        if (isEmpty) {
            const qcc::GUID128& guid = endpoint.GetRemoteGUID();
            it = m_b2bEndpoints.begin();
            while (it != m_b2bEndpoints.end()) {
                if (it->second->GetRemoteGUID() == guid) {
                    isEmpty = false;
                    break;
                }
                ++it;
            }
        }
    } else {
        isEmpty = m_b2bEndpoints.empty();
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return isEmpty;
}

QStatus VirtualEndpoint::AddSessionRef(SessionId id, RemoteEndpoint& b2bEp)
{
    QCC_DbgTrace(("VirtualEndpoint::AddSessionRef(this=%s, id=%u, b2b=%s)", GetUniqueName().c_str(), id, b2bEp.GetUniqueName().c_str()));

    assert(id != 0);

    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);

    /* Sanity check. Make sure b2bEp is connected to this virtual ep (with sessionId == 0) */
    bool canUse = CanUseRoute(b2bEp);
    if (canUse) {
        /* Increment b2bEp ref */
        b2bEp.IncrementRef();
        /* Map sessionId to b2bEp */
        m_b2bEndpoints.insert(pair<SessionId, RemoteEndpoint*>(id, &b2bEp));
        m_hasRefs = true;
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return canUse ? ER_OK : ER_FAIL;
}

QStatus VirtualEndpoint::AddSessionRef(SessionId id, SessionOpts* opts, RemoteEndpoint*& b2bEp)
{
    QCC_DbgTrace(("VirtualEndpoint::AddSessionRef(this=%s, %u, <opts>, %s)", GetUniqueName().c_str(), id, b2bEp ? b2bEp->GetUniqueName().c_str() : "<none>"));

    RemoteEndpoint* bestEp = NULL;
    //uint32_t hops = 1000;

    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);

#if 0
    /* Look for best B2B that matches SessionOpts */
    multimap<SessionId, RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.begin();
    while ((it != m_b2bEndpoints.end()) && (it->first == 0)) {
        map<RemoteEndpoint*, B2BInfo>::const_iterator bit = m_b2bInfos.find(it->second);
        if ((bit != m_b2bInfos.end()) && (!opts || bit->second.opts.IsCompatible(*opts)) && (bit->second.hops < hops)) {
            bestEp = it->second;
            hops = bit->second.hops;
        }
        ++it;
    }
#else
    /* TODO: Placeholder until we exchange session opts and hop count via ExchangeNames */
    multimap<SessionId, RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.find(id);
    if (it == m_b2bEndpoints.end()) {
        it = m_b2bEndpoints.begin();
    }
    if ((it != m_b2bEndpoints.end()) && ((it->first == 0) || it->first == id)) {
        bestEp = it->second;
    }
#endif

    /* Map session id to bestEp */
    if (bestEp) {
        AddSessionRef(id, *bestEp);
    }
    b2bEp = bestEp;
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return bestEp ? ER_OK : ER_FAIL;
}

void VirtualEndpoint::RemoveSessionRef(SessionId id)
{
    QCC_DbgTrace(("VirtualEndpoint::RemoveSessionRef(this=%s, id=%u)", GetUniqueName().c_str(), id));
    assert(id != 0);
    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::iterator it = m_b2bEndpoints.find(id);
    if (it != m_b2bEndpoints.end()) {
        it->second->DecrementRef();
        m_b2bEndpoints.erase(it);
    } else {
        QCC_DbgPrintf(("VirtualEndpoint::RemoveSessionRef: vep=%s failed to find session = %u", m_uniqueName.c_str(), id));
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
}

bool VirtualEndpoint::CanUseRoute(const RemoteEndpoint& b2bEndpoint) const
{
    bool isFound = false;
    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.begin();
    while ((it != m_b2bEndpoints.end()) && (it->first == 0)) {
        if (it->second == &b2bEndpoint) {
            isFound = true;
            break;
        }
        ++it;
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return isFound;
}

bool VirtualEndpoint::CanUseRoutes(const multiset<String>& b2bNameSet) const
{
    bool isFound = false;
    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multiset<String>::const_iterator nit = b2bNameSet.begin();
    while (nit != b2bNameSet.end()) {
        multimap<SessionId, RemoteEndpoint*>::const_iterator eit = m_b2bEndpoints.begin();
        while (eit != m_b2bEndpoints.end()) {
            String n = eit->second->GetUniqueName();
            if (*nit == n) {
                isFound = true;
                break;
            }
            ++eit;
        }
        if (isFound) {
            break;
        }
        ++nit;
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return isFound;
}

bool VirtualEndpoint::CanRouteWithout(const qcc::GUID128& guid) const
{
    bool canRoute = false;
    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
    multimap<SessionId, RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.begin();
    while (it != m_b2bEndpoints.end()) {
        if (guid != it->second->GetRemoteGUID()) {
            canRoute = true;
            break;
        }
        ++it;
    }
    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    return canRoute;
}

}
