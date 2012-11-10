/**
 * @file
 * DaemonRouter is a "full-featured" router responsible for routing Bus messages
 * between one or more remote endpoints and a single local endpoint.
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
#ifndef _ALLJOYN_DAEMONROUTER_H
#define _ALLJOYN_DAEMONROUTER_H

#include <qcc/platform.h>

#include <qcc/Thread.h>

#include "Transport.h"

#include <Status.h>

#include "LocalTransport.h"
#include "Router.h"
#include "NameTable.h"
#include "RuleTable.h"

namespace ajn {

/**
 * @internal Forward delcarations
 */
class BusController;

/**
 * DaemonRouter is a "full-featured" router responsible for routing Bus messages
 * between one or more remote endpoints and a single local endpoint.
 */
class DaemonRouter : public Router {

    friend class TCPEndpoint;
    friend class UnixEndpoint;
    friend class LocalEndpoint;

  public:
    /**
     * Constructor
     */
    DaemonRouter();

    /**
     * Set the busController associated with this router.
     *
     * @param busController   The bus controller.
     */
    void SetBusController(BusController* busController) { this->busController = busController; }

    /**
     * Add a bus name listener.
     *
     * @param listener    Pointer to object that implements AllJoynNameListerer
     */
    void AddBusNameListener(NameListener* listener) { nameTable.AddListener(listener); }

    /**
     * Remote a bus name listener.
     *
     * @param listener    Pointer to object that implements AllJoynNameListerer
     */
    void RemoveBusNameListener(NameListener* listener) { nameTable.RemoveListener(listener); }

    /**
     * Set GUID of the bus.
     *
     * @param guid   GUID of bus associated with this router.
     */
    void SetGlobalGUID(const qcc::GUID128& guid) { nameTable.SetGUID(guid); }

    /**
     * Generate a unique endpoint name.
     *
     * @return A unique bus name that can be assigned to a (server-side) endpoint.
     */
    qcc::String GenerateUniqueName(void) { return nameTable.GenerateUniqueName(); }

    /**
     * Add a well-known (alias) bus name.
     *
     * @param aliasName    Alias (well-known) name of bus.
     * @param uniqueName   Unique name of endpoint attempting to own aliasName.
     * @param flags        AddAlias flags from NameTable.
     * @param disposition  [OUT] Outcome of add alias operation. Valid if return code is ER_OK.
     * @param listener     Optional listener whose AddAliasComplete method will be called if return code is ER_OK.
     * @param context      Optional context passed to listener.
     * @return  ER_OK if successful;
     */
    QStatus AddAlias(const qcc::String& aliasName,
                     const qcc::String& uniqueName,
                     uint32_t flags,
                     uint32_t& disposition,
                     NameListener* listener = NULL,
                     void* context = NULL)
    {
        return nameTable.AddAlias(aliasName, uniqueName, flags, disposition, listener, context);
    }

    /**
     * Remove a well-known bus name.
     *
     * @param aliasName     Well-known name to be removed.
     * @param ownerName     Unique name of owner of aliasName.
     * @param disposition   Outcome of remove alias operation. Valid only if return code is ER_OK.
     * @param listener      Optional listener whose RemoveAliasComplete method will be called if return code is ER_OK.
     * @param context       Optional context passed to listener.
     */
    void RemoveAlias(const qcc::String& aliasName,
                     const qcc::String& ownerName,
                     uint32_t& disposition,
                     NameListener* listener = NULL,
                     void* context = NULL)
    {
        nameTable.RemoveAlias(aliasName, ownerName, disposition, listener, context);
    }

    /**
     * Get a list of bus names.
     *
     * @param names  OUT Parameter: Vector of bus names.
     */
    void GetBusNames(std::vector<qcc::String>& names) const;

    /**
     * Find the endpoint that owns the given unique or well-known name.
     *
     * @param busname    Unique or well-known bus name
     * @return  Matching endpoint or NULL if none exists.
     */
    BusEndpoint* FindEndpoint(const qcc::String& busname);

    /**
     * Add a rule for an endpoint.
     *
     * @param endpoint   The endpoint that this rule applies to.
     * @param rule       Rule for endpoint
     * @return ER_OK if successful;
     */
    QStatus AddRule(BusEndpoint& endpoint, Rule& rule) { return ruleTable.AddRule(endpoint, rule); }

    /**
     * Remove a rule for an endpoint.
     *
     * @param endpoint    Endpoint that rule applies to.
     * @param rule        Rule to remove.
     * @return ER_OK if   successful;
     */
    QStatus RemoveRule(BusEndpoint& endpoint, Rule& rule) { return ruleTable.RemoveRule(endpoint, rule); }

    /**
     * Remove all rules for a given endpoint.
     *
     * @param endpoint    Endpoint whose rules will be removed.
     * @return ER_OK if successful;
     */
    QStatus RemoveAllRules(BusEndpoint& endpoint) { return ruleTable.RemoveAllRules(endpoint); }

    /**
     * Route an incoming Message Bus Message from an endpoint.
     *
     * @param sender  Endpoint that is sending the message
     * @param msg     Message to be processed.
     * @return ER_OK if successful.
     */
    QStatus PushMessage(Message& msg, BusEndpoint& sender);

    /**
     * Register an endpoint.
     * This method must be called by an endpoint before attempting to use the router.
     *
     * @param endpoint   Endpoint being registered.
     * @param isLocal    true iff endpoint is local.
     */
    QStatus RegisterEndpoint(BusEndpoint& endpoint, bool isLocal);

    /**
     * Un-register an endpoint.
     * This method must be called by an endpoint before the endpoint is deallocted.
     *
     * @param endpoint   Endpoint being registered.
     */
    void UnregisterEndpoint(BusEndpoint& endpoint);

