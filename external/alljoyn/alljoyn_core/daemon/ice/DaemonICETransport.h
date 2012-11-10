/**
 * @file
 * DaemonICETransport is a specialization of class Transport for daemons talking over
 * ICE.
 */

/******************************************************************************
 * Copyright 2012 Qualcomm Innovation Center, Inc.
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

#ifndef _ALLJOYN_DAEMONICETRANSPORT_H
#define _ALLJOYN_DAEMONICETRANSPORT_H

#ifndef __cplusplus
#error Only include DaemonICETransport.h in C++ code.
#endif

#include <qcc/platform.h>

#include <set>

#include <qcc/String.h>
#include <qcc/Mutex.h>
#include <qcc/Thread.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/time.h>

#include "Transport.h"
#include "RemoteEndpoint.h"
#include "Status.h"
#include "DiscoveryManager.h"
#include "ICESessionListener.h"
#include "PeerCandidateListener.h"
#include "PacketEngine.h"
#include "TokenRefreshListener.h"

#ifdef QCC_OS_GROUP_POSIX
#include "posix/ICEPacketStream.h"
#endif

using namespace qcc;


// Maximum time in milli seconds that an AllJoyn Client connect request will wait to
// receive the candidates from the Service on the remote daemon.
// Units in ms
// PPN - Need to review this time
const uint32_t ICE_CLIENT_SESSION_WAIT_TIMEOUT = 15000;

// Maximum time in milli seconds that the DaemonICETransport will wait for the ICESession to allocate an ICE session
// PPN - Need to review this time
const uint32_t ICE_ALLOCATE_SESSION_WAIT_TIMEOUT = 15000;

// Maximum time in milli seconds that the DaemonICETransport will wait to receive the new refreshed tokens
// PPN - Need to review this time
const uint32_t ICE_REFRESH_TOKENS_WAIT_TIMEOUT = 15000;

/* Assuming that the MTU size of the interface is 1500 bytes, the total STUN overhead when sending
 * data through the relay is 172 bytes/packet. Hence the max MTU for data would be 1328 bytes.
 * This needs to be hard-coded here because PacketEngine does not allow us to set the MTU per
 * packet stream instead it needs to know the max value when it is started so that it may allocate
 * packet pools appropriately.*/
const uint32_t MAX_ICE_MTU = 1328;

namespace ajn {

class DaemonICEEndpoint;

/* Class providing a callback mechanism used by the ICESession to notify updates to the DaemonICETransport */
class ICESessionListenerImpl : public ICESessionListener {
  public:
    ICESessionListenerImpl() : waitEvent(), state(ICESession::ICEProcessingFailed) { }

    void ICESessionChanged(ICESession* session)
    {
        state = session->GetState();

        waitEvent.SetEvent();
    }

    ICESession::ICESessionState GetState() { return state; }

    QStatus Wait()
    {
        QStatus status = Event::Wait(waitEvent, ICE_ALLOCATE_SESSION_WAIT_TIMEOUT);
        if (ER_OK == status) {
            waitEvent.ResetEvent();
        }
        return status;
    }

  private:
    Event waitEvent;
    ICESession::ICESessionState state;
};

/* Class providing a callback mechanism used by the DiscoveryManager to notify availability of Peer Candidates to the DaemonICETransport */
class PeerCandidateListenerImpl : public PeerCandidateListener {
  public:

    PeerCandidateListenerImpl() : waitEvent(), ice_frag(), ice_pwd() { }

    void SetPeerCandiates(list<ICECandidates>& candidates, const String& frag, const String& pwd)
    {
        peerCandidates = candidates;
        ice_frag = frag;
        ice_pwd = pwd;
        waitEvent.SetEvent();
    }

    void GetPeerCandiates(list<ICECandidates>& candidates, String& frag, String& pwd)
    {
        candidates = peerCandidates;
        frag = ice_frag;
        pwd = ice_pwd;
    }

    QStatus Wait()
    {
        QStatus status = Event::Wait(waitEvent, ICE_CLIENT_SESSION_WAIT_TIMEOUT);
        if (ER_OK == status) {
            waitEvent.ResetEvent();
        }
        return status;
    }

