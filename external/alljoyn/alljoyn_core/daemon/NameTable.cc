/**
 * @file
 * NameTable is a thread-safe mapping between unique/well-known
 * bus names and the BusEndpoint that these names exist on.
 * This mapping is many (names) to one (endpoint). Every endpoint has
 * exactly one unique name and zero or more well-known names.
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

#include <assert.h>

#include <qcc/Debug.h>
#include <qcc/Logger.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include "NameTable.h"
#include "VirtualEndpoint.h"

#include <alljoyn/DBusStd.h>

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

qcc::String NameTable::GenerateUniqueName(void)
{
    return uniquePrefix + U32ToString(IncrementAndFetch((int32_t*)&uniqueId));
}

void NameTable::SetGUID(const qcc::GUID128& guid)
{
    Log(LOG_INFO, "AllJoyn Daemon GUID = %s (%s)\n", guid.ToString().c_str(), guid.ToShortString().c_str());
    uniquePrefix = ":";
    uniquePrefix.append(guid.ToShortString());
    uniquePrefix.append(".");
}

void NameTable::AddUniqueName(BusEndpoint& endpoint)
{
    QCC_DbgTrace(("NameTable::AddUniqueName(%s)", endpoint.GetUniqueName().c_str()));

    const qcc::String& uniqueName = endpoint.GetUniqueName();
    QCC_DbgPrintf(("Add unique name %s", uniqueName.c_str()));
    lock.Lock(MUTEX_CONTEXT);
    uniqueNames[uniqueName] = &endpoint;
    lock.Unlock(MUTEX_CONTEXT);

    /* Notify listeners */
    CallListeners(uniqueName, NULL, &uniqueName);
}

void NameTable::RemoveUniqueName(const qcc::String& uniqueName)
{
    QCC_DbgTrace(("RemoveUniqueName %s", uniqueName.c_str()));

    /* Erase the unique bus name and any well-known names that use the same endpoint */
    lock.Lock(MUTEX_CONTEXT);
    unordered_map<qcc::String, BusEndpoint*, Hash, Equal>::iterator it = uniqueNames.find(uniqueName);
    if (it != uniqueNames.end()) {
        BusEndpoint* endpoint = it->second;

        /* Remove well-known names asssociated with uniqueName */
        unordered_map<qcc::String, deque<NameQueueEntry>, Hash, Equal>::iterator ait = aliasNames.begin();
        while (ait != aliasNames.end()) {
            deque<NameQueueEntry>::iterator lit = ait->second.begin();
            bool startOver = false;
            while (lit != ait->second.end()) {
                if (lit->endpointName == endpoint->GetUniqueName()) {
                    if (lit == ait->second.begin()) {
                        uint32_t disposition;
                        RemoveAlias(ait->first, endpoint->GetUniqueName(), disposition, NULL, NULL);
                        if (DBUS_RELEASE_NAME_REPLY_RELEASED == disposition) {
                            ait = aliasNames.begin();
                            startOver = true;
                            break;
                        } else {
                            QCC_LogError(ER_FAIL, ("Failed to release %s from %s",
                                                   ait->first.c_str(),
                                                   endpoint->GetUniqueName().c_str()));
                            break;
                        }
                    } else {
                        ait->second.erase(lit);
                        break;
                    }
                } else {
                    ++lit;
                }
            }
            if (!startOver) {
                ++ait;
            }
        }

        uniqueNames.erase(it);
        lock.Unlock(MUTEX_CONTEXT);
        QCC_DbgPrintf(("Removed ep=%s from name table", uniqueName.c_str()));

        /* Notify listeners */
        CallListeners(uniqueName, &uniqueName, NULL);
    } else {
        lock.Unlock(MUTEX_CONTEXT);
    }
}

