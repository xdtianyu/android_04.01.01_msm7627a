/**
 * @file DiscoveryManager.h
 *
 * DiscoveryManager is responsible for all the interactions with the Rendezvous server.
 *
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#ifndef _DISCOVERYMANAGER_H
#define _DISCOVERYMANAGER_H

#ifndef __cplusplus
#error Only include DiscoveryManager.h in C++ code.
#endif

#include <vector>
#include <list>

#include <qcc/String.h>
#include <qcc/Thread.h>
#include <qcc/Mutex.h>
#include <qcc/Socket.h>
#include <qcc/Crypto.h>
#include <qcc/StringSource.h>
#include <qcc/StringSink.h>
#include <qcc/IfConfig.h>
#include <qcc/Timer.h>
#include <alljoyn/BusAttachment.h>

#include "Status.h"
#include "Callback.h"
#include "HttpConnection.h"
#include "RendezvousServerInterface.h"
#include "RendezvousServerConnection.h"
#include "NetworkInterface.h"
#include "SCRAM_SHA1.h"
#include "PeerCandidateListener.h"

using namespace qcc;

#if defined(QCC_OS_ANDROID)
#define ENABLE_PROXIMITY_FRAMEWORK
#endif

namespace ajn {

class ProximityScanEngine;

/**
 * @brief API to provide ICE Discovery for AllJoyn.
 *
 * The basic goal of this class is to provide a way for AllJoyn Services to
 * advertise themselves and for AllJoyn Clients to discover services by
 * communicating with the Rendezvous Server.
 */
class DiscoveryManager : public Thread, public AlarmListener {
  public:
    //friend class ProximityScanEngine;
    BusAttachment& bus;

    /**
     * @internal
     *
     * @brief Construct a Discovery Manager object.
     */
    DiscoveryManager(BusAttachment& bus);

    /**
     * @internal
     *
     * @brief Destroy a Discovery Manager object.
     */
    virtual ~DiscoveryManager();

    /**
     * @internal
     *
     * @brief Disconnect the existing connection from the Rendezvous Server.
     */
    void Disconnect(void);

    /**
     * @brief Initialize the Discovery Manager.
     *
     * Some operations relating to initializing the Discovery Manager and
     * arranging the communication with the Rendezvous Server can fail.
     * These operations are broken out into an Init method so we can
     * return an error condition.  You may be able to try and Init()
     * at a later time if an error is returned.
     *
     * @param guid The daemon guid of the daemon using this service.
     *
     * @return Status of the operation.  Returns ER_OK on success.
     *
     */
    QStatus Init(const String& guid);

    /**
     * @brief Tell the Discovery Manager to connect to the Rendezvous Server
     * on the provided network interface.
     */
    QStatus OpenInterface(const String& name);

    /**
     * @internal
     * @brief An internal enum used to specify the type of callback.
     */
    enum CallbackType {
        /* Found call back */
        FOUND = 0x01,

        /* Allocate ICE session callback */
        ALLOCATE_ICE_SESSION = 0x02
    };

    /**
     * @brief Set the Callback for notification of events.
     */
    void SetCallback(Callback<void, CallbackType, const String&, const vector<String>*, uint8_t>* iceCb);

    /**
     * @brief Advertise an AllJoyn daemon service.
     *
     * If an AllJoyn daemon wants to advertise the presence of a well-known name
     * on the Rendezvous Server, it calls this function.
     *
     * This method allows the caller to specify a single well-known interface
     * name supported by the exporting AllJoyn.
     *
     * @param[in] name      The AllJoyn interface name to Advertise(e.g., "org.freedesktop.Sensor").
     *
     * @return Status of the operation.  Returns ER_OK on success.
     */
    QStatus AdvertiseName(const String& name);

    /**
     * @brief Search an AllJoyn daemon service.
     *
     * If an AllJoyn daemon wants to search the presence of a well-known name
     * on the Rendezvous Server, it calls this function.
     *
     * This method allows the caller to specify a single well-known interface
     * name supported by the exporting AllJoyn.
     *
     * @param[in] name      The AllJoyn interface name to Search(e.g., "org.freedesktop.Sensor").
     *
     * @return Status of the operation.  Returns ER_OK on success.
     */
    QStatus SearchName(const String& name);

