/**
 * @file
 * Router is responsible for taking inbound messages and routing them
 * to an appropriate set of endpoints.
 */

/******************************************************************************
 * Copyright 2009-2012, Qualcomm Innovation Center, Inc.
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

#include <assert.h>

#include <qcc/Debug.h>
#include <qcc/Logger.h>
#include <qcc/String.h>
#include <qcc/Util.h>

#include <Status.h>

#include "BusController.h"
#include "BusEndpoint.h"
#include "DaemonRouter.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;


namespace ajn {


DaemonRouter::DaemonRouter() : localEndpoint(NULL), ruleTable(), nameTable(), busController(NULL)
{
}

static QStatus SendThroughEndpoint(Message& msg, BusEndpoint& ep, SessionId sessionId)
{
    QStatus status;
    if ((sessionId != 0) && (ep.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
        status = static_cast<VirtualEndpoint&>(ep).PushMessage(msg, sessionId);
    } else {
        status = ep.PushMessage(msg);
    }
    if (status != ER_OK) {
        QCC_LogError(status, ("SendThroughEndpoint(dest=%s, ep=%s, id=%u) failed", msg->GetDestination(), ep.GetUniqueName().c_str(), sessionId));
    }
    return status;
}

QStatus DaemonRouter::PushMessage(Message& msg, BusEndpoint& origSender)
{
    QStatus status = ER_OK;
    BusEndpoint* sender = &origSender;
    bool replyExpected = (msg->GetType() == MESSAGE_METHOD_CALL) && ((msg->GetFlags() & ALLJOYN_FLAG_NO_REPLY_EXPECTED) == 0);

    const char* destination = msg->GetDestination();
    SessionId sessionId = msg->GetSessionId();

    bool destinationEmpty = destination[0] == '\0';
    if (!destinationEmpty) {
        nameTable.Lock();
        BusEndpoint* destEndpoint = nameTable.FindEndpoint(destination);
        if (destEndpoint) {
            /* If this message is coming from a bus-to-bus ep, make sure the receiver is willing to receive it */
            if (!((sender->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) && !destEndpoint->AllowRemoteMessages())) {
                /*
                 * If the sender doesn't allow remote messages reject method calls that go off
                 * device and require a reply because the reply will be blocked and this is most
                 * definitely not what the sender expects.
                 */
                if ((destEndpoint->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL) && replyExpected && !sender->AllowRemoteMessages()) {
                    QCC_DbgPrintf(("Blocking method call from %s to %s (serial=%d) because caller does not allow remote messages",
                                   msg->GetSender(),
                                   destEndpoint->GetUniqueName().c_str(),
                                   msg->GetCallSerial()));
                    msg->ErrorMsg(msg, "org.alljoyn.Bus.Blocked", "Method reply would be blocked because caller does not allow remote messages");
                    PushMessage(msg, *localEndpoint);
                } else {
                    BusEndpoint::EndpointType epType = destEndpoint->GetEndpointType();
                    RemoteEndpoint* protectEp = (epType == BusEndpoint::ENDPOINT_TYPE_REMOTE) || (epType == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) ? static_cast<RemoteEndpoint*>(destEndpoint) : NULL;
                    if (protectEp) {
                        protectEp->IncrementWaiters();
                    }
                    nameTable.Unlock();
                    status = SendThroughEndpoint(msg, *destEndpoint, sessionId);
                    if (protectEp) {
                        protectEp->DecrementWaiters();
                    }
                    nameTable.Lock();
                }
            } else {
                QCC_DbgPrintf(("Blocking message from %s to %s (serial=%d) because receiver does not allow remote messages",
                               msg->GetSender(),
                               destEndpoint->GetUniqueName().c_str(),
                               msg->GetCallSerial()));
                /* If caller is expecting a response return an error indicating the method call was blocked */
                if (replyExpected) {
                    qcc::String description("Remote method calls blocked for bus name: ");
                    description += destination;
                    msg->ErrorMsg(msg, "org.alljoyn.Bus.Blocked", description.c_str());
                    PushMessage(msg, *localEndpoint);
                }
            }
            if ((ER_OK != status) && (ER_BUS_ENDPOINT_CLOSING != status)) {
                QCC_LogError(status, ("BusEndpoint::PushMessage failed"));
            }
            nameTable.Unlock();
        } else {
            nameTable.Unlock();
            if ((msg->GetFlags() & ALLJOYN_FLAG_AUTO_START) &&
                (sender->GetEndpointType() != BusEndpoint::ENDPOINT_TYPE_BUS2BUS) &&
                (sender->GetEndpointType() != BusEndpoint::ENDPOINT_TYPE_NULL)) {

                status = busController->StartService(msg, sender);
            } else {
                status = ER_BUS_NO_ROUTE;
            }
            if (status != ER_OK) {
                if (replyExpected) {
                    QCC_LogError(status, ("Returning error %s no route to %s", msg->Description().c_str(), destination));
                    /* Need to let the sender know its reply message cannot be passed on. */
                    qcc::String description("Unknown bus name: ");
                    description += destination;
                    msg->ErrorMsg(msg, "org.freedesktop.DBus.Error.ServiceUnknown", description.c_str());
                    PushMessage(msg, *localEndpoint);
                } else {
                    QCC_LogError(status, ("Discarding %s no route to %s:%d", msg->Description().c_str(), destination, sessionId));
                }
            }
        }
    } else if (sessionId == 0) {
        /*
         * The message has an empty destination field and no session is specified so this is a
         * regular broadcast message.
         */
        nameTable.Lock();
        ruleTable.Lock();
        RuleIterator it = ruleTable.Begin();
        while (it != ruleTable.End()) {
            if (it->second.IsMatch(msg)) {
                BusEndpoint* dest = it->first;
                QCC_DbgPrintf(("Routing %s (%d) to %s", msg->Description().c_str(), msg->GetCallSerial(), dest->GetUniqueName().c_str()));
                /*
                 * If the message originated locally or the destination allows remote messages
                 * forward the message, otherwise silently ignore it.
                 */
                if (!((sender->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) && !dest->AllowRemoteMessages())) {
                    BusEndpoint::EndpointType epType = dest->GetEndpointType();
                    RemoteEndpoint* protectEp = (epType == BusEndpoint::ENDPOINT_TYPE_REMOTE) || (epType == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) ? static_cast<RemoteEndpoint*>(dest) : NULL;
                    if (protectEp) {
                        protectEp->IncrementWaiters();
                    }
                    ruleTable.Unlock();
                    nameTable.Unlock();
                    QStatus tStatus = SendThroughEndpoint(msg, *dest, sessionId);
                    status = (status == ER_OK) ? tStatus : status;
                    if (protectEp) {
                        protectEp->DecrementWaiters();
                    }
                    nameTable.Lock();
                    ruleTable.Lock();
                }
                it = ruleTable.AdvanceToNextEndpoint(dest);
            } else {
                ++it;
            }
        }
        ruleTable.Unlock();
        nameTable.Unlock();
        /*
         * Route global broadcast to all bus-to-bus endpoints that aren't the sender of the message
         */
        if (msg->IsGlobalBroadcast()) {
            m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
            set<RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.begin();
            while (it != m_b2bEndpoints.end()) {
                RemoteEndpoint* ep = *it;
                if (ep != &origSender) {
                    BusEndpoint::EndpointType epType = ep->GetEndpointType();
                    RemoteEndpoint* protectEp = (epType == BusEndpoint::ENDPOINT_TYPE_REMOTE) || (epType == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) ? static_cast<RemoteEndpoint*>(ep) : NULL;
                    if (protectEp) {
                        protectEp->IncrementWaiters();
                    }
                    m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
                    QStatus tStatus = SendThroughEndpoint(msg, *ep, sessionId);
                    status = (status == ER_OK) ? tStatus : status;
                    if (protectEp) {
                        protectEp->DecrementWaiters();
                    }
                    m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
                    it = m_b2bEndpoints.lower_bound(ep);
                }
                if (it != m_b2bEndpoints.end()) {
                    ++it;
                }
            }
            m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
        }
    } else {
        /*
         * The message has an empty destination field and a session id was specified so this is a
         * session multicast message.
         */
        sessionCastSetLock.Lock(MUTEX_CONTEXT);
        RemoteEndpoint* lastB2b = NULL;
        SessionCastEntry sce(sessionId, msg->GetSender(), NULL, NULL);
        set<SessionCastEntry>::iterator sit = sessionCastSet.lower_bound(sce);
        while ((sit != sessionCastSet.end()) && (sit->id == sce.id) && (sit->src == sce.src)) {
            if (!sit->b2bEp || (sit->b2bEp != lastB2b)) {
                lastB2b = sit->b2bEp;
                SessionCastEntry entry = *sit;
                BusEndpoint::EndpointType epType = sit->destEp->GetEndpointType();
                RemoteEndpoint* protectEp = (epType == BusEndpoint::ENDPOINT_TYPE_REMOTE) || (epType == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) ? static_cast<RemoteEndpoint*>(sit->destEp) : NULL;
                if (protectEp) {
                    protectEp->IncrementWaiters();
                }
                sessionCastSetLock.Unlock(MUTEX_CONTEXT);
                QStatus tStatus = SendThroughEndpoint(msg, *sit->destEp, sessionId);
                status = (status == ER_OK) ? tStatus : status;
                if (protectEp) {
                    protectEp->DecrementWaiters();
                }
                sessionCastSetLock.Lock(MUTEX_CONTEXT);
                sit = sessionCastSet.lower_bound(entry);
            }
            if (sit != sessionCastSet.end()) {
                ++sit;
            }
        }
        sessionCastSetLock.Unlock(MUTEX_CONTEXT);
    }
    return status;
}