  private:
    Event waitEvent;
    list<ICECandidates> peerCandidates;
    String ice_frag;
    String ice_pwd;
};

/* Class providing a callback mechanism used by the DiscoveryManager to notify availability of new tokens to the DaemonICETransport */
class TokenRefreshListenerImpl : public TokenRefreshListener {
  public:

    TokenRefreshListenerImpl() : waitEvent(), acct(), pwd(), expiryTime(0), recvTime(0) { }

    void SetTokens(String newAcct, String newPwd, uint32_t recvtime, uint32_t expTime)
    {
        acct = newAcct;
        pwd = newPwd;
        recvTime = recvtime;
        expiryTime = expTime;
        waitEvent.SetEvent();
    }

    void GetTokens(String& newAcct, String& newPwd, uint32_t& recvtime, uint32_t& expTime)
    {
        newAcct = acct;
        newPwd = pwd;
        recvtime = recvTime;
        expTime = expiryTime;
    }

    QStatus Wait()
    {
        QStatus status = Event::Wait(waitEvent, ICE_REFRESH_TOKENS_WAIT_TIMEOUT);
        if (ER_OK == status) {
            waitEvent.ResetEvent();
        }
        return status;
    }

  private:
    Event waitEvent;
    String acct;
    String pwd;
    uint32_t expiryTime;
    uint32_t recvTime;
};

/**
 * @brief A class for ICE Transports used in daemons.
 *
 * The DaemonICETransport class has different incarnations depending on whether or not
 * an instantiated endpoint using the transport resides in a daemon, or in the
 * case of Windows, on a service or client.  The differences between these
 * versions revolves around routing and discovery. This class provides a
 * specialization of class Transport for use by daemons.
 */
class DaemonICETransport : public Transport, public RemoteEndpoint::EndpointListener, public Thread, public AlarmListener, public PacketEngineListener {
    friend class DaemonICEEndpoint;

  public:

    /**
     * Create a ICE based transport for use by daemons.
     *
     * @param bus The BusAttachment associated with this endpoint
     */
    DaemonICETransport(BusAttachment& bus);

    /**
     * Destructor
     */
    virtual ~DaemonICETransport();

    /**
     * Start the transport and associate it with a router.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Start();

    /**
     * Stop the transport.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Stop();

    /**
     * Pend the caller until the transport stops.
     *
     * @return
     *      - ER_OK if successful
     *      - an error status otherwise.
     */
    QStatus Join();

    /**
     * Determine if this transport is running. Running means Start() has been called.
     *
     * @return  Returns true if the transport is running.
     */
    bool IsRunning() { return Thread::IsRunning(); }

    /**
     * @internal
     * @brief Normalize a transport specification.
     *
     * Given a transport specification, convert it into a form which is guaranteed to
     * have a one-to-one relationship with a connection instance.
     *
     * @param inSpec    Input transport connect spec.
     * @param outSpec   Output transport connect spec.
     * @param argMap    Parsed parameter map.
     *
     * @return ER_OK if successful.
     */
    QStatus NormalizeTransportSpec(const char* inSpec, String& outSpec, std::map<String, String>& argMap) const;

    /**
     * Connect to a specified remote AllJoyn/DBus address.
     *
     * @param connectSpec    Transport specific key/value args used to configure the client-side endpoint.
     *                       The form of this string is @c "<transport>:<key1>=<val1>,<key2>=<val2>..."
     * @param opts           Requested sessions opts.
     * @param newep          [OUT] Endpoint created as a result of successful connect.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Connect(const char* connectSpec, const SessionOpts& opts, BusEndpoint** newep);

    /**
     * Disconnect from a specified AllJoyn/DBus address.
     *
     * @param connectSpec    The connectSpec used in Connect.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Disconnect(const char* connectSpec);

    /**
     * Start listening for incomming connections on a specified bus address.
     *
     * @param listenSpec  Transport specific key/value arguments that specify the physical interface to listen on.
     *                    - Valid transport is @c "tcp". All others ignored.
     *                    - Valid keys are:
     *                        - @c addr = IP address of server to connect to.
     *                        - @c port = Port number of server to connect to.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus StartListen(const char* listenSpec);

    /**
     * @brief Stop listening for incoming connections on a specified bus address.
     *
     * This method cancels a StartListen request. Therefore, the listenSpec must
     * match previous call to StartListen().
     *
     * @param listenSpec  Transport specific key/value arguments that specify the physical interface to listen on.
     *                    - Valid transport is @c "ice". All others ignored.
     *                    - Valid keys are:
     *                        - @c addr = IP address of server to connect to.
     *                        - @c port = Port number of server to connect to.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus StopListen(const char* listenSpec);

    /**
     * Set a listener for transport related events.  There can only be one
     * listener set at a time. Setting a listener implicitly removes any
     * previously set listener.
     *
     * @param listener  Listener for transport related events.
     */
    void SetListener(TransportListener* listener) { m_listener = listener; }