    /**
     * @brief Cancel an AllJoyn daemon service advertisement.
     *
     * If an AllJoyn daemon wants to cancel the advertisement of a well-known name
     * on the Rendezvous Server it calls this function.
     *
     * @param[in] name The AllJoyn interface name to stop Advertising(e.g., "org.freedesktop.Sensor").
     *
     * @return Status of the operation.  Returns ER_OK on success.
     */
    QStatus CancelAdvertiseName(const String& name);

    /**
     * @brief Cancel an AllJoyn daemon service search.
     *
     * If an AllJoyn daemon wants to cancel the search of a well-known name
     * on the Rendezvous Server it calls this function.
     *
     * @param[in] name The AllJoyn interface name to stop searching(e.g., "org.freedesktop.Sensor").
     *
     * @return Status of the operation.  Returns ER_OK on success.
     */
    QStatus CancelSearchName(const String& name);

    /**
     * @brief Return the STUN server information.
     *
     * This function returns the the STUN server information corresponding to the particular service/client name
     * on the remote daemon that we are intending to connect to
     *
     * @param[in] client          true indicates that this function is being called on behalf of an AllJoyn client
     *
     * @param[in] remotePeerID    Peer address of the remote daemon on which the service/client of interest is running
     *
     * @param[out] stunInfo       STUN server information
     *
     * @return Status of the operation.  Returns ER_OK on success.
     */
    QStatus GetSTUNInfo(bool client, String remotePeerID, STUNServerInfo& stunInfo);

    /*
     * Internal structure used to store information related to the initiator and receiver of an ICE session.
     */
    struct SessionEntry {

        /* ICE Session user name */
        String ice_frag;

        /* ICE Session password */
        String ice_pwd;

        /* Address candidates of the service */
        list<ICECandidates> serviceCandidates;

        /* Address candidates of the client */
        list<ICECandidates> clientCandidates;

        /* If set to true, valid STUN server information is added by the Rendezvous Server
         * before passing on the message to the other peer*/
        bool AddSTUNInfo;

        /*
         * If true, it indicates that valid STUN Server information is present in STUNInfo
         */
        bool STUNInfoPresent;

        /* STUN server information */
        STUNServerInfo STUNInfo;

        /* Listener to call back on availability of peer candidates */
        PeerCandidateListener* peerListener;

        SessionEntry() : AddSTUNInfo(false), STUNInfoPresent(false) { }

        SessionEntry(bool client, list<ICECandidates> iceCandidates,
                     String frag, String pwd) {
            ice_frag = frag;
            ice_pwd = pwd;
            AddSTUNInfo = false;
            STUNInfoPresent = false;
            if (client) {
                clientCandidates = iceCandidates;
            } else {
                serviceCandidates = iceCandidates;
            }
        }

        void SetClientInfo(list<ICECandidates> iceCandidates,
                           String frag, String pwd, PeerCandidateListener* listener, bool addStun = true) {
            ice_frag = frag;
            ice_pwd = pwd;
            clientCandidates = iceCandidates;
            peerListener = listener;
            AddSTUNInfo = addStun;
        }

        void SetServiceInfo(list<ICECandidates> iceCandidates,
                            String frag, String pwd, PeerCandidateListener* listener) {
            ice_frag = frag;
            ice_pwd = pwd;
            serviceCandidates = iceCandidates;
            peerListener = listener;
        }

        void SetSTUNInfo(STUNServerInfo stunInfo) {
            STUNInfoPresent = true;
            STUNInfo = stunInfo;
        }
    };