void DaemonRouter::GetBusNames(vector<qcc::String>& names) const
{
    nameTable.GetBusNames(names);
}


BusEndpoint* DaemonRouter::FindEndpoint(const qcc::String& busName)
{
    BusEndpoint* ep = nameTable.FindEndpoint(busName);
    if (!ep) {
        m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
        for (set<RemoteEndpoint*>::const_iterator it = m_b2bEndpoints.begin(); it != m_b2bEndpoints.end(); ++it) {
            if ((*it)->GetUniqueName() == busName) {
                ep = *it;
                break;
            }
        }
        m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    }
    return ep;
}

QStatus DaemonRouter::RegisterEndpoint(BusEndpoint& endpoint, bool isLocal)
{
    QCC_DbgTrace(("DaemonRouter::RegisterEndpoint(%s, %s)", endpoint.GetUniqueName().c_str(), isLocal ? "true" : "false"));

    QStatus status = ER_OK;

    /* Keep track of local endpoint */
    if (isLocal) {
        localEndpoint = static_cast<LocalEndpoint*>(&endpoint);
    }

    if (endpoint.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_BUS2BUS) {
        /* AllJoynObj is in charge of managing bus-to-bus endpoints and their names */
        RemoteEndpoint* busToBusEndpoint = static_cast<RemoteEndpoint*>(&endpoint);

        /*
         * If the bus controller is NULL, it was either never set, or it has
         * removed itself.  If gone at this point, we certainly can't call out
         * to it.  Since it goes away here when the BusController is going away,
         * it probably means we are about to stop.
         */
        if (busController == NULL) {
            return ER_BUS_STOPPING;
        }

        status = busController->GetAllJoynObj().AddBusToBusEndpoint(*busToBusEndpoint);

        /* Add to list of bus-to-bus endpoints */
        m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
        m_b2bEndpoints.insert(busToBusEndpoint);
        m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);
    } else {
        /* Bus-to-client endpoints appear directly on the bus */
        nameTable.AddUniqueName(endpoint);
    }

    /* Notify local endpoint that it is connected */
    if (&endpoint == localEndpoint) {
        localEndpoint->BusIsConnected();
    }

    return status;
}