    /**
     * Return true if this router is in contact with a bus (either locally or remotely)
     * This method can be used to determine whether messages sent to "the bus" will be routed.
     *
     * @return true iff the messages can be routed currently.
     */
    bool IsBusRunning(void) const { return NULL != localEndpoint; }

    /**
     * Indicate that this Bus instance is an AllJoyn daemon
     *
     * @return true since DaemonRouter is always part of an AllJoyn daemon.
     */
    bool IsDaemon() const { return true; }

    /**
     * Lock name table
     */
    void LockNameTable() { nameTable.Lock(); }

    /**
     * Unlock name table
     */
    void UnlockNameTable() { nameTable.Unlock(); }

    /**
     * Get all unique names and their exportable alias (well-known) names.
     *
     * @param  nameVec   Vector of (uniqueName, aliases) pairs where aliases is a vector of alias names.
     */
    void GetUniqueNamesAndAliases(std::vector<std::pair<qcc::String, std::vector<qcc::String> > >& nameVec) const
    {
        nameTable.GetUniqueNamesAndAliases(nameVec);
    }

    /**
     * Get all the unique names that are in queue for the same alias (well-known) name
     *
     * @param[in] busName (well-known) name
     * @param[out] names vecter of uniqueNames in queue for the
     */
    void GetQueuedNames(const qcc::String& busName, std::vector<qcc::String>& names)
    {
        nameTable.GetQueuedNames(busName, names);
    }
    /**
     * Set (or clear) a virtual alias.
     * A virtual alias is a well-known bus name for a virtual endpoint.
     * Virtual aliases differ from regular aliases in that the local bus controller
     * does not handle name queueing. It is up to the remote endpoint to manange
     * the queueing for such aliases.
     *
     * @param alias        The virtual alias being modified.
     * @param newOwnerEp   The VirtualEndpoint that is the new owner of alias or NULL if none.
     * @param requestingEp A Virtual endpoint from the remote daemon that is requesting this change.
     * @return  true iff alias was a change to the name table
     */
    bool SetVirtualAlias(const qcc::String& alias, VirtualEndpoint* newOwnerEp, VirtualEndpoint& requestingEp)
    {
        return nameTable.SetVirtualAlias(alias, newOwnerEp, requestingEp);
    }

    /**
     * Remove well-known names associated with a virtual endpoint.
     *
     * @param vep    Virtual endpoint whose well-known names are to be removed.
     */
    void RemoveVirtualAliases(VirtualEndpoint& vep)
    {
        nameTable.RemoveVirtualAliases(vep);
    }

    /**
     * Add a session route.
     *
     * @param  id          Session Id.
     * @param  srcEp       Route source endpoint.
     * @param  srcB2bEp    Source B2B endpoint. (NULL if srcEp is not virtual).
     * @param  destEp      BusEndpoint of route destination.
     * @param  destB2bEp   [IN/OUT] If passed in as NULL, attempt to use qosHint to choose destB2bEp and return selected ep.
     * @param  optsHint    Optional session options constraint for selection of destB2bEp if not explicitly specified.
     * @return  ER_OK if successful.
     */
    QStatus AddSessionRoute(SessionId id, BusEndpoint& srcEp, RemoteEndpoint* srcB2bEp, BusEndpoint& destEp,
                            RemoteEndpoint*& destB2bEp, SessionOpts* optsHint = NULL);

    /**
     * Remove a (single) session route.
     *
     * @param  id      Session Id.
     * @param  srcEp   BusEndpoint of route source.
     * @param  destEp  BusEndpoint of route destination.
     * @return  ER_OK if successful.
     */
    QStatus RemoveSessionRoute(SessionId id, BusEndpoint& srcEp, BusEndpoint& destEp);

    /**
     * Remove existing session routes.
     * This method removes routes that involve uniqueName as a source or as a destination for a particular session id.
     * When sessionId is 0, all routes that involved uniqueName are removed.
     *
     * @param  uniqueName  Unique name.
     * @param  id          Session id or 0 to indicate "all sessions".
     */
    void RemoveSessionRoutes(const char* uniqueName, SessionId id);

  private:
    LocalEndpoint* localEndpoint;   /**< The local endpoint */
    RuleTable ruleTable;            /**< Routing rule table */
    NameTable nameTable;            /**< BusName to transport lookupl table */
    BusController* busController;   /**< The bus controller used with this router */

    std::set<RemoteEndpoint*> m_b2bEndpoints;  /**< Collection of Bus-to-bus endpoints */
    qcc::Mutex m_b2bEndpointsLock;       /**< Lock that protects m_b2bEndpoints */

    /** Session multicast destination map */
    struct SessionCastEntry {
        SessionId id;
        qcc::String src;
        RemoteEndpoint* b2bEp;
        BusEndpoint* destEp;

        SessionCastEntry(SessionId id, const qcc::String& src, RemoteEndpoint* b2bEp, BusEndpoint* destEp) :
            id(id), src(src), b2bEp(b2bEp), destEp(destEp) { }

        bool operator<(const SessionCastEntry& other) const {
            return (id < other.id) || ((id == other.id) && ((src < other.src) || ((src == other.src) && ((b2bEp < other.b2bEp) || ((b2bEp == other.b2bEp) && (destEp < other.destEp))))));
        }

        bool operator==(const SessionCastEntry& other) const {
            return (id == other.id)  && (src == other.src) && (b2bEp == other.b2bEp) && (destEp == other.destEp);
        }
    };
    std::set<SessionCastEntry> sessionCastSet;
    qcc::Mutex sessionCastSetLock;      /**< Lock that protects sessionCastSet */
};

}

#endif