    /**
     * @internal
     * @brief Enum describing the type of Discovery Manager message.
     */
    enum MessageType {
        INVALID_MESSAGE = 0,              /*Invalid message type*/
        ADVERTISEMENT,                    /*Advertisement Message*/
        SEARCH,                           /*Search Message*/
        ADDRESS_CANDIDATES,               /*Address candidates Message*/
        PROXIMITY,                        /*Proximity Message*/
        RENDEZVOUS_SESSION_DELETE,        /*Rendezvous Session Delete Message*/
        GET_MESSAGE,                      /*GET Message*/
        CLIENT_LOGIN,                     /*Client Login Message*/
        DAEMON_REGISTRATION,              /*Daemon Registration Message*/
        TOKEN_REFRESH                     /*Token refresh Message*/
    };

    /**
     * @internal
     * @brief Class describing the fields of a Discovery Manager message.
     */
    class RendezvousMessage {
      public:

        /*HTTP Method to be used to send this message to the Rendezvous Server*/
        HttpConnection::Method httpMethod;

        /*RendezvousMessage Type*/
        MessageType messageType;

        InterfaceMessage* interfaceMessage;

        /*Constructor*/
        RendezvousMessage() {
            messageType = INVALID_MESSAGE;
            httpMethod = HttpConnection::METHOD_INVALID;
            interfaceMessage = NULL;
        }

        /*Destructor*/
        ~RendezvousMessage() {
            messageType = INVALID_MESSAGE;
            httpMethod = HttpConnection::METHOD_INVALID;
        }

        void Clear() {
            messageType = INVALID_MESSAGE;
            httpMethod = HttpConnection::METHOD_INVALID;

            if (interfaceMessage) {
                delete interfaceMessage;
                interfaceMessage = NULL;
            }
        }
    };

    /**
     * @brief Compose an Advertisement or Search message to be sent to the Rendezvous Server.
     *
     * @param[in] advertisement   If true, Advertisement message is sent or else Search message is sent.
     *
     * @param[in] httpMethod      HTTP Method to use to send the message to the Rendezvous Server.
     *
     * @param[in] message         Composed message
     *
     * @return None.
     */
    void ComposeAdvertisementorSearch(bool advertisement, HttpConnection::Method httpMethod, RendezvousMessage& message);

    /**
     * @brief Queue an ICE Address Candidate message for transmission out to the Rendezvous Server.
     *
     * If an AllJoyn daemon wants send ICE Address Candidates to the Rendezvous Server, it calls this function.
     *
     * @param[in] client          Set to true if this function is called by a client, Set to false if this function is called by a service
     *
     * @param[in] sessionDetail   Pair of the remote daemon GUID to the session request receiver details
     *
     * @return Status of the operation.  Returns ER_OK on success.
     */
    QStatus QueueICEAddressCandidatesMessage(bool client, std::pair<String, SessionEntry> sessionDetail);

    /**
     * @brief Remove a timeout entry from the session maps.
     *
     *
     * @param[in] client          Set to true if this function is called by a client, Set to false if this function is called by a service
     *
     * @param[in] sessionDetail   Pair of a session request initiator name to the session request receiver details
     *
     * @return Status of the operation.  Returns ER_OK on success.
     */
    void RemoveSessionDetailFromMap(bool client, std::pair<String, SessionEntry> sessionDetail);

    /**
     * @brief Queue a Proximity message for transmission out to the Rendezvous Server.
     *
     * If an AllJoyn daemon wants send Proximity message to the Rendezvous Server, it calls this function.
     *
     * @param[in] proximity  Proximity message to be sent to the Rendezvous Server
     *
     * @param[in] bssids     List of latest Wi-Fi BSSIDs
     *
     * @param[in] bssids     Bluetooth MAC IDs
     *
     * @return Status of the operation.  Returns ER_OK on success.
     *
     * NOTE: The function calling this method should ensure to lock DiscoveryManagerMutex
     */
    QStatus QueueProximityMessage(ProximityMessage proximity, list<String> bssids, list<String> btMacIds);