void DaemonRouter::UnregisterEndpoint(BusEndpoint& endpoint)
{
    QCC_DbgTrace(("UnregisterEndpoint: %s (type=%d)", endpoint.GetUniqueName().c_str(), endpoint.GetEndpointType()));

    if (BusEndpoint::ENDPOINT_TYPE_BUS2BUS == endpoint.GetEndpointType()) {
        /* Inform bus controller of bus-to-bus endpoint removal */
        RemoteEndpoint* busToBusEndpoint = static_cast<RemoteEndpoint*>(&endpoint);

        /*
         * If the bus controller is NULL, it was either never set, or it has
         * removed itself.  If gone at this point, we certainly can't call out
         * to it.  Since it goes away here when the BusController is going away,
         * it probably means we are about to stop; but we'll continue to do as
         * much local cleanup as we can here.
         */
        if (busController != NULL) {
            busController->GetAllJoynObj().RemoveBusToBusEndpoint(*busToBusEndpoint);
        }

        /* Remove the bus2bus endpoint from the list */
        m_b2bEndpointsLock.Lock(MUTEX_CONTEXT);
        set<RemoteEndpoint*>::iterator it = m_b2bEndpoints.begin();
        while (it != m_b2bEndpoints.end()) {
            if (*it == busToBusEndpoint) {
                m_b2bEndpoints.erase(it);
                break;
            }
            ++it;
        }
        m_b2bEndpointsLock.Unlock(MUTEX_CONTEXT);

        /* Remove entries from sessionCastSet with same b2bEp */
        sessionCastSetLock.Lock(MUTEX_CONTEXT);
        set<SessionCastEntry>::iterator sit = sessionCastSet.begin();
        while (sit != sessionCastSet.end()) {
            set<SessionCastEntry>::iterator doomed = sit;
            ++sit;
            if (doomed->b2bEp == &endpoint) {
                sessionCastSet.erase(doomed);
            }
        }
        sessionCastSetLock.Unlock(MUTEX_CONTEXT);
    } else {
        /* Remove any session routes */
        qcc::String uniqueName = endpoint.GetUniqueName();
        RemoveSessionRoutes(uniqueName.c_str(), 0);

        /* Remove endpoint from names and rules */
        nameTable.RemoveUniqueName(uniqueName);
        RemoveAllRules(endpoint);
#if defined(QCC_OS_ANDROID)
        PermissionDB::GetDB().RemovePermissionCache(endpoint);
#endif
    }

    /* Unregister static endpoints */
    if (&endpoint == localEndpoint) {
        localEndpoint = NULL;
    }
}