QStatus NameTable::AddAlias(const qcc::String& aliasName,
                            const qcc::String& uniqueName,
                            uint32_t flags,
                            uint32_t& disposition,
                            NameListener* listener,
                            void* context)
{
    QStatus status;

    QCC_DbgTrace(("NameTable: AddAlias(%s, %s)", aliasName.c_str(), uniqueName.c_str()));

    lock.Lock(MUTEX_CONTEXT);
    unordered_map<qcc::String, BusEndpoint*, Hash, Equal>::const_iterator it = uniqueNames.find(uniqueName);
    if (it != uniqueNames.end()) {
        unordered_map<qcc::String, deque<NameQueueEntry>, Hash, Equal>::iterator wasIt = aliasNames.find(aliasName);
        NameQueueEntry entry = { uniqueName, flags };
        const qcc::String* origOwner = NULL;
        const qcc::String* newOwner = NULL;

        if (wasIt != aliasNames.end()) {
            assert(!wasIt->second.empty());
            const NameQueueEntry& primary = wasIt->second[0];
            if (primary.endpointName == uniqueName) {
                /* Enpoint already owns this alias */
                disposition = DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER;
            } else if ((primary.flags & DBUS_NAME_FLAG_ALLOW_REPLACEMENT) && (flags & DBUS_NAME_FLAG_REPLACE_EXISTING)) {
                /* Make endpoint the current owner */
                wasIt->second.push_front(entry);
                disposition = DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
                origOwner = &primary.endpointName;
                newOwner = &uniqueName;
            } else {
                if (flags & DBUS_NAME_FLAG_DO_NOT_QUEUE) {
                    /* Cannot replace current owner */
                    disposition = DBUS_REQUEST_NAME_REPLY_EXISTS;
                } else {
                    /* Add this new potential owner to the end of the list */
                    wasIt->second.push_back(entry);
                    disposition = DBUS_REQUEST_NAME_REPLY_IN_QUEUE;
                }
            }
        } else {
            /* No pre-existing queue for this name */
            aliasNames[aliasName] = deque<NameQueueEntry>(1, entry);
            disposition = DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
            newOwner = &uniqueName;

            /* Check to see if we are overriding a virtual (remote) name */
            map<qcc::StringMapKey, VirtualEndpoint*>::const_iterator vit = virtualAliasNames.find(aliasName);
            if (vit != virtualAliasNames.end()) {
                origOwner = &vit->second->GetUniqueName();
            }
        }
        lock.Unlock(MUTEX_CONTEXT);

        if (listener) {
            listener->AddAliasComplete(aliasName, disposition, context);
        }
        if (newOwner) {
            CallListeners(aliasName, origOwner, newOwner);
        }
        status = ER_OK;
    } else {
        status = ER_BUS_NO_ENDPOINT;
        lock.Unlock(MUTEX_CONTEXT);
    }
    return status;
}

void NameTable::RemoveAlias(const qcc::String& aliasName,
                            const qcc::String& ownerName,
                            uint32_t& disposition,
                            NameListener* listener,
                            void* context)
{
    const qcc::String* oldOwner = NULL;
    const qcc::String* newOwner = NULL;
    qcc::String aliasNameCopy(aliasName);

    QCC_DbgTrace(("NameTable: RemoveAlias(%s, %s)", aliasName.c_str(), ownerName.c_str()));

    lock.Lock(MUTEX_CONTEXT);

    /* Find endpoint for aliasName */
    unordered_map<qcc::String, deque<NameQueueEntry>, Hash, Equal>::iterator it = aliasNames.find(aliasName);
    if (it != aliasNames.end()) {
        deque<NameQueueEntry>& queue = it->second;

        assert(!queue.empty());
        if (queue[0].endpointName == ownerName) {
            /* Remove primary */
            if (queue.size() > 1) {
                queue.pop_front();
                BusEndpoint* ep = FindEndpoint(queue[0].endpointName);
                newOwner = ep ? &queue[0].endpointName : NULL;
            }
            if (!newOwner) {
                /* Check to see if there is a (now unmasked) remote owner for the alias */
                map<qcc::StringMapKey, VirtualEndpoint*>::const_iterator vit = virtualAliasNames.find(aliasName);
                if (vit != virtualAliasNames.end()) {
                    newOwner = &vit->second->GetUniqueName();
                }
                aliasNames.erase(it);
            }
            oldOwner = &ownerName;
            disposition = DBUS_RELEASE_NAME_REPLY_RELEASED;
        } else {
            /* Alias is not owned by ownerName */
            disposition = DBUS_RELEASE_NAME_REPLY_NOT_OWNER;
        }
    } else {
        disposition = DBUS_RELEASE_NAME_REPLY_NON_EXISTENT;
    }

    lock.Unlock(MUTEX_CONTEXT);

    if (listener) {
        listener->RemoveAliasComplete(aliasNameCopy, disposition, context);
    }
    if (oldOwner) {
        CallListeners(aliasNameCopy, oldOwner, newOwner);
    }
}