    /**
     * @brief Compose a Proximity message for transmission out to the Rendezvous Server.
     *
     * If an AllJoyn daemon wants send Proximity message to the Rendezvous Server, it calls this function.
     *
     * @param[in] proximity   Proximity message to be sent to the Rendezvous Server
     *
     * @param[in] httpMethod  HTTP method to be used to send the Proximity message
     *
     * @param[in] message     Composed message
     *
     * NOTE: The function calling this method should ensure to lock DiscoveryManagerMutex
     */
    void ComposeProximityMessage(HttpConnection::Method httpMethod, RendezvousMessage& message);

#ifndef ENABLE_PROXIMITY_FRAMEWORK
    // Adding sample messages for proximity. This will be removed once the framework
    // to derive proximity information from the kernel is available.
    ProximityMessage proximity[3];

    uint8_t currentProximityIndex;

    void InitializeProximity()
    {
        WiFiProximity wifi;
        BTProximity bt;

        wifi.attached = true;
        bt.self = true;

        wifi.BSSID = String("a1");
        wifi.SSID = String("a1");
        bt.MAC = String("a1");

        proximity[0].wifiaps.push_back(wifi);
        proximity[0].BTs.push_back(bt);

        // Using this function to print the hard-coded proximity message
        GenerateJSONProximity(proximity[0]);

        wifi.BSSID = String("a1");
        wifi.SSID = String("a1");
        bt.MAC = String("a1");

        proximity[1].wifiaps.push_back(wifi);
        proximity[1].BTs.push_back(bt);

        // Using this function to print the hard-coded proximity message
        GenerateJSONProximity(proximity[1]);

        wifi.BSSID = String("a1");
        wifi.SSID = String("a1");
        bt.MAC = String("a1");

        proximity[2].wifiaps.push_back(wifi);
        proximity[2].BTs.push_back(bt);

        // Using this function to print the hard-coded proximity message
        GenerateJSONProximity(proximity[2]);

    }
#endif

    /**
     * @internal
     * @brief Utility function to print the string equivalent of enum MessageType.
     */
    String PrintMessageType(MessageType type);

    /**
     * @internal
     * @brief Send updated information to the Rendezvous Server based on the rdvzSessionActive
     * flag
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    QStatus UpdateInformationOnServer(MessageType messageType, bool rdvzSessionActive);

    /**
     * @internal
     * @brief Handle a response received over the On Demand connection.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    QStatus HandleOnDemandMessageResponse(Json::Value payload);

    /**
     * @internal
     * @brief Handle the Http status code and response received over the On Demand connection.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    void HandleOnDemandConnectionResponse(HttpConnection::HTTPResponse& response);

    /**
     * @internal
     * @brief Generate the client login first request and send it
     * to the server.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    QStatus SendClientLoginFirstRequest(void);

    /**
     * @internal
     * @brief Generate the client login final request and queue it for transmission
     * to the server.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    QStatus SendClientLoginFinalRequest(void);

    /**
     * @internal
     * @brief Handle received client login error.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    void HandleUnsuccessfulClientAuthentication(SASLError error);

    /**
     * @internal
     * @brief Handle a successful client login response.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    void HandleSuccessfulClientAuthentication(ClientLoginFinalResponse response);

    /**
     * @internal
     * @brief Handle the sending of updates to the Server
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    QStatus HandleUpdatesToServer(void);

    /**
     * @internal
     * @brief Handle a client login response received over the On Demand connection.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    QStatus HandleClientLoginResponse(Json::Value payload);

    /**
     * @internal
     * @brief Handle a token response received over the On Demand connection.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    QStatus HandleTokenRefreshResponse(Json::Value payload);

    /**
     * Main thread entry point.
     *
     * @param arg  Unused thread entry arg.
     */
    ThreadReturn STDCALL Run(void* arg);

    /**
     * @internal
     * @brief Queue a message for transmission out to the Rendezvous Server.
     *
     * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
     */
    void QueueMessage(RendezvousMessage message);

    /**
     * @internal
     * @brief Purge the OutboundMessageQueue to remove messages of the specified message type.
     */
    void PurgeOutboundMessageQueue(MessageType messageType);

    /**
     * @internal
     * @brief Send a message to the Rendezvous Server.
     * @return  ER_OK if successful.
     */
    QStatus SendMessage(RendezvousMessage message);