QStatus DaemonRouter::AddSessionRoute(SessionId id, BusEndpoint& srcEp, RemoteEndpoint* srcB2bEp, BusEndpoint& destEp, RemoteEndpoint*& destB2bEp, SessionOpts* optsHint)
{
    QCC_DbgTrace(("DaemonRouter::AddSessionRoute(%u, %s, %s, %s, %s, %s)", id, srcEp.GetUniqueName().c_str(), srcB2bEp ? srcB2bEp->GetUniqueName().c_str() : "<none>", destEp.GetUniqueName().c_str(), destB2bEp ? destB2bEp->GetUniqueName().c_str() : "<none>", optsHint ? "opts" : "NULL"));

    QStatus status = ER_OK;
    if (id == 0) {
        return ER_BUS_NO_SESSION;
    }

    if (destEp.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL) {
        if (destB2bEp) {
            status = static_cast<VirtualEndpoint&>(destEp).AddSessionRef(id, *destB2bEp);
        } else if (optsHint) {
            status = static_cast<VirtualEndpoint&>(destEp).AddSessionRef(id, optsHint, destB2bEp);
        } else {
            status = ER_BUS_NO_SESSION;
        }
        if (status != ER_OK) {
            QCC_LogError(status, ("AddSessionRef(this=%s, %u, %s%s) failed", destEp.GetUniqueName().c_str(),
                                  id, destB2bEp ? "" : "opts, ", destB2bEp ? destB2bEp->GetUniqueName().c_str() : "NULL"));
        }
    }

    if ((status == ER_OK) && srcB2bEp) {
        assert(srcEp.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL);
        status = static_cast<VirtualEndpoint&>(srcEp).AddSessionRef(id, *srcB2bEp);
        if (status != ER_OK) {
            assert(destEp.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL);
            QCC_LogError(status, ("AddSessionRef(this=%s, %u, %s) failed", srcEp.GetUniqueName().c_str(), id, srcB2bEp->GetUniqueName().c_str()));
            static_cast<VirtualEndpoint&>(destEp).RemoveSessionRef(id);
        }
    }

    /* Add sessionCast entries */
    if (status == ER_OK) {
        sessionCastSetLock.Lock(MUTEX_CONTEXT);
        SessionCastEntry entry(id, srcEp.GetUniqueName(), destB2bEp, &destEp);
        sessionCastSet.insert(entry);
        SessionCastEntry entry2(id, destEp.GetUniqueName(), srcB2bEp, &srcEp);
        sessionCastSet.insert(entry2);
        sessionCastSetLock.Unlock(MUTEX_CONTEXT);
    }
    return status;
}