BusEndpoint* NameTable::FindEndpoint(const qcc::String& busName) const
{
    BusEndpoint* ret = NULL;

    lock.Lock(MUTEX_CONTEXT);
    if (busName[0] == ':') {
        unordered_map<qcc::String, BusEndpoint*, Hash, Equal>::const_iterator it = uniqueNames.find(busName);
        if (it != uniqueNames.end()) {
            ret = it->second;
        }

    } else {
        unordered_map<qcc::String, deque<NameQueueEntry>, Hash, Equal>::const_iterator it = aliasNames.find(busName);
        if (it != aliasNames.end()) {
            assert(!it->second.empty());
            ret = FindEndpoint(it->second[0].endpointName);
        }
        /* Fallback to virtual (remote) aliases if a suitable local one cannot be found */
        if (NULL == ret) {
            map<qcc::StringMapKey, VirtualEndpoint*>::const_iterator vit = virtualAliasNames.find(busName);
            if (vit != virtualAliasNames.end()) {
                ret = vit->second;
            }
        }
    }
    lock.Unlock(MUTEX_CONTEXT);
    return ret;
}

void NameTable::GetBusNames(vector<qcc::String>& names) const
{
    lock.Lock(MUTEX_CONTEXT);

    unordered_map<qcc::String, deque<NameQueueEntry>, Hash, Equal>::const_iterator it = aliasNames.begin();
    while (it != aliasNames.end()) {
        names.push_back(it->first);
        ++it;
    }
    unordered_map<qcc::String, BusEndpoint*, Hash, Equal>::const_iterator uit = uniqueNames.begin();
    while (uit != uniqueNames.end()) {
        names.push_back(uit->first);
        ++uit;
    }
    lock.Unlock(MUTEX_CONTEXT);
}

void NameTable::GetUniqueNamesAndAliases(vector<pair<qcc::String, vector<qcc::String> > >& names) const
{

    /* Create a intermediate map to avoid N^2 perf */
    multimap<const BusEndpoint*, qcc::String> epMap;
    lock.Lock(MUTEX_CONTEXT);
    unordered_map<qcc::String, BusEndpoint*, Hash, Equal>::const_iterator uit = uniqueNames.begin();
    while (uit != uniqueNames.end()) {
        epMap.insert(pair<const BusEndpoint*, qcc::String>(uit->second, uit->first));
        ++uit;
    }
    unordered_map<qcc::String, deque<NameQueueEntry>, Hash, Equal>::const_iterator ait = aliasNames.begin();
    while (ait != aliasNames.end()) {
        if (!ait->second.empty()) {
            BusEndpoint* ep = FindEndpoint(ait->second.front().endpointName);
            if (ep) {
                epMap.insert(pair<const BusEndpoint*, qcc::String>(ep, ait->first));
            }
        }
        ++ait;
    }
    map<StringMapKey, VirtualEndpoint*>::const_iterator vit = virtualAliasNames.begin();
    while (vit != virtualAliasNames.end()) {
        epMap.insert(pair<const BusEndpoint*, qcc::String>(vit->second, vit->first.c_str()));
        ++vit;
    }
    lock.Unlock(MUTEX_CONTEXT);

    /* Fill in the caller's vector */
    qcc::String uniqueName;
    vector<qcc::String> aliasVec;
    const BusEndpoint* lastEp = NULL;
    multimap<const BusEndpoint*, qcc::String>::iterator it = epMap.begin();
    names.reserve(uniqueNames.size());  // prevent dynamic resizing in loop
    while (true) {
        if ((it == epMap.end()) || (lastEp != it->first)) {
            if (!uniqueName.empty()) {
                names.push_back(pair<qcc::String, vector<qcc::String> >(uniqueName, aliasVec));
            }
            uniqueName.clear();
            aliasVec.clear();
            if (it == epMap.end()) {
                break;
            }
        }
        if (it->second[0] == ':') {
            uniqueName = it->second;
        } else {
            aliasVec.push_back(it->second);
        }
        lastEp = it->first;
        ++it;
    }
}

void NameTable::GetQueuedNames(const qcc::String& busName, std::vector<qcc::String>& names)
{
    unordered_map<qcc::String, deque<NameQueueEntry>, Hash, Equal>::iterator ait = aliasNames.find(busName.c_str());
    if (ait != aliasNames.end()) {

        names.reserve(ait->second.size()); //prevent dynamic resizing in loop
        for (deque<NameQueueEntry>::iterator lit = ait->second.begin(); lit != ait->second.end(); ++lit) {
            names.push_back(lit->endpointName);
        }
    } else {
        names.clear();
    }
}