    /**
     * @internal
     * @brief Handle the Search Match Response message.
     */
    QStatus HandleSearchMatchResponse(SearchMatchResponse response);

    /**
     * @internal
     * @brief Handle the Start ICE Checks Response message.
     */
    QStatus HandleStartICEChecksResponse(StartICEChecksResponse response);

    /**
     * @internal
     * @brief Handle the Match Revoked Response message.
     */
    QStatus HandleMatchRevokedResponse(MatchRevokedResponse response);

    /**
     * @internal
     * @brief Handle the Address Candidates Response message.
     */
    QStatus HandleAddressCandidatesResponse(AddressCandidatesResponse response);

    /**
     * @internal
     * @brief Handle the response received over the Persistent connection.
     */
    QStatus HandlePersistentMessageResponse(Json::Value payload);

    /**
     * @internal
     * @brief Handle the http status code and response received over the Persistent connection.
     */
    void HandlePersistentConnectionResponse(HttpConnection::HTTPResponse& response);

    /**
     * Internal helper method used to return the URI and content for a message
     * to be sent to the Rendezvous Server.
     *
     * @param message  Message to be sent over http.
     *
     * @return ER_OK if successful.
     */
    QStatus PrepareOutgoingMessage(RendezvousMessage messageType, HttpConnection::Method& httpMethod, String& uri, bool& contentPresent, String& content);

    /**
     * @internal
     * @brief Set the T_KEEP_ALIVE_IN_MS value
     */
    void SetTKeepAlive(uint32_t tsecs);

    /**
     * @internal
     * @brief Returns the value of T_KEEP_ALIVE_IN_MS
     */
    uint32_t GetTKeepAlive(void) { return T_KEEP_ALIVE_IN_MS; };

    /**
     * @internal
     * @brief Send a Daemon Registration Message to the Server
     */
    QStatus SendDaemonRegistrationMessage(void);

    /**
     * @internal
     * @brief Update the interfaces and connect to the Rendezvous Server
     */
    QStatus Connect(void);

    /**
     * @internal
     * @brief Get the Wait timeout
     */
    uint32_t GetWaitTimeOut(void);

    /**
     * @internal
     * @brief Alarm handler
     */
    void AlarmTriggered(const qcc::Alarm& alarm, QStatus status);

    class UserCredentials {

      public:

        /* User Name */
        String userName;

        /* Password */
        String userPassword;

        UserCredentials() : userName(String("")), userPassword(String(" ")) { }

        void SetCredentials(String user, String password) {
            userName = user;
            userPassword = password;
        };
    };

    /**
     * @internal
     * @brief Retrieve the user credentials from the Client Login Bus Interface
     */
    void GetUserCredentials(void);

    /**
     * @internal
     * @brief Compose and queue a token refresh message
     */
    void ComposeAndQueueTokenRefreshMessage(TokenRefreshMessage refreshMessage);

    /**
     * @internal
     * @brief Set the disconnect event
     */
    void SetDisconnectEvent(void) { DisconnectEvent.SetEvent(); };

    /**
     * @internal
     * @brief Return the PeerAddr
     */
    qcc::String GetPeerAddr(void) { return PeerAddr; };

    /**
     * @internal
     * @brief Return the value of EnableIPv6
     */
    bool GetEnableIPv6(void) { return EnableIPv6; };

    /**
     * Stop the Discovery Manager.
     *
     * @return ER_OK if successful.
     */
    QStatus Stop();

    /**
     * Pend the caller until the Discovery Manager stops.
     * @return ER_OK if successful.
     */
    QStatus Join();

    /**
     * @internal
     * @brief Return IPAddresses of the interfaces
     * over which the Persistent and the On Demand connections have
     * been setup with the Rendezvous Server.
     */
    void GetRendezvousConnIPAddresses(IPAddress& onDemandAddress, IPAddress& persistentAddress);

  private:

    /**
     * @internal
     *
     * @brief Number of milliseconds in a second
     */
    static const uint32_t MS_IN_A_SECOND = 1000;