QStatus DaemonRouter::RemoveSessionRoute(SessionId id, BusEndpoint& srcEp, BusEndpoint& destEp)
{
    QStatus status = ER_OK;
    RemoteEndpoint* srcB2bEp = NULL;
    RemoteEndpoint* destB2bEp = NULL;
    if (id == 0) {
        return ER_BUS_NO_SESSION;
    }

    /* Update virtual endpoint state */
    if (destEp.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL) {
        VirtualEndpoint& vDestEp = static_cast<VirtualEndpoint&>(destEp);
        destB2bEp = vDestEp.GetBusToBusEndpoint(id);
        vDestEp.RemoveSessionRef(id);
    }
    if (srcEp.GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL) {
        VirtualEndpoint& vSrcEp = static_cast<VirtualEndpoint&>(srcEp);
        srcB2bEp = vSrcEp.GetBusToBusEndpoint(id);
        vSrcEp.RemoveSessionRef(id);
    }

    /* Remove entries from sessionCastSet */
    if (status == ER_OK) {
        sessionCastSetLock.Lock(MUTEX_CONTEXT);
        SessionCastEntry entry(id, srcEp.GetUniqueName(), destB2bEp, &destEp);
        set<SessionCastEntry>::iterator it = sessionCastSet.find(entry);
        if (it != sessionCastSet.end()) {
            sessionCastSet.erase(it);
        }

        SessionCastEntry entry2(id, destEp.GetUniqueName(), srcB2bEp, &srcEp);
        set<SessionCastEntry>::iterator it2 = sessionCastSet.find(entry2);
        if (it2 != sessionCastSet.end()) {
            sessionCastSet.erase(it2);
        }
        sessionCastSetLock.Unlock(MUTEX_CONTEXT);
    }
    return status;
}

void DaemonRouter::RemoveSessionRoutes(const char* src, SessionId id)
{
    String srcStr = src;
    BusEndpoint* ep = FindEndpoint(srcStr);
    if (!ep) {
        QCC_LogError(ER_BUS_NO_ENDPOINT, ("Cannot find %s", src));
        return;
    }

    sessionCastSetLock.Lock(MUTEX_CONTEXT);
    set<SessionCastEntry>::iterator it = sessionCastSet.begin();
    while (it != sessionCastSet.end()) {
        if (((it->id == id) || (id == 0)) && ((it->src == src) || (it->destEp == ep))) {
            if ((it->id != 0) && (it->destEp->GetEndpointType() == BusEndpoint::ENDPOINT_TYPE_VIRTUAL)) {
                static_cast<VirtualEndpoint*>(it->destEp)->RemoveSessionRef(it->id);
            }
            sessionCastSet.erase(it++);
        } else {
            ++it;
        }
    }
    sessionCastSetLock.Unlock(MUTEX_CONTEXT);
}

}