void NameTable::RemoveVirtualAliases(VirtualEndpoint& ep)
{
    QCC_DbgTrace(("NameTable::RemoveVirtualAliases(%s)", ep.GetUniqueName().c_str()));

    lock.Lock(MUTEX_CONTEXT);
    map<qcc::StringMapKey, VirtualEndpoint*>::iterator vit = virtualAliasNames.begin();
    while (vit != virtualAliasNames.end()) {
        if (vit->second == &ep) {
            String alias = vit->first.c_str();
            String epName = ep.GetUniqueName();
            virtualAliasNames.erase(vit++);
            if (aliasNames.find(alias) == aliasNames.end()) {
                lock.Unlock(MUTEX_CONTEXT);
                CallListeners(alias, &epName, NULL);
                lock.Lock(MUTEX_CONTEXT);
                vit = virtualAliasNames.upper_bound(alias);
            }
        } else {
            ++vit;
        }
    }
    lock.Unlock(MUTEX_CONTEXT);
}

bool NameTable::SetVirtualAlias(const qcc::String& alias,
                                VirtualEndpoint* newOwner,
                                VirtualEndpoint& requestingEndpoint)
{
    QCC_DbgTrace(("NameTable::SetVirtualAlias(%s, %s, %s)", alias.c_str(), newOwner ? newOwner->GetUniqueName().c_str() : "<none>", requestingEndpoint.GetUniqueName().c_str()));

    lock.Lock(MUTEX_CONTEXT);

    map<qcc::StringMapKey, VirtualEndpoint*>::iterator vit = virtualAliasNames.find(alias);
    BusEndpoint* oldOwner = (vit == virtualAliasNames.end()) ? NULL : vit->second;

    /*
     * Virtual aliases cannot directly change ownership from one remote daemon to another.
     * Allowing this would allow a daemon to "take" an existing name from another daemon.
     * Name changes are allowed within the same remote daemon or when the name is not already
     * owned.
     */
    if (oldOwner) {
        const String& oldOwnerName = oldOwner->GetUniqueName();
        const String& reqOwnerName = requestingEndpoint.GetUniqueName();
        size_t oldPeriodOff = oldOwnerName.find_first_of('.');
        size_t reqPeriodOff = reqOwnerName.find_first_of('.');
        if ((oldPeriodOff == String::npos) || (0 != oldOwnerName.compare(0, oldPeriodOff, reqOwnerName, 0, reqPeriodOff))) {
            lock.Unlock(MUTEX_CONTEXT);
            return false;
        }
    }

    bool madeChange = (oldOwner != newOwner);
    bool maskingLocalName = (aliasNames.find(alias) != aliasNames.end());

    if (newOwner) {
        virtualAliasNames[alias] = newOwner;
    } else {
        virtualAliasNames.erase(StringMapKey(alias));
    }
    lock.Unlock(MUTEX_CONTEXT);

    /* Virtual aliases cannot override locally requested aliases */
    if (madeChange && !maskingLocalName) {
        CallListeners(alias,
                      oldOwner ? &(oldOwner->GetUniqueName()) : NULL,
                      newOwner ? &(newOwner->GetUniqueName()) : NULL);
    }
    return madeChange;
}

void NameTable::AddListener(NameListener* listener)
{
    lock.Lock(MUTEX_CONTEXT);
    listeners.insert(ProtectedNameListener(listener));
    lock.Unlock(MUTEX_CONTEXT);
}

void NameTable::RemoveListener(NameListener* listener)
{
    lock.Lock(MUTEX_CONTEXT);
    ProtectedNameListener pl(listener);
    set<ProtectedNameListener>::iterator it = listeners.find(pl);
    if (it != listeners.end()) {
        /* Remove listener from set */
        listeners.erase(it);

        /* Wait until references to pl reach q (pl is only remaining ref) */
        while (pl.GetRefCount() > 1) {
            lock.Unlock(MUTEX_CONTEXT);
            qcc::Sleep(4);
            lock.Lock(MUTEX_CONTEXT);
        }
    }
    lock.Unlock(MUTEX_CONTEXT);
}

void NameTable::CallListeners(const qcc::String& aliasName, const qcc::String* origOwner, const qcc::String* newOwner)
{
    lock.Lock(MUTEX_CONTEXT);
    set<ProtectedNameListener>::iterator it = listeners.begin();
    while (it != listeners.end()) {
        ProtectedNameListener nl = *it;
        lock.Unlock(MUTEX_CONTEXT);
        (*nl)->NameOwnerChanged(aliasName, origOwner, newOwner);
        lock.Lock(MUTEX_CONTEXT);
        it = listeners.upper_bound(nl);
    }
    lock.Unlock(MUTEX_CONTEXT);
}

}