    /**
     * @brief The property value used to specify the wildcard interface name.
     */
    static const char* INTERFACES_WILDCARD;

    /**
     * The modulus indicating the minimum time between interface updates.
     * Units are milli seconds.
     */
    /*PPN - Review duration*/
    static const uint32_t INTERFACE_UPDATE_MIN_INTERVAL = 180000;

    /**
     * @internal
     *
     * @brief Time interval to wait for receiving a response for a GET message from the Rendezvous Server.
     */
    uint32_t T_KEEP_ALIVE_IN_MS;

    /**
     * @internal
     *
     * @brief Minimum value of TKeepAlive in seconds
     */
    static const uint32_t T_KEEP_ALIVE_MIN_IN_SECS = 30;

    /**
     * @internal
     *
     * @brief Value to be multiplied with the server sent value of TkeepAlive to arrive
     * at T_KEEP_ALIVE_IN_MS
     */
    static const uint32_t T_KEEP_ALIVE_BUFFER_MULTIPLE = 2;

    /**
     * @internal
     *
     * @brief Client Login Service Name
     */
    String ClientLoginServiceName;

    /**
     * @internal
     *
     * @brief Client Login Service Object
     */
    String ClientLoginServiceObject;

    /**
     * @internal
     *
     * @brief Methof to get the account name from the Client Login Service
     */
    String GetAccountNameMethod;

    /**
     * @internal
     *
     * @brief Methof to get the account password from the Client Login Service
     */
    String GetAccountPasswordMethod;

    /**
     * @internal
     *
     * @brief Peer ID assigned for this daemon.
     */
    String PeerID;

    /**
     * @internal
     *
     * @brief Peer Address assigned to this daemon.
     */
    String PeerAddr;

    /**
     * @internal
     *
     * @brief Last message sent on the On Demand connection.
     */
    RendezvousMessage LastOnDemandMessageSent;

    /**
     * @internal
     *
     * @brief Rendezvous server address.
     */
    String RendezvousServer;

    /* Map of the all the sessions that have been initiated by the AllJoyn client attached to this daemon*/
    multimap<String, SessionEntry> OutgoingICESessions;

    /* Map of the all the sessions that have been initiated by the AllJoyn client running on a remote daemon and which
     * are intended for AllJoyn services running on this daemon. The sessions corresponding to the entries in this map
     * are ready to go into the ICE checks phase */
    multimap<String, SessionEntry> IncomingICESessions;

    /**
     * @brief
     * Private notion of what state the implementation object is in.
     */
    enum State {
        IMPL_INVALID,           /**< Should never be seen on a constructed object */
        IMPL_SHUTDOWN,          /**< Nothing is running and object may be destroyed */
        IMPL_INITIALIZING,      /**< Object is in the process of coming up and may be inconsistent */
        IMPL_RUNNING,           /**< Object is running and ready to go */
    };

    /**
     * @internal
     * @brief State variable to indicate what the implementation is doing or is
     * capable of doing.
     */
    State DiscoveryManagerState;

    /**
     * @internal
     * @brief Mutex object used to protect various lists that may be accessed
     * by multiple threads.
     */
    Mutex DiscoveryManagerMutex;

    /**
     * @internal
     * @brief The GUID string of the daemon associated with this instance
     * of the Discovery Manager.
     */
    String PersistentIdentifier;

    /**
     * @internal
     * @brief Parameter used to specify the interface type to be used
     * to connect to the Rendezvous Server.
     */
    uint8_t InterfaceFlags;

    /**
     * @internal
     * @brief HTTP connection setup with the Rendezvous Server.
     */
    RendezvousServerConnection* Connection;

    /**
     * @internal
     * @brief This boolean indicates whether AllJoyn Daemon has been authenticated by the
     * Rendezvous Server.
     */
    bool ConnectionAuthenticationComplete;

    /**
     * @internal
     * @brief ICE Callback
     */
    Callback<void, CallbackType, const String&, const vector<String>*, uint8_t>* iceCallback;

    class RemoteDaemonServicesInfo {
      public:
        /* GUID of the remote daemon */
        String remoteGUID;