    /**
     * Indicates whether this transport is used for client-to-bus or bus-to-bus connections.
     *
     * @return  Always returns true, ICE is a bus-to-bus transport.
     */
    bool IsBusToBus() const { return true; }

    /**
     * @internal
     * @brief Start discovering busses.
     */
    void EnableDiscovery(const char* namePrefix);

    /**
     * @internal
     * @brief Stop discovering busses to connect to.
     */
    void DisableDiscovery(const char* namePrefix);

    /**
     * Start advertising a well-known name with the given quality of service.
     *
     * @param advertiseName   Well-known name to add to list of advertised names.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus EnableAdvertisement(const String& advertiseName);

    /**
     * Stop advertising a well-known name with a given quality of service.
     *
     * @param advertiseName   Well-known name to remove from list of advertised names.
     * @param nameListEmpty   Indicates whether advertise name list is completely empty (safe to disable OTA advertising).
     */
    void DisableAdvertisement(const String& advertiseName, bool nameListEmpty);

    /**
     * Returns the name of this transport
     */
    const char* GetTransportName() const { return TransportName; }

    /**
     * Get the transport mask for this transport
     *
     * @return the TransportMask for this transport.
     */
    TransportMask GetTransportMask() const { return TRANSPORT_ICE; }

    /**
     * Get a list of the possible listen specs of the current Transport for a
     * given set of session options.
     *
     * Session options specify high-level characteristics of session, such as
     * whether or not the underlying transport carries data encapsulated in
     * AllJoyn messages, and whether or not delivery is reliable.
     *
     * It is possible that there is more than one answer to the question: what
     * abstract address should I use when talking to another endpoint.  Each
     * Transports is equipped to understand how many answers there are and also
     * which answers are better than the others.  This method fills in the
     * provided vector with a list of currently available busAddresses ordered
     * according to which the transport thinks would be best.
     *
     * If there are no addresses appropriate to the given session options the
     * provided vector of String is left unchanged.  If there are addresses,
     * they are added at the end of the provided vector.
     *
     * @param opts Session options describing the desired characteristics of
     *             an underlying session
     * @param busAddrs A vector of String to which bus addresses corresponding
     *                 to IFF_UP interfaces matching the desired characteristics
     *                 are added.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus GetListenAddresses(const SessionOpts& opts, std::vector<String>& busAddrs) const;

    /**
     * Callback for DaemonICEEndpoint exit.
     *
     * @param endpoint   DaemonICEEndpoint instance that has exited.
     */
    void EndpointExit(RemoteEndpoint* endpoint);

    struct AlarmContext {

        enum ContextType {
            CONTEXT_NAT_KEEPALIVE,
            CONTEXT_SCHEDULE_RUN
        };

        AlarmContext() : contextType(CONTEXT_SCHEDULE_RUN) { }

        AlarmContext(ICEPacketStream* stream) : contextType(CONTEXT_NAT_KEEPALIVE), pktStream(stream) { }

        virtual ~AlarmContext() { }

        ContextType contextType;

        ICEPacketStream* pktStream;
    };

    /**
     * Function to check if the TURN server tokens validity has expired.
     *
     * @return  true if the tokens have not expired, false otherwise.
     */
    bool CheckTURNTokenExpiry(STUNServerInfo stunInfo);

    /**
     * Function to retrieve the .
     *
     * @return  true if the tokens have not expired, false otherwise.
     */
    QStatus GetNewTokensFromServer(bool client, STUNServerInfo& stunInfo, String remotePeerAddress);

    /**
     * @internal
     * @brief Alarm handler
     */
    void AlarmTriggered(const qcc::Alarm& alarm, QStatus status);

    /**
     * PacketEngineAccept callback
     */
    bool PacketEngineAcceptCB(PacketEngine& engine, const PacketEngineStream& stream, const PacketDest& dest);

    /**
     * PacketEngineConnect callback
     */
    void PacketEngineConnectCB(PacketEngine& engine, QStatus status, const PacketEngineStream* stream, const PacketDest& dest, void* context);

    /**
     * PacketEngineDisconnect callback
     */
    void PacketEngineDisconnectCB(PacketEngine& engine, const PacketEngineStream& stream, const PacketDest& dest);

    /**
     * Name of transport used in transport specs.
     */
    static const char* TransportName;

  private:

    DaemonICETransport(const DaemonICETransport& other);
    DaemonICETransport& operator =(const DaemonICETransport& other);

    BusAttachment& m_bus;                                          /**< The message bus for this transport */
    DiscoveryManager* m_dm;                                        /**< The Discovery Manager used for discovery */
    ICEManager m_iceManager;                                       /**< The ICE Manager used for managing ICE operations */
    bool m_stopping;                                               /**< True if Stop() has been called but endpoints still exist */
    TransportListener* m_listener;                                 /**< Registered TransportListener */
    std::set<DaemonICEEndpoint*> m_authList;                       /**< Set of authenticating endpoints */
    std::set<DaemonICEEndpoint*> m_endpointList;                   /**< Set of active endpoints */
    Mutex m_endpointListLock;                                      /**< Mutex that protects the endpoint and auth lists */

    ///< Event that indicates that a new AllocateICESession request has been received.
    Event wakeDaemonICETransportRun;

    /* Instance of the packet engine associated with the ICE transport*/
    PacketEngine m_packetEngine;

    Mutex m_IncomingICESessionsLock; /**< Mutex that protects IncomingICESessions */

    /*
     * List of the GUIDs of the remote daemons trying to connected to this daemon.
     */
    list<String> IncomingICESessions;

    /** AllocateICESessionThread handles a AllocateICESession request from a remote client on a separate thread */
    class AllocateICESessionThread : public Thread, public ThreadListener {
      public:
        AllocateICESessionThread(DaemonICETransport* transportObj, String clientGUID)
            : Thread("AllocateICESessionThread"), transportObj(transportObj), clientGUID(clientGUID), pktStream(NULL) { }

        /* Virtual destructor is needed to ensure that base class destructors get run */
        virtual ~AllocateICESessionThread() { }

        /* Called when thread instance exits */
        void ThreadExit(Thread* thread);

      protected:
        ThreadReturn STDCALL Run(void* arg);

      private:
        DaemonICETransport* transportObj;
        String clientGUID;
        ICEPacketStream* pktStream;
    };

    std::vector<AllocateICESessionThread*> allocateICESessionThreads;  /**< List of outstanding AllocateICESession requests */
    Mutex allocateICESessionThreadsLock; /**< Lock that protects allocateICESessionThreads */

    /**
     * @internal
     * @brief Manage the list of endpoints for the transport.
     */
    void ManageEndpoints(qcc::Timespec tTimeout);

    /**
     * @internal
     * @brief Thread entry point.
     *
     * @param arg  Unused thread entry arg.
     */
    ThreadReturn STDCALL Run(void* arg);

    /**
     * @internal
     * @brief Authentication complete notificiation.
     *
     * @param conn Pointer to the DaemonICEEndpoint that completed authentication.
     */
    void Authenticated(DaemonICEEndpoint* conn);

    /**
     * @internal
     * @brief Normalize a ICE specification.
     *
     * Given a ICE specification (which is the same as a transport
     * specification but with relaxed semantics allowing defaults), convert
     * it into a form which is guaranteed to have a one-to-one relationship
     * with a bus instance.
     *
     * @param inSpec    Input transport connect spec.
     * @param argMap    Parsed parameter map.
     *
     * @return ER_OK if successful.
     */
    QStatus NormalizeListenSpec(const char* inSpec, String& outSpec, std::map<String, String>& argMap) const;

    /**
     * @internal
     * @brief Populate IncomingICESessions with the details of a incoming request for
     * ICE Session Allocation.
     *
     * @param guid              GUID of the remote daemon that is the initiator of this session request
     */
    void RecordIncomingICESessions(String guid);