        /* Vector of services running on the remote daemon that have been discovered by us */
        vector<String> services;
    };

    class RemoteDaemonStunInfo {
      public:
        /* STUN Info to be used for ICE connectivity with the Daemon running the Service */
        STUNServerInfo stunInfo;

        /* list of services running on the remote daemon that have been discovered by us */
        list<String> services;
    };

    /**
     * @internal
     * @brief A class used to store a list of the received responses for a FindName.
     */
    class SearchResponseInfo {
      public:
        list<RemoteDaemonServicesInfo> response; /**< list of the GUID of the Daemon from which the information was received and the
                                                     vector of services discovered */

        //Constructor
        SearchResponseInfo() { }

        //Destructor
        ~SearchResponseInfo() {
            response.clear();
        }
    };

    /**
     * @internal
     * @brief A sorted list of all of the current well known names that we are advertising. This is derived from advertiseMap. Though this is
     * extra memory consumed for already existing information this is required to reduce the turn around time required to respond to the
     * responses from the Rendezvous Server.
     */
    list<String> currentAdvertiseList;

    /**
     * @internal
     * @brief A sorted list of all of the advertising well known names that we are intending to send to the Rendezvous Server. This is required because we will know that
     * a list of advertisements has made it to the Server only after we receive a 200 OK response for it. Until then, we would have to hold the list and not populate it in
     * lastSentAdvertiseList
     */
    list<String> tempSentAdvertiseList;

    /**
     * @internal
     * @brief A sorted list of all of the advertising well known names that were last sent to the Rendezvous Server. This is copy of currentAdvertiseList taken when
     * an Advertisement RendezvousMessage is sent. Though this is extra memory consumed for already existing information this is required to reduce the turn around
     * time required to respond to the responses from the Rendezvous Server.
     */
    list<String> lastSentAdvertiseList;

    /**
     * @internal
     * @brief A map of all of the names that a user has requested to be found along with the response received for each.
     */
    std::map<String, SearchResponseInfo> searchMap;

    /**
     * @internal
     * @brief A map of all the remote GUIDs and the associated STUN and TURN server information.
     */
    std::map<String, RemoteDaemonStunInfo> StunAndTurnServerInfo;

    /**
     * @internal
     * @brief A sorted list of all of the current well known names that we are discovering. This is derived from findMap. Though this is
     * extra memory consumed for already existing information this is required to reduce the turn around time required to respond to the
     * responses from the Rendezvous Server.
     */
    list<String> currentSearchList;

    /**
     * @internal
     * @brief A sorted list of all of the discovering well known names that we are intending to send to the Rendezvous Server. This is required because we will know that
     * a list of searches has made it to the Server only after we receive a 200 OK response for it. Until then, we would have to hold the list and not populate it in
     * lastSentAdvertiseList
     */
    list<String> tempSentSearchList;

    /**
     * @internal
     * @brief A sorted list of all of the discovering well known names that were last sent to the Rendezvous Server. This is copy of currentSearchList taken when
     * an Search RendezvousMessage is sent. Though this is extra memory consumed for already existing information this is required to reduce the turn around
     * time required to respond to the responses from the Rendezvous Server.
     */
    list<String> lastSentSearchList;

    /**
     * @internal
     * @brief A sorted list of all of the current BSSIDs that we have identified
     */
    list<String> currentBSSIDList;

    /**
     * @internal
     * @brief A sorted list of all the BSSIDs that we are intending to send to the Rendezvous Server
     */
    list<String> tempSentBSSIDList;

    /**
     * @internal
     * @brief A sorted list of all BSSIDs that were last sent to the Rendezvous Server
     */
    list<String> lastSentBSSIDList;

    /**
     * @internal
     * @brief A sorted list of all of the current BT MACs that we have identified
     */
    list<String> currentBTMACList;

    /**
     * @internal
     * @brief A sorted list of all the BT MACs that we are intending to send to the Rendezvous Server
     */
    list<String> tempSentBTMACList;