    /**
     * @internal
     * @brief Purge IncomingICESessions and OutICESessions of the entries related to the
     * remote daemon with GUID=peerID and/or service names in the nameList
     *
     * @param guid              GUID of the remote daemon that is no longer advertising
     * @param nameList          Vector of service names that are being revoked by the remote daemon
     */
    void PurgeSessionsMap(String peerID,  const vector<String>* nameList);

    /**
     * STUN keep-alive and TURN refresh helper.
     *
     * @param icePacketStream   ICEPacketStream
     */
    void SendSTUNKeepAliveAndTURNRefreshRequest(ICEPacketStream& icePacketStream);

    ICEPacketStream* AcquireICEPacketStream(const qcc::String& connectSpec);

    QStatus AcquireICEPacketStreamByPointer(ICEPacketStream* icePacketStream);

    void ReleaseICEPacketStream(const ICEPacketStream& icePktStream);


    class ICECallback {
      public:
        ICECallback(TransportListener*& listener, DaemonICETransport* DaemonICETransport) : m_listener(listener), m_daemonICETransport(DaemonICETransport) { }
        void ICE(ajn::DiscoveryManager::CallbackType cbType, const String& guid, const vector<String>* nameList, uint8_t ttl = 0xFF);
      private:
        TransportListener*& m_listener;
        DaemonICETransport* m_daemonICETransport;
    };

    /**< Called by DiscoveryManager when new services are discovered */
    ICECallback m_iceCallback;

    /**
     * @brief The default timeout for in-process authentications.
     *
     * The authentication process can be used as the basis of a denial of
     * service attack by simply stopping in mid-authentication.  If an
     * authentication takes longer than this number of milliseconds, it may be
     * summarily aborted if another connection comes in.  This value can be
     * overridden in the config file by setting "auth_timeout".  The 30 second
     * number comes from the smaller of two common DBus auth_timeout settings:
     * 30 sec or 240 sec.
     */
    static const uint32_t ALLJOYN_AUTH_TIMEOUT_DEFAULT = 30000;

    /**
     * @brief The default value for the maximum number of authenticating
     * connections.
     *
     * This corresponds to the configuration item "max_incomplete_connections"
     * in the DBus configuration, but it applies only to the ICE transport.  To
     * override this value, change the limit, "max_incomplete_connections_ice".
     * Typically, DBus sets this value to 10,000 which is essentially infinite
     * from the perspective of a phone.  Since this represents a transient state
     * in connection establishment, there should be few connections in this
     * state, so we default to a quite low number.
     */
    static const uint32_t ALLJOYN_MAX_INCOMPLETE_CONNECTIONS_ICE_DEFAULT = 10;

    /**
     * @brief The default value for the maximum number of ICE connections
     * (remote endpoints).
     *
     * This corresponds to the configuration item "max_completed_connections"
     * in the DBus configuration, but it applies only to the ICE transport.
     * To override this value, change the limit, "max_completed_connections_ice".
     * Typically, DBus sets this value to 100,000 which is essentially infinite
     * from the perspective of a phone.  Since we expect bus topologies to be
     * relatively small, we default to a quite low number.
     *
     * @warning This maximum is enforced on incoming connections only.  An
     * AllJoyn daemon is free to form as many outbound connections as it pleases
     * but if the total number of connections exceeds this value, no inbound
     * connections will be accepted.  This is because we are defending against
     * attacks from "abroad" and trust ourselves implicitly.
     */
    static const uint32_t ALLJOYN_MAX_COMPLETED_CONNECTIONS_ICE_DEFAULT = 50;

    /**
     * @brief The scheduling interval for the DaemonICETransport::Run thread.
     */
    // TODO: PPN - May need to tweak this value
    static const uint32_t DAEMON_ICE_TRANSPORT_RUN_SCHEDULING_INTERVAL = 5000;

    /* Timer used to handle the alarms */
    Timer daemonICETransportTimer;

    qcc::Mutex pktStreamMapLock;
    std::map<qcc::String, std::pair<ICEPacketStream, int32_t> > pktStreamMap;
};

} // namespace ajn

#endif // _ALLJOYN_DAEMONICETRANSPORT_H