    /**
     * @internal
     * @brief A sorted list of all BT MACs that were last sent to the Rendezvous Server
     */
    list<String> lastSentBTMACList;

    /**
     * @internal
     * @brief Event used to wake up the main Discovery Manager thread
     */
    Event WakeEvent;

    /**
     * @internal
     * @brief Event used to wake up the main Discovery Manager thread and tell it
     * that a response has been received on the On Demand connection or that we
     * have timed out waiting for the response.
     */
    Event* OnDemandResponseEvent;

    /**
     * @internal
     * @brief Event used to wake up the main Discovery Manager thread and tell it
     * that a response has been received on the Persistent connection or that we
     * have timed out waiting for the response.
     */
    Event* PersistentResponseEvent;

    /**
     * @internal
     * @brief Event used to wake up the main Discovery Manager thread and tell it
     * that tear down the existing connection with the Rendezvous Server
     * and set up another connection.
     */
    Event ConnectionResetEvent;

    /**
     * @internal
     * @brief Event used to wake up the main Discovery Manager thread and tell it
     * that tear down the existing connection with the Rendezvous Server.
     */
    Event DisconnectEvent;

    /**
     * @internal
     * @brief Set to true to force a interface update cycle
     */
    bool ForceInterfaceUpdateFlag;

    /**
     * @internal
     * @brief Set to true to force a client login procedure
     */
    bool ClientAuthenticationRequiredFlag;

    /**
     * @internal
     * @brief Set to true to re-send all the latest Advertisements, Searches and
     * Proximity to the Server
     */
    bool UpdateInformationOnServerFlag;

    /**
     * @internal
     * @brief State of the SessionActive field sent by the Server as a part of the
     * Client Login procedure
     */
    bool RendezvousSessionActiveFlag;

    /**
     * @internal
     * @brief Set to true to send the Daemon Registration message to the Server
     */
    bool RegisterDaemonWithServer;

    /**
     * @internal
     * @brief Time stamp captured when a message is sent to the Server over the
     * Persistent connection
     */
    uint32_t PersistentMessageSentTimeStamp;

    /**
     * @internal
     * @brief Time stamp captured when a message is sent to the Server over the
     * On Demand connection
     */
    uint32_t OnDemandMessageSentTimeStamp;

    /**
     * @internal
     * @brief Boolean used to ensure that a response for a message sent over the On Demand
     * connection is received before sending the next message
     */
    bool SentMessageOverOnDemandConnection;

    /**
     * @internal
     * @brief Indicates the last message type (Advertisement/Search/Proximity) that was
     * sent to the Server to update the information on the Server
     */
    MessageType LastSentUpdateMessage;

    /**
     * @internal
     * @brief A list of messages queued for transmission out to the
     * Rendezvous Server.
     */
    list<RendezvousMessage> OutboundMessageQueue;

    /* GET Message */
    RendezvousMessage GETMessage;

    /* Rendezvous Session Delete Message */
    RendezvousMessage RendezvousSessionDeleteMessage;

    /* The SCRAM-SHA-1 authentication module */
    SCRAM_SHA_1 SCRAMAuthModule;

    /* Proximity Scanner instance */
    ProximityScanEngine* ProximityScanner;

    /* Boolean indicating if client authentication failed due to either the DEACTIVATED_USER or
     * UNKNOWN_USER error. If this is set we do not attempt a reconnect unless the Advertise/Search
     * list has changed */
    bool ClientAuthenticationFailed;

    /* Timer used to handle the alarms */
    Timer DiscoveryManagerTimer;

    /* Interface update alarm */
    Alarm* InterfaceUpdateAlarm;

    /* Flag indicating if a GET message was sent after the Persistent connection was set up */
    bool SentFirstGETMessage;

    /* User Credentials */
    UserCredentials userCredentials;

    /* Boolean indicating if HTTP protocol needs to be used for connection with the Server */
    bool UseHTTP;

    /* Boolean indicating if IPv6 interface should be used for connections with the Server and also
     * as probable ICE candidates */
    bool EnableIPv6;
};

} // namespace ajn

#endif //_DISCOVERYMANAGER_H
