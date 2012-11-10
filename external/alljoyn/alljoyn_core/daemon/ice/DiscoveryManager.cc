/**
 * @file DiscoveryManager.cc
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
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <algorithm>

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/Event.h>
#include <qcc/Stream.h>
#include <qcc/Socket.h>
#include <qcc/StringSource.h>
#include <qcc/StringSink.h>
#include <alljoyn/version.h>

#include <qcc/Util.h>
#include <alljoyn/BusAttachment.h>


#include "DiscoveryManager.h"
#include "RendezvousServerInterface.h"
#include "DaemonConfig.h"
#include "ProximityScanEngine.h"
#include "RendezvousServerConnection.h"

using namespace std;
using namespace qcc;

#define QCC_MODULE "ICE_DISCOVERY_MANAGER"

namespace ajn {

//
// There are configurable attributes of the Discovery Manager which are determined
// by the configuration database.  A module name is required and is defined
// here.  An example of how to use this is in setting the interfaces the discovery
// manager will use for discovery.
//
//   <busconfig>
//     <ice_discovery_manager>
//       <property interfaces="*"/>
//       <property server="rdvs.alljoyn.org"/>
//       <property protocol="HTTPS"/>
//       <property enable_ipv6=\"false\"/>"
//     </ice_discovery_manager>
//   </busconfig>
//

//
// The value of the interfaces property used to configure the Discovery Manager
// to run discovery over all interfaces in the system.
//
const char* DiscoveryManager::INTERFACES_WILDCARD = "*";

DiscoveryManager::DiscoveryManager(BusAttachment& bus)
    : Thread("DiscoveryManager"),
    bus(bus),
    ClientLoginServiceName(String("org.alljoyn.ice.clientloginservice")),
    ClientLoginServiceObject(String("/ClientLoginService")),
    GetAccountNameMethod(String("GetClientAccountName")),
    GetAccountPasswordMethod(String("GetClientAccountPassword")),
    PeerID(),
    PeerAddr(),
    DiscoveryManagerState(IMPL_SHUTDOWN),
    PersistentIdentifier(),
    InterfaceFlags(NONE),
    Connection(NULL),
    ConnectionAuthenticationComplete(false),
    iceCallback(NULL),
    WakeEvent(),
    OnDemandResponseEvent(NULL),
    PersistentResponseEvent(NULL),
    ConnectionResetEvent(),
    DisconnectEvent(),
    ForceInterfaceUpdateFlag(false),
    ClientAuthenticationRequiredFlag(false),
    UpdateInformationOnServerFlag(false),
    RendezvousSessionActiveFlag(false),
    RegisterDaemonWithServer(false),
    PersistentMessageSentTimeStamp(0),
    OnDemandMessageSentTimeStamp(0),
    SentMessageOverOnDemandConnection(false),
    LastSentUpdateMessage(INVALID_MESSAGE),
    SCRAMAuthModule(),
    ProximityScanner(NULL),
    ClientAuthenticationFailed(false),
    InterfaceUpdateAlarm(NULL),
    SentFirstGETMessage(false),
    userCredentials(),
    UseHTTP(false),
    EnableIPv6(false)
{
    QCC_DbgPrintf(("DiscoveryManager::DiscoveryManager()\n"));

    //
    // There are configurable attributes of the Discovery Manager which are determined
    // by the configuration database.  A module name is required and is defined
    // here.  An example of how to use this is in setting the interfaces the discovery
    // manager will use for discovery.
    //
    //   <busconfig>
    //     <ice_discovery_manager>
    //       <property interfaces="*"/>
    //       <property server="rdvs.alljoyn.org"/>
    //       <property protocol="HTTPS"/>
    //       <property enable_ipv6=\"false\"/>"
    //     </ice_discovery_manager>
    //   </busconfig>
    //
    DaemonConfig* config = DaemonConfig::Access();

    /* Retrieve the Rendezvous Server address from the config file */
    RendezvousServer = config->Get("ice_discovery_manager/property@server", "rdvs.alljoyn.org");

    /* Retrieve the connection protocol to be used */
    if (config->Get("ice_discovery_manager/property@protocol") == "HTTP") {
        QCC_DbgPrintf(("DiscoveryManager::DiscoveryManager(): Using HTTP"));
        UseHTTP = true;
    }

    /* See if IPv6 interfaces are allowed to be used */
    if (config->Get("ice_discovery_manager/property@enable_ipv6") == "true") {
        QCC_DbgPrintf(("DiscoveryManager::DiscoveryManager(): Enabling use of IPv6 interfaces"));
        EnableIPv6 = true;
    }

    QCC_DbgPrintf(("DiscoveryManager::DiscoveryManager(): RendezvousServer = %s\n", RendezvousServer.c_str()));

    /* Compose the GET message and the Rendezvous Session Delete messages */
    GETMessage.httpMethod = HttpConnection::METHOD_GET;
    GETMessage.messageType = GET_MESSAGE;

    RendezvousSessionDeleteMessage.httpMethod = HttpConnection::METHOD_DELETE;
    RendezvousSessionDeleteMessage.messageType = RENDEZVOUS_SESSION_DELETE;

    /* Initialize the keep alive timer value to the default value */
    SetTKeepAlive(T_KEEP_ALIVE_MIN_IN_SECS);

    /* Start the DiscoveryManagerTimer which is used to handle all the alarms */
    DiscoveryManagerTimer.Start();

    /* Clear all our lists */
    currentAdvertiseList.clear();
    tempSentAdvertiseList.clear();
    lastSentAdvertiseList.clear();
    currentSearchList.clear();
    tempSentSearchList.clear();
    lastSentSearchList.clear();
    currentBSSIDList.clear();
    tempSentBSSIDList.clear();
    lastSentBSSIDList.clear();
    currentBTMACList.clear();
    tempSentBTMACList.clear();
    lastSentBTMACList.clear();

    OutboundMessageQueue.clear();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
    /* Initialize the ProximityScanEngine */
    ProximityScanner = new ProximityScanEngine(this);
#else
    currentProximityIndex = 0;
    // This will be removed once the framework
    // to derive proximity information from the kernel is available.
    InitializeProximity();
#endif
}

DiscoveryManager::~DiscoveryManager()
{
    QCC_DbgPrintf(("DiscoveryManager::~DiscoveryManager()\n"));

    /* Delete all the active alarms */
    if (InterfaceUpdateAlarm) {
        DiscoveryManagerTimer.RemoveAlarm(*InterfaceUpdateAlarm);
        delete InterfaceUpdateAlarm;
        InterfaceUpdateAlarm = NULL;
    }

    /* Stop the DiscoveryManagerTimer which is used to handle all the alarms */
    DiscoveryManagerTimer.Stop();

    //
    // Send a delete all message to the Rendezvous Server if we are still
    // connected to the Server
    //
    if (Connection) {
        SendMessage(RendezvousSessionDeleteMessage);
    }

    //
    // Stop the worker thread to get things calmed down.
    //
    if (IsRunning()) {
        Stop();
        Join();
    }

    //
    // We may have an active connection with the Rendezvous Server.
    // We need to tear it down
    //
    Disconnect();

    //
    // We should delete the ProximityScanner object here to avoid a race condition
    // because the Run() thread may still be using it. So we delete the ProximityScanner
    // after the Run() thread has joined
    //
#ifdef ENABLE_PROXIMITY_FRAMEWORK
    if (ProximityScanner) {
        ProximityScanner->StopScan();
        delete ProximityScanner;
        ProximityScanner = NULL;
    }
#endif

    //
    // Delete any callbacks that a user of this class may have set.
    //
    if (iceCallback) {
        delete iceCallback;
        iceCallback = NULL;
    }

    DiscoveryManagerState = IMPL_SHUTDOWN;
}

void DiscoveryManager::Disconnect(void)
{
    QCC_DbgPrintf(("DiscoveryManager::Disconnect()\n"));

    if (Connection) {

        Connection->Disconnect();
        delete Connection;
        Connection = NULL;

        LastOnDemandMessageSent.Clear();
    }
}

QStatus DiscoveryManager::Init(const String& guid)
{
    QCC_DbgPrintf(("DiscoveryManager::Init()\n"));

    //
    // Can only call Init() if the object is not running or in the process
    // of initializing
    //
    if (DiscoveryManagerState != IMPL_SHUTDOWN) {
        return ER_FAIL;
    }

    DiscoveryManagerState = IMPL_INITIALIZING;

    PersistentIdentifier = guid;

    assert(IsRunning() == false);

    Start(this);

    DiscoveryManagerState = IMPL_RUNNING;

    return ER_OK;
}

QStatus DiscoveryManager::OpenInterface(const String& name)
{
    QCC_DbgPrintf(("DiscoveryManager::OpenInterface(%s)\n", name.c_str()));

    //
    // Can only call OpenInterface() if the object is running.
    //
    if (DiscoveryManagerState != IMPL_RUNNING) {
        QCC_DbgPrintf(("DiscoveryManager::OpenInterface(): Not running\n"));
        return ER_FAIL;
    }

    //
    // There are at least two threads that can wander through the vector below
    // so we need to protect access to the list with a convenient DiscoveryManagerMutex.
    //
    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

    //
    // If the user specifies the wildcard interface name, this trumps everything
    // else.
    //
    if (name == INTERFACES_WILDCARD) {
        InterfaceFlags = NetworkInterface::ANY;
        QCC_DbgPrintf(("DiscoveryManager::OpenInterface: Interface Type = INTERFACES_WILDCARD\n"));
    } else {
        InterfaceFlags = NetworkInterface::NONE;
        QCC_DbgPrintf(("DiscoveryManager::OpenInterface: Interface Type = NONE\n"));
    }

    ForceInterfaceUpdateFlag = true;
    QCC_DbgPrintf(("DiscoveryManager::OpenInterface: Set the wake event\n"));
    WakeEvent.SetEvent();

    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

void DiscoveryManager::SetCallback(Callback<void, CallbackType, const String&, const vector<String>*, uint8_t>* iceCb)
{
    QCC_DbgPrintf(("DiscoveryManager::SetCallback()\n"));

    //
    // Set the callback
    //
    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
    if (iceCallback) {
        delete iceCallback;
        iceCallback = NULL;
    }
    iceCallback = iceCb;
    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
}

void DiscoveryManager::ComposeAdvertisementorSearch(bool advertisement, HttpConnection::Method httpMethod, RendezvousMessage& message)
{
    QCC_DbgPrintf(("DiscoveryManager::ComposeAdvertisementorSearch()\n"));

    list<String> tempCurrentList = currentAdvertiseList;
    list<String>* tempSentList = &tempSentAdvertiseList;
    message.messageType = ADVERTISEMENT;

    if (!advertisement) {
        tempCurrentList = currentSearchList;
        tempSentList = &tempSentSearchList;
        message.messageType = SEARCH;
        QCC_DbgPrintf(("DiscoveryManager::ComposeAdvertisementorSearch(): Called for sending a Search message"));
    } else {
        QCC_DbgPrintf(("DiscoveryManager::ComposeAdvertisementorSearch(): Called for sending an Advertisement message"));
    }

    /* Return if the current list is empty as we have nothing to send */
    if (tempCurrentList.empty() && (httpMethod != HttpConnection::METHOD_DELETE)) {
        message.messageType = INVALID_MESSAGE;
        return;
    }

    // Update the corresponding sent list with the latest
    // information that is being sent to the Rendezvous Server
    tempSentList->clear();
    *tempSentList = tempCurrentList;

    if (httpMethod != HttpConnection::METHOD_DELETE) {
        //
        // Compose an Advertisement/Search RendezvousMessage
        //
        if (advertisement) {

            AdvertiseMessage* advertise = new AdvertiseMessage();
            Advertisement adv;

            while (!tempCurrentList.empty()) {
                adv.service = tempCurrentList.front();
                advertise->ads.push_back(adv);
                tempCurrentList.pop_front();
            }

            message.interfaceMessage = static_cast<InterfaceMessage*>(advertise);

        } else {

            SearchMessage* searchMsg = new SearchMessage();

            Search search;
            while (!tempCurrentList.empty()) {
                search.service = tempCurrentList.front();
                searchMsg->search.push_back(search);
                tempCurrentList.pop_front();
            }

            message.interfaceMessage = static_cast<InterfaceMessage*>(searchMsg);

        }
    }

    message.httpMethod = httpMethod;
}

QStatus DiscoveryManager::AdvertiseName(const String& name)
{
    QCC_DbgPrintf(("DiscoveryManager::AdvertiseName()\n"));

    if (DiscoveryManagerState != IMPL_RUNNING) {
        QCC_DbgPrintf(("DiscoveryManager::AdvertiseName(): Not IMPL_RUNNING\n"));
        return ER_FAIL;
    }

    QCC_DbgPrintf(("DiscoveryManager::AdvertiseName(): Called for an Advertising %s", name.c_str()));

    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

    // Check is the name is already being advertised
    list<String>::iterator it;

    if (!currentAdvertiseList.empty()) {
        for (it = currentAdvertiseList.begin(); it != currentAdvertiseList.end();) {

            if (*it == name) {
                // Release the mutex.
                DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

                // As we are already advertising the name, we don't need to do anything
                QCC_DbgPrintf(("DiscoveryManager::AdvertiseName(): Already advertising %s", name.c_str()));
                return ER_OK;
            } else {
                ++it;
            }
        }
    }

    QCC_DbgPrintf(("DiscoveryManager::AdvertiseName(): Adding %s", name.c_str()));

    currentAdvertiseList.push_back(name);
    currentAdvertiseList.sort();

    // If the ClientAuthenticationFailed flag is set, reset it as the Advertise list
    // has changed
    if (ClientAuthenticationFailed) {
        ClientAuthenticationFailed = false;
    }

    HttpConnection::Method httpMethod = HttpConnection::METHOD_POST;

    RendezvousMessage message;
    ComposeAdvertisementorSearch(true, httpMethod, message);

    //
    // Queue this message for transmission out to the Rendezvous Server.
    //
    if (message.messageType != INVALID_MESSAGE) {
        QueueMessage(message);
    }

    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

QStatus DiscoveryManager::SearchName(const String& name)
{
    QCC_DbgPrintf(("DiscoveryManager::SearchName()\n"));

    if (DiscoveryManagerState != IMPL_RUNNING) {
        QCC_DbgPrintf(("DiscoveryManager::SearchName(): Not IMPL_RUNNING\n"));
        return ER_FAIL;
    }

    QCC_DbgPrintf(("DiscoveryManager::SearchName(): Called for a Searching %s", name.c_str()));

    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

    // Check is the name is already being searched
    if (!searchMap.empty()) {
        map<String, SearchResponseInfo>::iterator it = searchMap.find(name);

        if (it != searchMap.end()) {
            // Release the mutex.
            DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

            // As we are already searching the name, we don't need to do anything
            QCC_DbgPrintf(("DiscoveryManager::SearchName(): Already searching %s", name.c_str()));
            return ER_OK;
        }
    }

    QCC_DbgPrintf(("DiscoveryManager::SearchName(): Adding %s", name.c_str()));

    // Add entry to the map
    SearchResponseInfo temp;
    searchMap.insert(pair<String, SearchResponseInfo>(name, temp));

    // Add the entry to the corresponding current list, sort the list and then run unique function on the list
    // to remove any duplicates
    currentSearchList.push_back(name);
    currentSearchList.sort();

    // If the ClientAuthenticationFailed flag is set, reset it as the Search list
    // has changed
    if (ClientAuthenticationFailed) {
        ClientAuthenticationFailed = false;
    }

    HttpConnection::Method httpMethod = HttpConnection::METHOD_POST;

    RendezvousMessage message;
    ComposeAdvertisementorSearch(false, httpMethod, message);

    //
    // Queue this message for transmission out to the Rendezvous Server.
    //
    if (message.messageType != INVALID_MESSAGE) {
        QueueMessage(message);
    }

    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

QStatus DiscoveryManager::CancelAdvertiseName(const String& name)
{
    QCC_DbgPrintf(("DiscoveryManager::CancelAdvertiseName()\n"));

    if (DiscoveryManagerState != IMPL_RUNNING) {
        QCC_DbgPrintf(("DiscoveryManager::CancelAdvertiseName(): Not IMPL_RUNNING\n"));
        return ER_FAIL;
    }

    QCC_DbgPrintf(("DiscoveryManager::CancelAdvertiseName(): Called for a deleting Advertise %s", name.c_str()));

    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

    // Check is the name is still being advertised
    list<String>::iterator it;

    if (!currentAdvertiseList.empty()) {
        for (it = currentAdvertiseList.begin(); it != currentAdvertiseList.end();) {

            if (*it == name) {

                QCC_DbgPrintf(("DiscoveryManager::CancelAdvertiseName(): Deleting entry %s\n", name.c_str()));

                // Remove the corresponding entry from the currentAdvertiseList and sort it
                currentAdvertiseList.remove(name);
                currentAdvertiseList.sort();

                // If there are no entries in the list, it means that we are
                // deleting all Advertisements/Searches. So use
                // DELETE. Otherwise use POST.
                HttpConnection::Method httpMethod = HttpConnection::METHOD_POST;

                if (currentAdvertiseList.empty()) {
                    httpMethod = HttpConnection::METHOD_DELETE;
                }

                RendezvousMessage message;
                ComposeAdvertisementorSearch(true, httpMethod, message);

                //
                // Queue this message for transmission out to the Rendezvous Server.
                //
                if (message.messageType != INVALID_MESSAGE) {
                    QueueMessage(message);
                }

                // Break out of the loop
                break;

            } else {
                ++it;
            }
        }
    }

    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

QStatus DiscoveryManager::CancelSearchName(const String& name)
{
    QCC_DbgPrintf(("DiscoveryManager::CancelSearchName()\n"));

    if (DiscoveryManagerState != IMPL_RUNNING) {
        QCC_DbgPrintf(("DiscoveryManager::CancelSearchName(): Not IMPL_RUNNING\n"));
        return ER_FAIL;
    }

    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

    // Check is the name is already removed from the searchMap
    if (!searchMap.empty()) {
        map<String, SearchResponseInfo>::iterator it = searchMap.find(name);

        if (it != searchMap.end()) {

            QCC_DbgPrintf(("DiscoveryManager::CancelSearchName(): Deleting entry %s\n", name.c_str()));

            // Send Found callback to remove all the names that we discovered corresponding
            // to this Search from the nameMap
            list<RemoteDaemonServicesInfo>* remoteDaemonServicesInfo = &(it->second.response);
            list<RemoteDaemonServicesInfo>::iterator remoteDaemonServices_it;

            for (remoteDaemonServices_it = remoteDaemonServicesInfo->begin(); remoteDaemonServices_it != remoteDaemonServicesInfo->end();) {
                vector<String> wkn = remoteDaemonServices_it->services;
                if (!wkn.empty()) {

                    if (iceCallback) {

                        QCC_DbgPrintf(("DiscoveryManager::CancelSearchName(): Trying to invoke the iceCallback to clear discovered services with GUID %s corresponding "
                                       "to the find name %s from nameMap\n", remoteDaemonServices_it->remoteGUID.c_str(), name.c_str()));

                        (*iceCallback)(FOUND, remoteDaemonServices_it->remoteGUID, &wkn, 0);
                    }

                    // Purge the StunAndTurnServerInfo
                    map<String, RemoteDaemonStunInfo>::iterator stun_it = StunAndTurnServerInfo.find(remoteDaemonServices_it->remoteGUID);
                    if (stun_it != StunAndTurnServerInfo.end()) {
                        for (uint8_t i = 0; i < remoteDaemonServices_it->services.size(); i++) {
                            stun_it->second.services.remove(remoteDaemonServices_it->services[i]);
                            QCC_DbgPrintf(("DiscoveryManager::CancelSearchName(): Removed service %s from StunAndTurnServerInfo\n", remoteDaemonServices_it->services[i].c_str()));
                        }

                        if (stun_it->second.services.empty()) {
                            StunAndTurnServerInfo.erase(stun_it);
                            QCC_DbgPrintf(("DiscoveryManager::CancelSearchName(): Removed entry for GUID %s from StunAndTurnServerInfo\n", remoteDaemonServices_it->remoteGUID.c_str()));
                        }
                    }
                }

                ++remoteDaemonServices_it;
            }

            // Remove the entry from the searchMap
            searchMap.erase(it);

            // Remove the corresponding entry from the currentSearchList and sort it
            currentSearchList.remove(name);
            currentSearchList.sort();

            // If there are no entries in the list, it means that we are
            // deleting all Advertisements/Searches. So use
            // DELETE. Otherwise use POST.
            HttpConnection::Method httpMethod = HttpConnection::METHOD_POST;

            if (currentSearchList.empty()) {
                httpMethod = HttpConnection::METHOD_DELETE;
            }

            RendezvousMessage message;
            ComposeAdvertisementorSearch(false, httpMethod, message);

            //
            // Queue this message for transmission out to the Rendezvous Server.
            //
            if (message.messageType != INVALID_MESSAGE) {
                QueueMessage(message);
            }
        }
    }

    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

QStatus DiscoveryManager::GetSTUNInfo(bool client, String remotePeerId, STUNServerInfo& stunInfo)
{
    if (client) {
        QCC_DbgPrintf(("DiscoveryManager::GetSTUNInfo(): Trying to retrieve the STUN server info for a service on Daemon with GUID %s\n",
                       remotePeerId.c_str()));

        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

        map<String, RemoteDaemonStunInfo>::iterator stun_it = StunAndTurnServerInfo.find(remotePeerId);
        if (stun_it != StunAndTurnServerInfo.end()) {
            // We found the entry
            stunInfo = stun_it->second.stunInfo;
            DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
            QCC_DbgPrintf(("DiscoveryManager::GetSTUNInfo(): Found the STUN server info\n"));
            return ER_OK;
        }

        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

        QCC_DbgPrintf(("DiscoveryManager::GetSTUNInfo(): Did not find an entry corresponding to the peerId %s\n", remotePeerId.c_str()));

        return ER_FAIL;
    } else {

        QCC_DbgPrintf(("DiscoveryManager::GetSTUNInfo(): Trying to retrieve the STUN server info for client on Daemon with GUID %s\n",
                       remotePeerId.c_str()));

        multimap<String, SessionEntry>::iterator it;

        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
        for (it = IncomingICESessions.begin(); it != IncomingICESessions.end(); it++) {
            if (((it->first) == remotePeerId) && ((it->second).STUNInfoPresent)) {
                stunInfo = (it->second).STUNInfo;
                DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                QCC_DbgPrintf(("DiscoveryManager::GetSTUNInfo(): Found the STUN server info\n"));
                return ER_OK;
            }
        }

        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

        QCC_DbgPrintf(("DiscoveryManager::GetSTUNInfo(): Did not find an entry corresponding to the service\n"));

        return ER_FAIL;
    }
}


QStatus DiscoveryManager::QueueICEAddressCandidatesMessage(bool client, std::pair<String, SessionEntry> sessionDetail)
{
    RendezvousMessage message;

    message.messageType = ADDRESS_CANDIDATES;
    message.httpMethod = HttpConnection::METHOD_POST;

    ICECandidatesMessage* addressCandidates = new ICECandidatesMessage();

    addressCandidates->ice_ufrag = sessionDetail.second.ice_frag;
    addressCandidates->ice_pwd = sessionDetail.second.ice_pwd;
    addressCandidates->destinationPeerID = sessionDetail.first;

    //
    // If a client is sending the address candidate message, then
    // we need to request the Rendezvous Server to append the
    // STUN server information to this message before passing
    // it on to the Daemon running the service as per the
    // interface protocol.
    //
    if (client) {

        addressCandidates->candidates = sessionDetail.second.clientCandidates;
        addressCandidates->requestToAddSTUNInfo = sessionDetail.second.AddSTUNInfo;

        // We just go ahead and directly populate the session request details in OutgoingICESessions. We do not
        // care if a same entry already exists in the map. This is because it is perfectly valid to have two
        // session requests from the same client to the same service on the same remote daemon.
        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
        OutgoingICESessions.insert(sessionDetail);
        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    } else {

        addressCandidates->candidates = sessionDetail.second.serviceCandidates;
        multimap<String, SessionEntry>::iterator it;
        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
        for (it = IncomingICESessions.begin(); it != IncomingICESessions.end(); it++) {
            if ((it->first) == sessionDetail.first) {
                (it->second).peerListener = sessionDetail.second.peerListener;
            }
        }
        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
    }

    message.interfaceMessage = static_cast<InterfaceMessage*>(addressCandidates);

    //
    // Queue this message for transmission out to the Rendezvous Server.
    //
    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
    QueueMessage(message);
    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

void DiscoveryManager::RemoveSessionDetailFromMap(bool client, std::pair<String, SessionEntry> sessionDetail)
{
    multimap<String, SessionEntry>::iterator it;

    if (client) {
        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
        for (it = OutgoingICESessions.begin(); it != OutgoingICESessions.end();) {

            if (it->first == sessionDetail.first) {
                OutgoingICESessions.erase(it++);
            } else {
                ++it;
            }
        }
        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
    } else {
        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
        for (it = IncomingICESessions.begin(); it != IncomingICESessions.end();) {

            if (it->first == sessionDetail.first) {
                IncomingICESessions.erase(it++);
            } else {
                ++it;
            }
        }
        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
    }
}


QStatus DiscoveryManager::QueueProximityMessage(ProximityMessage proximity, list<String> bssids, list<String> btMacIds)
{

    QCC_DbgPrintf(("DiscoveryManager::QueueProximityMessage(): Queueing proximity message"));

    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
    /* Queue a proximity message only if we have active advertisements and searches */
    if (((!(currentAdvertiseList.empty()))) || ((!(currentSearchList.empty())))) {
        RendezvousMessage message;

        message.messageType = PROXIMITY;
        message.httpMethod = HttpConnection::METHOD_POST;

        ProximityMessage* proximityMsg = new ProximityMessage();
        *proximityMsg = proximity;

        message.interfaceMessage = static_cast<InterfaceMessage*>(proximityMsg);

        /* Update the lists */
        currentBSSIDList = bssids;
        currentBTMACList = btMacIds;

        tempSentBSSIDList.clear();
        tempSentBTMACList.clear();

        tempSentBSSIDList = currentBSSIDList;
        tempSentBTMACList = currentBTMACList;

        //
        // Queue this message for transmission out to the Rendezvous Server.
        //
        QueueMessage(message);
    }
    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

    return ER_OK;
}

void DiscoveryManager::ComposeProximityMessage(HttpConnection::Method httpMethod, RendezvousMessage& message)
{
    QCC_DbgPrintf(("DiscoveryManager::ComposeProximityMessage()"));

    ProximityMessage* proximityMsg = new ProximityMessage();

#ifdef ENABLE_PROXIMITY_FRAMEWORK

    /* Return if the current list is empty as we have nothing to send */
    if ((currentBSSIDList.empty()) && (currentBTMACList.empty())) {
        message.messageType = INVALID_MESSAGE;
        return;
    }

    /* Get the current Proximity information */
    if (ProximityScanner) {
        *proximityMsg = ProximityScanner->GetScanResults(currentBSSIDList, currentBTMACList);
    }
#else
    *proximityMsg = proximity[currentProximityIndex];
    currentProximityIndex = ((currentProximityIndex + 1) % 3);
#endif

    message.messageType = PROXIMITY;

    /* Clear the temporary sent lists and populate them with the content of the current lists */
    tempSentBSSIDList.clear();
    tempSentBTMACList.clear();

    tempSentBSSIDList = currentBSSIDList;
    tempSentBTMACList = currentBTMACList;

    /* Compose the message */
    message.interfaceMessage = static_cast<InterfaceMessage*>(proximityMsg);
    message.httpMethod = httpMethod;
}

QStatus DiscoveryManager::Connect(void)
{
    QCC_DbgPrintf(("DiscoveryManager::Connect()"));

    QStatus status = ER_OK;

    if (InterfaceFlags == NetworkInterface::NONE) {
        status = ER_FAIL;
        QCC_LogError(status, ("DiscoveryManager::Connect(): InterfaceFlags = NONE"));
    } else {
        if (!(Connection)) {
            Connection = new RendezvousServerConnection(RendezvousServer, EnableIPv6, UseHTTP);
        }

        /* Set up or update the Persistent Connection if we have active Advertisements or Searches */
        if (((!(currentAdvertiseList.empty()))) || ((!(currentSearchList.empty())))) {
            RendezvousServerConnection::ConnectionFlag connFlag = RendezvousServerConnection::BOTH;

            status = Connection->Connect(InterfaceFlags, connFlag);

            if (status == ER_OK) {
                QCC_DbgPrintf(("DiscoveryManager::Connect(): Successfully connected to the Rendezvous Server"));
            } else {
                status = ER_UNABLE_TO_CONNECT_TO_RENDEZVOUS_SERVER;
                QCC_LogError(status, ("DiscoveryManager::Connect(): %s", QCC_StatusText(status)));
            }
        }
    }

    return status;
}

void* DiscoveryManager::Run(void* arg)
{
    //
    // This method is executed by the Discovery Manager main thread and becomes the
    // center of the Discovery Manager universe.  All incoming and outgoing messages
    // percolate through this thread because of the way we have to deal with
    // interfaces coming up and going down underneath us in a mobile
    // environment.
    //
    QCC_DbgPrintf(("DiscoveryManager::Run()\n"));

    RendezvousMessage message;

    QStatus status;

    vector<Event*> checkEvents, signaledEvents;
    uint32_t waitTimeout;

    bool skipForceInterfaceUpdateFlagReset = false;

    //
    // Create a set of events to wait on.
    // We always wait on the stop event, the timer event and the event used
    // to signal us when an outgoing message is queued or a forced wakeup for
    // a interface update is done.
    //
    checkEvents.push_back(&stopEvent);
    checkEvents.push_back(&WakeEvent);
    checkEvents.push_back(&ConnectionResetEvent);
    checkEvents.push_back(&DisconnectEvent);

    signaledEvents.clear();

    while (!IsStopping()) {

        QCC_DbgPrintf(("Top of Discovery Manager"));

        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

        QCC_DbgPrintf(("Locked DiscoveryManagerMutex"));

        // We need to attempt to do any operation if and only if the ClientAuthenticationFailed flag has not been set

        if (!ClientAuthenticationFailed) {

            //
            // We need an active connection with the Rendezvous Server whenever
            // we have any message to be sent and also whenever we have any active
            // advertisements or find advertised names.
            if ((OutboundMessageQueue.size()) || ((!(currentAdvertiseList.empty()))) || ((!(currentSearchList.empty())))) {

                QCC_DbgPrintf(("DiscoveryManager::Run(): OutboundMessageQueue.size()=%d currentAdvertiseList.empty()=%d currentSearchList.empty()=%d\n",
                               OutboundMessageQueue.size(), currentAdvertiseList.empty(), currentSearchList.empty()));

                if (ForceInterfaceUpdateFlag || (!Connection)) {

                    QCC_DbgPrintf(("DiscoveryManager::Run(): ForceInterfaceUpdateFlag(%d)\n", ForceInterfaceUpdateFlag));

                    /**
                     * Unlock the mutex before the call to connect and lock it back later. This is required to ensure that we do not
                     * lock up the mutex for the time that it takes for the DNS lookup on the server address.
                     **/
                    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

                    status = Connect();
                    QCC_DbgPrintf(("%s: Server connect return status = %s", __FUNCTION__, QCC_StatusText(status)));

                    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

                    LastSentUpdateMessage = INVALID_MESSAGE;

                    if (status == ER_OK) {

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                        /* Release and acquire back the DiscoveryManagerMutex before call to StopScan and
                         * StartScan to ensure that there is no deadlock between the ProximityScanEngine
                         * and DiscoveryManager*/
                        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                        if (ProximityScanner) {
                            /* Stop the proximity scan before start to rule out any race conditions */
                            ProximityScanner->StopScan();
                            ProximityScanner->StartScan();
                        }
                        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif

                        /* If the On Demand connection has been newly setup, create a response event for the same
                         * and add it to checkEvents */
                        if (Connection->GetOnDemandConnectionChanged()) {

                            LastOnDemandMessageSent.Clear();
                            SentMessageOverOnDemandConnection = false;

                            Connection->ResetOnDemandConnectionChanged();

                            if (OnDemandResponseEvent != NULL) {
                                /*Delete the existent OnDemandResponseEvent from checkEvents*/
                                for (vector<Event*>::iterator j = checkEvents.begin(); j != checkEvents.end(); ++j) {
                                    if (*j == OnDemandResponseEvent) {
                                        checkEvents.erase(j);
                                        OnDemandResponseEvent = NULL;
                                        break;
                                    }
                                }
                            }

                            /*Create a response event corresponding to the new On Demand connection*/
                            OnDemandResponseEvent = &(Connection->GetOnDemandSourceEvent());

                            /* Add it to checkEvents */
                            checkEvents.push_back(OnDemandResponseEvent);
                        }

                        /* If the Persistent connection has been newly setup, create a response event for the same
                         * and add it to checkEvents after sending a GET message*/
                        if (Connection->GetPersistentConnectionChanged()) {

                            SentFirstGETMessage = false;

                            Connection->ResetPersistentConnectionChanged();

                            if (PersistentResponseEvent != NULL) {
                                /* Delete the existent PersistentResponseEvent from checkEvents */
                                for (vector<Event*>::iterator j = checkEvents.begin(); j != checkEvents.end(); ++j) {
                                    if (*j == PersistentResponseEvent) {
                                        checkEvents.erase(j);
                                        PersistentResponseEvent = NULL;
                                        break;
                                    }
                                }
                            }

                            /*Create a response event corresponding to the new On Demand connection*/
                            PersistentResponseEvent = &(Connection->GetPersistentSourceEvent());

                            /* Add it to checkEvents */
                            checkEvents.push_back(PersistentResponseEvent);

                            /* Send a GET message over the Persistent connection if the PeerID has a valid value and ClientAuthenticationRequiredFlag
                             * is not set */
                            if ((!PeerID.empty()) && (!ClientAuthenticationRequiredFlag)) {
                                /* Send a GET message */
                                status = SendMessage(GETMessage);

                                if (status != ER_OK) {
                                    /* Disconnect from the Server and set skipForceInterfaceUpdateFlagReset so that we dont end up resetting
                                     * the ForceInterfaceUpdateFlag */
                                    Disconnect();
#ifdef ENABLE_PROXIMITY_FRAMEWORK
                                    /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                                     * to ensure that there is no deadlock between the ProximityScanEngine
                                     * and DiscoveryManager*/
                                    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                                    if (ProximityScanner) {
                                        /* Stop the proximity scan before start to rule out any race conditions */
                                        ProximityScanner->StopScan();
                                    }
                                    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif
                                    skipForceInterfaceUpdateFlagReset = true;
                                } else {
                                    SentFirstGETMessage = true;
                                }
                            }
                        }
                    } else {
                        if (Connection) {
                            /* Call Disconnect to cleanup any intermediate state */
                            Connection->Disconnect();
                            delete Connection;
                            Connection = NULL;
                        }
                    }

                    if (!skipForceInterfaceUpdateFlagReset) {
                        ForceInterfaceUpdateFlag = false;
                    } else {
                        skipForceInterfaceUpdateFlagReset = false;
                    }
                }

                //
                // If we reach a stage where in we are unable to connect to the Rendezvous Server as none of the
                // interfaces are available, we need to flush out all the messages that we have in the OutboundMessageQueue queue.
                //
                // When a new connection is established with the Rendezvous Server at a later point in time, the
                // first messages that would be queued to be sent out would be all the active advertisements and find
                // names and proximity at that time. The Rendezvous Server is equipped to handle this scenario.
                //
                if (!(Connection)) {

                    OutboundMessageQueue.clear();

                    if (InterfaceUpdateAlarm) {
                        DiscoveryManagerTimer.RemoveAlarm(*InterfaceUpdateAlarm);
                        delete InterfaceUpdateAlarm;
                        InterfaceUpdateAlarm = NULL;
                    }

                    InterfaceUpdateAlarm = new Alarm(INTERFACE_UPDATE_MIN_INTERVAL, this, 0, NULL);
                    status = DiscoveryManagerTimer.AddAlarm(*InterfaceUpdateAlarm);

                    if (status != ER_OK) {
                        /* We do not take any action if we are not able to add the alarm to the timer as if some issue comes up with the
                         * connection we handle it resetting up the connection */
                        QCC_LogError(status, ("DiscoveryManager::Run(): Unable to add InterfaceUpdateAlarm to DiscoveryManagerTimer"));
                    }

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                    /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                     * to ensure that there is no deadlock between the ProximityScanEngine
                     * and DiscoveryManager*/
                    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                    if (ProximityScanner) {
                        ProximityScanner->StopScan();
                    }
                    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif

                } else {

                    if (!SentMessageOverOnDemandConnection) {
                        /* If the ClientAuthenticationRequiredFlag is set, we need to perform the client login procedure */
                        if ((PeerID.empty()) || (ClientAuthenticationRequiredFlag)) {

                            if (LastOnDemandMessageSent.messageType != CLIENT_LOGIN) {
                                DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                                status = SendClientLoginFirstRequest();
                                DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

                                if (status != ER_OK) {

                                    /* Disconnect from the Server and set ForceInterfaceUpdateFlag */
                                    Disconnect();
#ifdef ENABLE_PROXIMITY_FRAMEWORK
                                    /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                                     * to ensure that there is no deadlock between the ProximityScanEngine
                                     * and DiscoveryManager*/
                                    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                                    if (ProximityScanner) {
                                        /* Stop the proximity scan before start to rule out any race conditions */
                                        ProximityScanner->StopScan();
                                    }
                                    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif

                                    ForceInterfaceUpdateFlag = true;

                                }
                            }

                        } else {

                            // Send the first GET message over the Persistent connection if we have not done so yet
                            if (!SentFirstGETMessage) {
                                /* Send a GET message */
                                status = SendMessage(GETMessage);

                                if (status != ER_OK) {
                                    /* Disconnect from the Server and set ForceInterfaceUpdateFlag */
                                    Disconnect();
#ifdef ENABLE_PROXIMITY_FRAMEWORK
                                    /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                                     * to ensure that there is no deadlock between the ProximityScanEngine
                                     * and DiscoveryManager*/
                                    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                                    if (ProximityScanner) {
                                        /* Stop the proximity scan before start to rule out any race conditions */
                                        ProximityScanner->StopScan();
                                    }
                                    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif
                                    ForceInterfaceUpdateFlag = true;
                                } else {
                                    SentFirstGETMessage = true;
                                }
                            }

                            if (RegisterDaemonWithServer) {

                                status = SendDaemonRegistrationMessage();

                                if (status != ER_OK) {

                                    /* Disconnect from the Server and set ForceInterfaceUpdateFlag */
                                    Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                                    /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                                     * to ensure that there is no deadlock between the ProximityScanEngine
                                     * and DiscoveryManager*/
                                    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                                    if (ProximityScanner) {
                                        /* Stop the proximity scan before start to rule out any race conditions */
                                        ProximityScanner->StopScan();
                                    }
                                    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif

                                    ForceInterfaceUpdateFlag = true;

                                } else {
                                    /* Clear the RegisterDaemonWithServer if we could send the Daemon Registration Message successfully to
                                     * the Server */
                                    RegisterDaemonWithServer = false;
                                }

                            } else {

                                if (UpdateInformationOnServerFlag) {

                                    QCC_DbgPrintf(("DiscoveryManager::Run(): UpdateInformationOnServerFlag set\n"));

                                    status = HandleUpdatesToServer();

                                    if (status == ER_OK) {
                                        /* Purge all the messages belonging to the message type that we just sent from the OutboundMessageQueue
                                         * as we just sent the latest information to the Server */
                                        PurgeOutboundMessageQueue(LastSentUpdateMessage);

                                        /* If the last sent message as a part of the update sequence was a Proximity message,
                                         * we are done */
                                        if (LastSentUpdateMessage == PROXIMITY) {
                                            UpdateInformationOnServerFlag = false;
                                        }

                                        /* Set the wake event */
                                        WakeEvent.SetEvent();
                                    } else {

                                        UpdateInformationOnServerFlag = false;

                                        /* Disconnect from the Server and set ForceInterfaceUpdateFlag */
                                        Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                                        /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                                         * to ensure that there is no deadlock between the ProximityScanEngine
                                         * and DiscoveryManager*/
                                        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                                        if (ProximityScanner) {
                                            /* Stop the proximity scan before start to rule out any race conditions */
                                            ProximityScanner->StopScan();
                                        }
                                        DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif

                                        ForceInterfaceUpdateFlag = true;
                                    }

                                } else {
                                    //
                                    // If we have messages to send and we have a connection set up with the
                                    // Rendezvous Server, then send the messages.
                                    //
                                    if (OutboundMessageQueue.size()) {

                                        QCC_DbgPrintf(("DiscoveryManager::Run(): Messages about to be sent to Rendezvous Server\n"));

                                        message = OutboundMessageQueue.front();

                                        //
                                        // Send messages over to the Rendezvous Server.
                                        //
                                        if (message.messageType != INVALID_MESSAGE) {
                                            status = SendMessage(message);

                                            //
                                            // If we are unable to send the message, disconnect from the Server and then set ForceInterfaceUpdateFlag.
                                            //
                                            if (status != ER_OK) {
                                                QCC_DbgPrintf(("DiscoveryManager::Run(): SendMessage was unsuccessful"));

                                                /* Disconnect from the Server and set ForceInterfaceUpdateFlag */
                                                Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                                                /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                                                 * to ensure that there is no deadlock between the ProximityScanEngine
                                                 * and DiscoveryManager*/
                                                DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                                                if (ProximityScanner) {
                                                    /* Stop the proximity scan before start to rule out any race conditions */
                                                    ProximityScanner->StopScan();
                                                }
                                                DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif

                                                ForceInterfaceUpdateFlag = true;

                                            } else {
                                                //
                                                // The current message has been sent to the Rendezvous Server.
                                                // So we can discard it.
                                                //
                                                OutboundMessageQueue.pop_front();
                                            }
                                        } else {
                                            //
                                            // The current message is invalid.
                                            // So we can discard it.
                                            //
                                            OutboundMessageQueue.pop_front();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            //
            // We do not have anything to send to the Rendezvous Server
            //
            if ((Connection) && (currentAdvertiseList.empty()) && (currentSearchList.empty())) {
                QCC_DbgPrintf(("DiscoveryManager::Run(): Nothing to send or receive from the Rendezvous Server. Disconnecting from the Rendezvous Server"));

                //
                // Send a delete all message to the Rendezvous Server
                // We do not check the return value of SendMessage as we
                // are anyways going to disconnect
                //
                SendMessage(RendezvousSessionDeleteMessage);

                //
                // So we disconnect from the Rendezvous Server if connected.
                //
                Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                /* Release and acquire back the DiscoveryManagerMutex before call to StopScan
                 * to ensure that there is no deadlock between the ProximityScanEngine
                 * and DiscoveryManager*/
                DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                if (ProximityScanner) {
                    /* Stop the proximity scan before start to rule out any race conditions */
                    ProximityScanner->StopScan();
                }
                DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
#endif

                // Reset the Persistent and On Demand Time Stamps and the SentMessageOverOnDemandConnection flag
                PersistentMessageSentTimeStamp = 0;
                SentMessageOverOnDemandConnection = false;
                OnDemandMessageSentTimeStamp = 0;
            }

        } // end if(!ClientAuthenticationFailed)

        if (Connection) {

            if (!Connection->IsPersistentConnUp()) {
                // Remove the PersistentResponseEvent and OnDemandResponseEvent from checkEvents
                if (PersistentResponseEvent != NULL) {
                    /* Delete the existent PersistentResponseEvent from checkEvents */
                    for (vector<Event*>::iterator j = checkEvents.begin(); j != checkEvents.end(); ++j) {
                        if (*j == PersistentResponseEvent) {
                            QCC_DbgPrintf(("DiscoveryManager::Run(): Removed PersistentResponseEvent\n"));
                            checkEvents.erase(j);
                            PersistentResponseEvent = NULL;
                            break;
                        }
                    }
                }
            }

            if (!Connection->IsOnDemandConnUp()) {
                if (OnDemandResponseEvent != NULL) {
                    /* Delete the existent OnDemandResponseEvent from checkEvents */
                    for (vector<Event*>::iterator j = checkEvents.begin(); j != checkEvents.end(); ++j) {
                        if (*j == OnDemandResponseEvent) {
                            QCC_DbgPrintf(("DiscoveryManager::Run(): Removed OnDemandResponseEvent\n"));
                            checkEvents.erase(j);
                            OnDemandResponseEvent = NULL;
                            break;
                        }
                    }
                }
            }

        } else {
            // Remove the PersistentResponseEvent and OnDemandResponseEvent from checkEvents
            if (PersistentResponseEvent != NULL) {
                /* Delete the existent PersistentResponseEvent from checkEvents */
                for (vector<Event*>::iterator j = checkEvents.begin(); j != checkEvents.end(); ++j) {
                    if (*j == PersistentResponseEvent) {
                        QCC_DbgPrintf(("DiscoveryManager::Run(): Removed PersistentResponseEvent\n"));
                        checkEvents.erase(j);
                        PersistentResponseEvent = NULL;
                        break;
                    }
                }
            }

            if (OnDemandResponseEvent != NULL) {
                /* Delete the existent OnDemandResponseEvent from checkEvents */
                for (vector<Event*>::iterator j = checkEvents.begin(); j != checkEvents.end(); ++j) {
                    if (*j == OnDemandResponseEvent) {
                        QCC_DbgPrintf(("DiscoveryManager::Run(): Removed OnDemandResponseEvent\n"));
                        checkEvents.erase(j);
                        OnDemandResponseEvent = NULL;
                        break;
                    }
                }
            }
        }

        //
        // We are going to go to sleep, so
        // we definitely need to release other (user) threads that might
        // be waiting to talk to us.
        //
        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

        waitTimeout = GetWaitTimeOut();

        /* Override the wait to make it infinite if we are not connected to the Server.
         * This is required so that we dont aggressively try to keep connecting to the Server*/
        if (!Connection) {
            waitTimeout = Event::WAIT_FOREVER;
        }

        status = Event::Wait(checkEvents, signaledEvents, waitTimeout);

        if (status != ER_OK) {

            QCC_DbgPrintf(("DiscoveryManager::Run(): Wait failed or timed out: waitTimeout = %d, status = %s \n", waitTimeout, QCC_StatusText(status)));

            /* If Wait fails or times out, Disconnect and reconnect to the Server */
            Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
            if (ProximityScanner) {
                /* Stop the proximity scan before start to rule out any race conditions */
                ProximityScanner->StopScan();
            }
#endif

            ForceInterfaceUpdateFlag = true;

            signaledEvents.clear();
        }

        //
        // Loop over the events for which we expect something has happened
        //
        for (vector<Event*>::iterator i = signaledEvents.begin(); i != signaledEvents.end(); ++i) {
            if (*i == &stopEvent) {

                QCC_DbgPrintf(("DiscoveryManager::Run(): Stop event fired\n"));

                //
                // Disconnect from the Rendezvous Server if connected.
                //
                Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                if (ProximityScanner) {
                    /* Stop the proximity scan before start to rule out any race conditions */
                    ProximityScanner->StopScan();
                }
#endif

                //
                // We heard the stop event, so reset it.  We'll pop out of the
                // server loop when we run through it again (above).
                //
                stopEvent.ResetEvent();

            } else if (*i == &WakeEvent) {
                //
                // The trigger is a wake event
                //
                QCC_DbgPrintf(("DiscoveryManager::Run(): Wake event fired\n"));

                WakeEvent.ResetEvent();

            } else if (*i == &ConnectionResetEvent) {
                //
                // The trigger is a HTTP reset event
                //
                QCC_DbgPrintf(("DiscoveryManager::Run(): HTTP reset event fired\n"));

                Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                if (ProximityScanner) {
                    /* Stop the proximity scan before start to rule out any race conditions */
                    ProximityScanner->StopScan();
                }
#endif

                // We just set ForceInterfaceUpdateFlag to true so that the loop handles the
                // setting up of a new connection
                ForceInterfaceUpdateFlag = true;

                ConnectionResetEvent.ResetEvent();

            } else if (*i == &DisconnectEvent) {
                //
                // The trigger is a HTTP disconnect event
                //
                QCC_DbgPrintf(("DiscoveryManager::Run(): HTTP disconnect event fired\n"));

                Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                if (ProximityScanner) {
                    /* Stop the proximity scan before start to rule out any race conditions */
                    ProximityScanner->StopScan();
                }
#endif

                DisconnectEvent.ResetEvent();

            } else if (Connection) {
                if ((Connection->IsOnDemandConnUp()) && (*i == OnDemandResponseEvent)) {

                    QCC_DbgPrintf(("DiscoveryManager::Run(): OnDemandResponseEvent fired\n"));

                    HttpConnection::HTTPResponse response;

                    /* Fetch the response */
                    status = Connection->FetchResponse(true, response);

                    if (status == ER_OK) {

                        HandleOnDemandConnectionResponse(response);

                    } else {

                        /* Something has gone wrong. So we disconnect and set the ForceInterfaceUpdateFlag */

                        Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                        if (ProximityScanner) {
                            /* Stop the proximity scan before start to rule out any race conditions */
                            ProximityScanner->StopScan();
                        }
#endif

                        ForceInterfaceUpdateFlag = true;

                    }

                } else if ((Connection->IsPersistentConnUp()) && (*i == PersistentResponseEvent)) {

                    QCC_DbgPrintf(("DiscoveryManager::Run(): PersistentResponseEvent fired\n"));

                    HttpConnection::HTTPResponse response;

                    /* Fetch the response */
                    status = Connection->FetchResponse(false, response);

                    if (status == ER_OK) {

                        HandlePersistentConnectionResponse(response);

                    } else {

                        /* Something has gone wrong. So we disconnect and set the ForceInterfaceUpdateFlag */
                        Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                        if (ProximityScanner) {
                            /* Stop the proximity scan before start to rule out any race conditions */
                            ProximityScanner->StopScan();
                        }
#endif

                        ForceInterfaceUpdateFlag = true;

                    }
                }
            }
        }

        signaledEvents.clear();
    }

    return 0;
}

/**
 * Ensure that the function invoking this function locks the DiscoveryManagerMutex.
 */
void DiscoveryManager::QueueMessage(RendezvousMessage message)
{
    QCC_DbgPrintf(("DiscoveryManager::QueueMessage(): messageType(%s) httpMethod(%d)\n",
                   (PrintMessageType(message.messageType)).c_str(), message.httpMethod));

    if (message.messageType != INVALID_MESSAGE) {

        OutboundMessageQueue.push_back(message);
        QCC_DbgPrintf(("DiscoveryManager::QueueMessage: Set the wake event\n"));
        WakeEvent.SetEvent();
    }
}

void DiscoveryManager::PurgeOutboundMessageQueue(MessageType messageType)
{
    QCC_DbgPrintf(("DiscoveryManager::PurgeOutboundMessageQueue(): OutboundMessageQueue.size() = %d", OutboundMessageQueue.size()));

    for (list<RendezvousMessage>::iterator i = OutboundMessageQueue.begin(); i != OutboundMessageQueue.end();) {
        if ((*i).messageType == messageType) {
            OutboundMessageQueue.erase(i++);
        } else {
            ++i;
        }
    }
}

QStatus DiscoveryManager::SendMessage(RendezvousMessage message)
{
    QStatus status = ER_OK;

    QCC_DbgPrintf(("DiscoveryManager::SendMessage()\n"));

    if (message.messageType != INVALID_MESSAGE) {

        QCC_DbgPrintf(("DiscoveryManager::SendMessage(): Sending %s message\n", PrintMessageType(message.messageType).c_str()));

        HttpConnection::Method httpMethod;
        String uri;
        bool contentPresent = false;
        String content;

        /* Prepare the HTTP message */
        status = PrepareOutgoingMessage(message, httpMethod, uri, contentPresent, content);

        if (ER_OK != status) {
            QCC_LogError(status, ("DiscoveryManager::SendMessage(): PrepareOutgoingMessage() failed"));
            return status;
        }

        if (!Connection) {
            status = ER_NOT_CONNECTED_TO_RENDEZVOUS_SERVER;
            QCC_LogError(status, ("DiscoveryManager::SendMessage(): %s", QCC_StatusText(status)));
        } else {
            /* Send the HTTP message to the Server */
            if (Connection->IsConnectedToServer()) {

                bool sendMessageOverPersistentConnection = false;

                if ((httpMethod == HttpConnection::METHOD_GET) && (message.messageType != TOKEN_REFRESH)) {
                    sendMessageOverPersistentConnection = true;
                }

                status = Connection->SendMessage(sendMessageOverPersistentConnection, httpMethod, uri, contentPresent, content);

                if (ER_OK == status) {
                    QCC_DbgPrintf(("DiscoveryManager::SendMessage(): Connection->SendMessage() returned ER_OK"));

                    /* If the message was sent over the On-Demand connection, then update LastOnDemandMessageSent to reflect
                     * the message that was just sent and also update the appropriate time stamp to indicate when
                     * a message was sent to the Server*/
                    if (!sendMessageOverPersistentConnection) {
                        LastOnDemandMessageSent.Clear();
                        LastOnDemandMessageSent = message;
                        OnDemandMessageSentTimeStamp = GetTimestamp();
                        SentMessageOverOnDemandConnection = true;
                    } else {
                        PersistentMessageSentTimeStamp = GetTimestamp();
                    }

                } else {
                    status = ER_UNABLE_TO_SEND_MESSAGE_TO_RENDEZVOUS_SERVER;
                    QCC_LogError(status, ("DiscoveryManager::SendMessage(): %s", QCC_StatusText(status)));
                }
            } else {
                status = ER_NOT_CONNECTED_TO_RENDEZVOUS_SERVER;
                QCC_LogError(status, ("DiscoveryManager::SendMessage(): %s", QCC_StatusText(status)));
            }
        }

    } else {

        /* We should never reach here as Run checks that the message is valid
           before passing it on to this function.*/
        status = ER_INVALID_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
        QCC_LogError(status, ("DiscoveryManager::SendMessage(): %s", QCC_StatusText(status)));
    }

    return status;
}

QStatus DiscoveryManager::HandleSearchMatchResponse(SearchMatchResponse response)
{
    QCC_DbgPrintf(("DiscoveryManager::HandleSearchMatchResponse(): Trying to invoke found callback for service %s on Daemon with GUID %s which is a response to the search %s\n",
                   response.service.c_str(), response.peerAddr.c_str(), response.searchedService.c_str()));

    QStatus status = ER_OK;

    vector<String> wkn;

    bool found = false;

    //
    // See if the well-known name that has been found is in our list of names
    // to be found.
    //
    map<String, SearchResponseInfo>::iterator it = searchMap.find(response.searchedService);

    if (it != searchMap.end()) {

        QCC_DbgPrintf(("DiscoveryManager::HandleSearchMatchResponse(): Found the corresponding entry %s in the searchMap\n", response.searchedService.c_str()));

        // Update the searchMap with the new information
        list<RemoteDaemonServicesInfo>* remoteDaemonServicesInfo = &(it->second.response);
        list<RemoteDaemonServicesInfo>::iterator remoteDaemonServices_it;

        for (remoteDaemonServices_it = remoteDaemonServicesInfo->begin(); remoteDaemonServices_it != remoteDaemonServicesInfo->end();) {

            if (remoteDaemonServices_it->remoteGUID == response.peerAddr) {

                // Check if we have already discovered this service
                for (uint8_t i = 0; i < remoteDaemonServices_it->services.size(); i++) {
                    if (response.service == remoteDaemonServices_it->services[i]) {
                        QCC_DbgPrintf(("DiscoveryManager::HandleSearchMatchResponse(): The service %s with GUID %s has already been discovered\n", response.service.c_str(), response.peerAddr.c_str()));
                        found = true;
                        // Break out of the remoteDaemonServices_it->services for loop
                        break;
                    }
                }

                if (!found) {
                    // Update the services list if this service that has been discovered is not a part of that list and also update
                    // the services list accordingly in StunAndTurnServerInfo

                    remoteDaemonServices_it->services.push_back(response.service);
                    wkn.push_back(response.service);

                    map<String, RemoteDaemonStunInfo>::iterator stun_it = StunAndTurnServerInfo.find(response.peerAddr);
                    if (stun_it != StunAndTurnServerInfo.end()) {
                        stun_it->second.services.push_back(response.service);
                        stun_it->second.stunInfo = response.STUNInfo;
                    } else {
                        RemoteDaemonStunInfo temp;
                        temp.stunInfo = response.STUNInfo;
                        temp.services.push_back(response.service);
                        StunAndTurnServerInfo.insert(pair<String, RemoteDaemonStunInfo>(response.peerAddr, temp));
                    }

                    found = true;

                    QCC_DbgPrintf(("DiscoveryManager::HandleSearchMatchResponse(): Added service %s with GUID %s to searchMap and StunAndTurnServerInfo\n",
                                   response.service.c_str(), response.peerAddr.c_str()));
                }

                // Break out of the remoteDaemonServices_it for loop
                break;
            }

            ++remoteDaemonServices_it;
        }

        if (!found) {
            // Insert a new entry corresponding to this GUID and service discovered in the searchMap and StunAndTurnServerInfo
            RemoteDaemonServicesInfo temp;
            temp.remoteGUID = response.peerAddr;
            temp.services.push_back(response.service);

            it->second.response.push_back(temp);
            wkn.push_back(response.service);

            // Update the StunAndTurnServerInfo with the new information
            RemoteDaemonStunInfo tempStunInfo;
            tempStunInfo.stunInfo = response.STUNInfo;
            tempStunInfo.services.push_back(response.service);
            StunAndTurnServerInfo.insert(pair<String, RemoteDaemonStunInfo>(response.peerAddr, tempStunInfo));

        }
    }

    if (!wkn.empty()) {
        if (iceCallback) {

            QCC_DbgPrintf(("DiscoveryManager::HandleSearchMatchResponse(): Trying to invoke the iceCallback\n"));

            (*iceCallback)(FOUND, response.peerAddr, &wkn, 0xFF);
        }
    }

    return status;
}

QStatus DiscoveryManager::HandleStartICEChecksResponse(StartICEChecksResponse response)
{
    QCC_DbgPrintf(("DiscoveryManager::HandleStartICEChecksResponse(): peerAddr = %s\n", response.peerAddr.c_str()));

    QStatus status = ER_OK;

    // Invoke the call back to tell the DaemonICETransport that the Address Candidates message corresponding to a Service
    // has been successfully delivered to the other peer
    multimap<String, SessionEntry>::iterator it;
    for (it = IncomingICESessions.begin(); it != IncomingICESessions.end(); it++) {
        if ((it->first) == response.peerAddr) {
            ((it->second).peerListener)->SetPeerCandiates((it->second).clientCandidates, (it->second).ice_frag, (it->second).ice_pwd);
            IncomingICESessions.erase(it);
            break;
        }
    }

    return status;
}

QStatus DiscoveryManager::HandleMatchRevokedResponse(MatchRevokedResponse response)
{

    QCC_DbgPrintf(("DiscoveryManager::HandleMatchRevokedResponse(): Trying to invoke found callback to record unavailability of previously available services "
                   "on Daemon with GUID %s\n", response.peerAddr.c_str()));

    QStatus status = ER_OK;

    //
    // If deleteall has been set, all the services from the Daemon with GUID peerID
    // should be deleted and the outgoing session maps need to be purged the same way as it is
    // done when a rendezvous session closed message is received
    //
    if (response.deleteAll) {

        QCC_DbgPrintf(("DiscoveryManager::HandleMatchRevokedResponse(): Delete All Set for peerAddress = %s", response.peerAddr.c_str()));

        // Remove the entry corresponding to this peerAddress from the StunAndTurnServerInfo as the remote peer has revoked all
        // its advertisements, we do not need to know the STUN server address as we anyways wont initiate any connections to that
        // remote daemon
        StunAndTurnServerInfo.erase(response.peerAddr);

        // Remove the entries corresponding to response.peerAddr from the searchMap
        map<String, SearchResponseInfo>::iterator it;
        for (it = searchMap.begin(); it != searchMap.end(); it++) {

            list<RemoteDaemonServicesInfo>* remoteDaemonServicesInfo = &(it->second.response);
            list<RemoteDaemonServicesInfo>::iterator remoteDaemonServices_it;

            for (remoteDaemonServices_it = remoteDaemonServicesInfo->begin(); remoteDaemonServices_it != remoteDaemonServicesInfo->end();) {

                if (remoteDaemonServices_it->remoteGUID == response.peerAddr) {

                    remoteDaemonServicesInfo->erase(remoteDaemonServices_it);

                    // Break out of the remoteDaemonServices_it for loop
                    break;
                }

                ++remoteDaemonServices_it;
            }
        }

        //
        // Invoke the found callback to purge the nameMap
        //
        if (iceCallback) {

            QCC_DbgPrintf(("DiscoveryManager::PurgeNameMap(): Trying to invoke the iceCallback\n"));

            (*iceCallback)(FOUND, response.peerAddr, NULL, 0);
        }

    } else {
        if (!response.services.empty()) {
            QCC_DbgPrintf(("DiscoveryManager::HandleMatchRevokedResponse(): Received a list of services being revoked\n"));

            list<String> tempList = response.services;

            // Purge the StunAndTurnServerInfo
            map<String, RemoteDaemonStunInfo>::iterator stun_it = StunAndTurnServerInfo.find(response.peerAddr);
            if (stun_it != StunAndTurnServerInfo.end()) {
                while (!tempList.empty()) {
                    stun_it->second.services.remove(tempList.front());
                    QCC_DbgPrintf(("DiscoveryManager::HandleMatchRevokedResponse(): Removed service %s from StunAndTurnServerInfo\n", tempList.front().c_str()));
                    tempList.pop_front();
                }

                if (stun_it->second.services.empty()) {
                    StunAndTurnServerInfo.erase(stun_it);
                    QCC_DbgPrintf(("DiscoveryManager::HandleMatchRevokedResponse(): Removed entry for GUID %s from StunAndTurnServerInfo\n", response.peerAddr.c_str()));
                }
            }

            // Purge the searchMap
            map<String, SearchResponseInfo>::iterator it;
            for (it = searchMap.begin(); it != searchMap.end(); it++) {

                tempList = response.services;

                // Update the searchMap with the new information
                list<RemoteDaemonServicesInfo>* remoteDaemonServicesInfo = &(it->second.response);
                list<RemoteDaemonServicesInfo>::iterator remoteDaemonServices_it;

                for (remoteDaemonServices_it = remoteDaemonServicesInfo->begin(); remoteDaemonServices_it != remoteDaemonServicesInfo->end();) {

                    if (remoteDaemonServices_it->remoteGUID == response.peerAddr) {

                        while (!tempList.empty()) {
                            vector<String>::iterator services_it;
                            for (services_it = remoteDaemonServices_it->services.begin(); services_it != remoteDaemonServices_it->services.end();) {

                                if (*services_it == tempList.front()) {
                                    remoteDaemonServices_it->services.erase(services_it);
                                    QCC_DbgPrintf(("DiscoveryManager::HandleSearchMatchResponse(): The service %s with GUID %s has been removed from searchMap\n",
                                                   tempList.front().c_str(), response.peerAddr.c_str()));
                                    // Break out of the remoteDaemonServices_it->services for loop
                                    break;
                                } else {
                                    ++services_it;
                                }
                            }

                            tempList.pop_front();
                        }

                        // Break out of the remoteDaemonServices_it for loop
                        break;
                    }

                    ++remoteDaemonServices_it;
                }
            }

            vector<String> wkn;

            while (!response.services.empty()) {
                wkn.push_back(response.services.front());
                response.services.pop_front();
            }

            //
            // Invoke the found callback to purge the nameMap
            //
            if (iceCallback) {

                QCC_DbgPrintf(("DiscoveryManager::HandleMatchRevokedResponse(): Trying to invoke the iceCallback\n"));

                (*iceCallback)(FOUND, response.peerAddr, &wkn, 0);
            }
        }
    }

    return status;
}

QStatus DiscoveryManager::HandleAddressCandidatesResponse(AddressCandidatesResponse response)
{

    QCC_DbgPrintf(("DiscoveryManager::HandleAddressCandidatesResponse(): Trying to invoke either the AllocateICESession or StartICEChecks callback\n"));

    QStatus status = ER_OK;

    // Flag used to indicate that the AllocateICESession callback was invoked
    bool invokedAllocateICESession = false;

    // Flag used to indicate that the StartICEChecks callback was invoked
    bool invokedStartICEChecks = false;

    //
    // If the address candidates message has been sent from a remote client to a local service, we need to
    // invoke the AllocateICESession callback or else we have to invoke the StartICEChecks callback
    //

    //
    // Check if we have received the address candidates from a remote service in response to the candidates that we sent out for a local
    // client
    //

    multimap<String, SessionEntry>::iterator it;

    for (it = OutgoingICESessions.begin(); it != OutgoingICESessions.end(); it++) {

        if (((it->first) == response.peerAddr)) {
            // Populate the details in the ActiveOutgoingICESessions map
            (it->second).serviceCandidates = response.candidates;
            (it->second).ice_frag = response.ice_ufrag;
            (it->second).ice_pwd = response.ice_pwd;

            // Invoke the callback to inform the DaemonICETransport that the Service candiates have been received
            ((it->second).peerListener)->SetPeerCandiates((it->second).serviceCandidates, response.ice_ufrag, response.ice_pwd);

            // Remove the entry from the OutgoingICESessions
            OutgoingICESessions.erase(it);

            invokedStartICEChecks = true;

            // break out of the for loop
            break;
        }
    }

    // If the StartChecksCallback was not invoked
    if (!invokedStartICEChecks) {
        // Check if the address candidates message received from the Client is in response
        // to a advertisement from this daemon.
        multimap<String, SearchResponseInfo>::iterator it;

        // Populate an entry corresponding to this in
        // IncomingICESession so that we can look that up later and direct the
        // address candidates that the service would generate to the appropriate client
        SessionEntry entry(true, response.candidates, response.ice_ufrag, response.ice_pwd);

        if (response.STUNInfoPresent) {
            entry.SetSTUNInfo(response.STUNInfo);
        }

        IncomingICESessions.insert(pair<String, SessionEntry>(response.peerAddr, entry));

        vector<String> wkn;

        // Invoke the AllocateICESession callback
        if (iceCallback) {
            QCC_DbgPrintf(("DiscoveryManager::HandleAddressCandidatesResponse(): Invoking the AllocateICESession callback\n"));
            (*iceCallback)(ALLOCATE_ICE_SESSION, response.peerAddr, &wkn, 0xFF);
        }

        invokedAllocateICESession = true;
    }

    if ((!invokedAllocateICESession) && (!invokedStartICEChecks)) {
        // We do not return an ER_FAIL because it might be the case that we received the address candidates from a peer for
        // a service or client that we have stopped advertising. Though there is also a possibility that there is a glitch
        // in the message, we are ok with it because if the peer is really interested it would try to send candidates again
        QCC_DbgPrintf(("DiscoveryManager::HandleAddressCandidatesResponse(): Neither the AllocateICESession nor the StartICEChecks callback was invoked\n"));
    }

    return status;
}

QStatus DiscoveryManager::HandlePersistentMessageResponse(Json::Value payload)
{
    QCC_DbgPrintf(("DiscoveryManager::HandlePersistentMessageResponse()\n"));
    QStatus status = ER_OK;


    //
    // If there is no callback, we can't tell the user anything about what is
    // going on, so it's pointless to go any further.
    //
    if (!iceCallback) {
        QCC_DbgPrintf(("DiscoveryManager::HandlePersistentMessageResponse(): No callback, so nothing to do\n"));

        // We return an ER_OK because this is not an error caused by the received response
        return status;
    }

    /* Parse the response */
    ResponseMessage response;

    status = ParseMessagesResponse(payload, response);

    if (status != ER_OK) {
        status = ER_INVALID_PERSISTENT_CONNECTION_MESSAGE_RESPONSE;
        QCC_LogError(status, ("DiscoveryManager::HandlePersistentMessageResponse(): %s", QCC_StatusText(status)));
    } else {
        QCC_DbgPrintf(("DiscoveryManager::HandlePersistentMessageResponse(): ParseMessagesResponse succeeded\n"));
        //
        // If there are no messages in the response, we can't tell the user anything about what is
        // going on, so it's pointless to go any further.
        //
        if (response.msgs.empty()) {
            status = ER_INVALID_PERSISTENT_CONNECTION_MESSAGE_RESPONSE;
            QCC_LogError(status, ("DiscoveryManager::HandlePersistentMessageResponse(): No messages in the response\n"));

            // We return an ER_FAIL because this is an error in the received response
            return status;
        }

        QCC_DbgPrintf(("DiscoveryManager::HandlePersistentMessageResponse(): Received number of responses = %d\n", response.msgs.size()));

        //
        // Iterate through the responses
        //
        list<Response>::iterator resp_it;

        for (resp_it = response.msgs.begin(); resp_it != response.msgs.end(); resp_it++) {

            if (resp_it->type != INVALID_RESPONSE) {

                QCC_DbgPrintf(("DiscoveryManager::HandlePersistentMessageResponse(): type = %s\n", PrintResponseType(resp_it->type).c_str()));
                //
                // Requested service(s) has been found. Handle it
                // by invoking the Found callback
                //
                if (resp_it->type == SEARCH_MATCH_RESPONSE) {
                    SearchMatchResponse* SearchMatch = static_cast<SearchMatchResponse*>(resp_it->response);

                    if (ER_OK != HandleSearchMatchResponse(*SearchMatch)) {
                        status = ER_INVALID_PERSISTENT_CONNECTION_MESSAGE_RESPONSE;
                        QCC_LogError(status, ("DiscoveryManager::HandlePersistentMessageResponse(): Received an erroneous search match response"));
                    }
                }
                //
                // Previously found service is no longer available. Handle it
                // by invoking the Found callback and setting ttl=0 so that the
                // entry is removed from the nameMap
                //
                else if (resp_it->type == MATCH_REVOKED_RESPONSE) {
                    MatchRevokedResponse* MatchRevoked = static_cast<MatchRevokedResponse*>(resp_it->response);

                    if (ER_OK != HandleMatchRevokedResponse(*MatchRevoked)) {
                        status = ER_INVALID_PERSISTENT_CONNECTION_MESSAGE_RESPONSE;
                        QCC_LogError(status, ("DiscoveryManager::HandlePersistentMessageResponse(): Received an erroneous match revoked response"));
                    }
                }
                //
                // Address Candidates have been received from a service or client.
                // Handle it by invoking the AllocateICESession or StartICEChecks
                // callback accordingly.
                //
                else if (resp_it->type == ADDRESS_CANDIDATES_RESPONSE) {
                    AddressCandidatesResponse* AddressCandidates = static_cast<AddressCandidatesResponse*>(resp_it->response);

                    if (ER_OK != HandleAddressCandidatesResponse(*AddressCandidates)) {
                        status = ER_INVALID_PERSISTENT_CONNECTION_MESSAGE_RESPONSE;
                        QCC_LogError(status, ("DiscoveryManager::HandlePersistentMessageResponse(): Received an erroneous address candidates response"));
                    }
                }
                //
                // Start ICE checks response has been received.
                // Handle it accordingly.
                //
                else if (resp_it->type ==  START_ICE_CHECKS_RESPONSE) {
                    StartICEChecksResponse* StartICEChecks = static_cast<StartICEChecksResponse*>(resp_it->response);

                    if (ER_OK != HandleStartICEChecksResponse(*StartICEChecks)) {
                        status = ER_INVALID_PERSISTENT_CONNECTION_MESSAGE_RESPONSE;
                        QCC_LogError(status, ("DiscoveryManager::HandlePersistentMessageResponse(): Received an erroneous start ICE checks response"));
                    }
                }
            } else {
                //
                // This is a forbidden state. We should never reach here.
                //
                status = ER_INVALID_PERSISTENT_CONNECTION_MESSAGE_RESPONSE;
                QCC_LogError(status, ("DiscoveryManager::HandlePersistentMessageResponse(): %s", QCC_StatusText(status)));
            }
        }
    }

    return status;
}

void DiscoveryManager::HandlePersistentConnectionResponse(HttpConnection::HTTPResponse& response)
{
    QCC_DbgPrintf(("DiscoveryManager::HandlePersistentConnectionResponse()\n"));
    QStatus status;

    /* Check the status code in the response */
    if (response.statusCode == HttpConnection::HTTP_STATUS_OK) {

        /* Handle the response */
        if (response.payloadPresent) {

            status = HandlePersistentMessageResponse(response.payload);

            if (status != ER_OK) {
                Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                if (ProximityScanner) {
                    /* Stop the proximity scan before start to rule out any race conditions */
                    ProximityScanner->StopScan();
                }
#endif

                ForceInterfaceUpdateFlag = true;
            }

        }

        /* Send another GET message to the Rendezvous Server */
        status = SendMessage(GETMessage);

        if (status != ER_OK) {
            status = ER_UNABLE_TO_SEND_MESSAGE_TO_RENDEZVOUS_SERVER;
            QCC_LogError(status, ("DiscoveryManager::HandlePersistentConnectionResponse(): %s", QCC_StatusText(status)));

            Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
            if (ProximityScanner) {
                /* Stop the proximity scan before start to rule out any race conditions */
                ProximityScanner->StopScan();
            }
#endif

            ForceInterfaceUpdateFlag = true;
        }

    } else if (response.statusCode == HttpConnection::HTTP_UNAUTHORIZED_REQUEST) {

        status = ER_RENDEZVOUS_SERVER_ERR401_UNAUTHORIZED_REQUEST;
        QCC_LogError(status, ("DiscoveryManager::HandlePersistentConnectionResponse(): %s", QCC_StatusText(status)));

        if (!ClientAuthenticationRequiredFlag) {
            /* Disconnect */
            Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
            if (ProximityScanner) {
                /* Stop the proximity scan before start to rule out any race conditions */
                ProximityScanner->StopScan();
            }
#endif

            /* We need to re-authenticate with the Server */
            ClientAuthenticationRequiredFlag = true;
            ForceInterfaceUpdateFlag = true;
        }

    } else {

        status = ER_RENDEZVOUS_SERVER_UNRECOVERABLE_ERROR;
        QCC_LogError(status, ("DiscoveryManager::HandlePersistentConnectionResponse(): %s", QCC_StatusText(status)));

        /* If any other http status code is received, we just disconnect and reconnect after INTERFACE_UPDATE_MIN_INTERVAL */
        Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
        if (ProximityScanner) {
            /* Stop the proximity scan before start to rule out any race conditions */
            ProximityScanner->StopScan();
        }
#endif

        if (InterfaceUpdateAlarm) {
            DiscoveryManagerTimer.RemoveAlarm(*InterfaceUpdateAlarm);
            delete InterfaceUpdateAlarm;
            InterfaceUpdateAlarm = NULL;
        }

        InterfaceUpdateAlarm = new Alarm(INTERFACE_UPDATE_MIN_INTERVAL, this, 0, NULL);
        status = DiscoveryManagerTimer.AddAlarm(*InterfaceUpdateAlarm);

        if (status != ER_OK) {
            /* We do not take any action if we are not able to add the alarm to the timer as if some issue comes up with the
             * connection we handle it resetting up the connection */
            QCC_LogError(status, ("DiscoveryManager::HandlePersistentConnectionResponse(): Unable to add InterfaceUpdateAlarm to DiscoveryManagerTimer"));
        }
    }
}

String DiscoveryManager::PrintMessageType(MessageType type) {
    String retStr = String("INVALID_MESSAGE");
    switch (type) {
    case ADVERTISEMENT:
        retStr = String("ADVERTISEMENT");
        break;

    case SEARCH:
        retStr = String("SEARCH");
        break;

    case ADDRESS_CANDIDATES:
        retStr = String("ADDRESS CANDIDATES");
        break;

    case PROXIMITY:
        retStr = String("PROXIMITY");
        break;

    case RENDEZVOUS_SESSION_DELETE:
        retStr = String("RENDEZVOUS SESSION DELETE");
        break;

    case GET_MESSAGE:
        retStr = String("GET MESSAGE");
        break;

    case CLIENT_LOGIN:
        retStr = String("CLIENT LOGIN");
        break;

    case DAEMON_REGISTRATION:
        retStr = String("DAEMON REGISTRATION");
        break;

    case TOKEN_REFRESH:
        retStr = String("TOKEN_REFRESH");
        break;

    case INVALID_MESSAGE:
    default:
        break;
    }
    return retStr;
}

QStatus DiscoveryManager::UpdateInformationOnServer(MessageType messageType, bool rdvzSessionActive)
{
    QStatus status = ER_OK;

    QCC_DbgPrintf(("DiscoveryManager::UpdateInformationOnServer(): messageType(%s) rdvzSessionActive(%d)",
                   PrintMessageType(messageType).c_str(), rdvzSessionActive));

    list<String> tempSentList;
    list<String> tempCurrentList;
    list<String> tempSentBTList;
    list<String> tempCurrentBTList;

    if (messageType == ADVERTISEMENT) {

        tempSentList = lastSentAdvertiseList;
        tempCurrentList = currentAdvertiseList;

    } else if (messageType == SEARCH) {

        tempSentList = lastSentSearchList;
        tempCurrentList = currentSearchList;

    } else if (messageType == PROXIMITY) {
#ifdef ENABLE_PROXIMITY_FRAMEWORK
        /* Get the current Proximity information */
        if (ProximityScanner) {
            ProximityScanner->GetScanResults(currentBSSIDList, currentBTMACList);

            tempSentList = lastSentBSSIDList;
            tempCurrentList = currentBSSIDList;
            tempSentBTList = lastSentBTMACList;
            tempCurrentBTList = currentBTMACList;
        }
#endif
    } else {
        status = ER_FAIL;
        QCC_DbgPrintf(("DiscoveryManager::UpdateInformationOnServer(): Invalid RendezvousMessage Type %d", messageType));
    }

    if (status == ER_OK) {
        HttpConnection::Method httpMethod = HttpConnection::METHOD_POST;

        // See if the current list has changed as compared to what was sent to Rendezvous Server. If it has, we
        // need to send an update.
        bool hasChanged = false;

        /* If the rdvzSessionActive flag has been set to false, we need to resend all advertisements, searches and proximity
         * using the POST method */
        if (!rdvzSessionActive) {
            hasChanged = true;
        } else {
            /* If the rdvzSessionActive flag has been set to true, we need to just send advertisements, searches and proximity if anything
             * has changes w.r.t what was last sent to the Server */

            QCC_DbgPrintf(("DiscoveryManager::UpdateInformationOnServer(): httpMethod = %d", httpMethod));

            if (tempSentList.size() != tempCurrentList.size()) {
                hasChanged = true;
            } else {
                if (!tempCurrentList.empty()) {

                    while (!tempCurrentList.empty() && !tempSentList.empty()) {
                        if (tempCurrentList.front() != tempSentList.front()) {
                            hasChanged = true;
                            break;
                        }
                        tempCurrentList.pop_front();
                        tempSentList.pop_front();
                    }
                }
            }

#ifdef ENABLE_PROXIMITY_FRAMEWORK
            /* If we are trying to send a Proximity message and if the Wi-Fi BSSID list has not changed,
             * check if the BT MAC IDs list has changed */
            if ((!hasChanged) && (messageType == PROXIMITY)) {

                if (tempSentBTList.empty()) {
                    httpMethod = HttpConnection::METHOD_POST;
                }

                QCC_DbgPrintf(("DiscoveryManager::UpdateInformationOnServer(): httpMethod = %d", httpMethod));

                if (tempSentBTList.size() != tempCurrentBTList.size()) {
                    hasChanged = true;
                } else {
                    if (!tempCurrentBTList.empty()) {

                        while (!tempCurrentBTList.empty() && !tempSentBTList.empty()) {
                            if (tempCurrentBTList.front() != tempSentBTList.front()) {
                                hasChanged = true;
                                break;
                            }
                            tempCurrentBTList.pop_front();
                            tempSentBTList.pop_front();
                        }
                    }
                }
            }
#endif
        }

        RendezvousMessage message;

        if (hasChanged) {
            if (messageType == ADVERTISEMENT) {
                ComposeAdvertisementorSearch(true, httpMethod, message);
            } else if (messageType == SEARCH) {
                ComposeAdvertisementorSearch(false, httpMethod, message);
            } else if (messageType == PROXIMITY) {
                ComposeProximityMessage(httpMethod, message);
            }

            if (message.messageType != INVALID_MESSAGE) {
                status = SendMessage(message);

                if (status == ER_OK) {
                    QCC_DbgPrintf(("DiscoveryManager::UpdateInformationOnServer(): Successfully sent the message to the Server"));
                } else {
                    status = ER_UNABLE_TO_SEND_MESSAGE_TO_RENDEZVOUS_SERVER;
                    QCC_LogError(status, ("DiscoveryManager::UpdateInformationOnServer(): %s", QCC_StatusText(status)));
                }
            }
        }
    }

    return status;
}

QStatus DiscoveryManager::HandleOnDemandMessageResponse(Json::Value payload)
{
    QStatus status = ER_OK;

    GenericResponse response;

    status = ParseGenericResponse(payload, response);

    if (status != ER_OK) {
        status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
        QCC_LogError(status, ("DiscoveryManager::HandleOnDemandMessageResponse(): ParseGenericResponse failed"));
    } else {
        /* Verify that the peerID in the received response is the one assigned to this daemon */
        if (response.peerID != PeerID) {
            status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
            QCC_LogError(status, ("DiscoveryManager::HandleOnDemandMessageResponse(): PeerID(%s) in the received response does not match with the one assigned to this daemon(%s)", response.peerID.c_str(), PeerID.c_str()));
        } else {
            switch (LastOnDemandMessageSent.messageType) {
            case ADVERTISEMENT:
                // Update the last sent advertisement list with the contents of the temp sent advertisement list
                lastSentAdvertiseList.clear();
                lastSentAdvertiseList = tempSentAdvertiseList;
                QCC_DbgPrintf(("DiscoveryManager::HandleOnDemandMessageResponse(): Updated lastSentAdvertiseList with contents of tempSentAdvertiseList"));
                break;

            case SEARCH:
                // Update the last sent search list with the contents of the temp sent search list
                lastSentSearchList.clear();
                lastSentSearchList = tempSentSearchList;
                QCC_DbgPrintf(("DiscoveryManager::HandleOnDemandMessageResponse(): Updated lastSentSearchList with contents of tempSentSearchList"));
                break;

            case PROXIMITY:
                lastSentBSSIDList.clear();
                lastSentBSSIDList = tempSentBSSIDList;
                lastSentBTMACList.clear();
                lastSentBTMACList = tempSentBTMACList;
                QCC_DbgPrintf(("DiscoveryManager::HandleOnDemandMessageResponse(): Updated last sent proximity lists with the contents of the temp sent proximity lists"));
                break;

            case GET_MESSAGE:
            case CLIENT_LOGIN:
            case TOKEN_REFRESH:
                status = ER_FAIL;
                QCC_LogError(status, ("DiscoveryManager::HandleOnDemandMessageResponse(): Cannot handle response for %s message in this function", PrintMessageType(LastOnDemandMessageSent.messageType).c_str()));
                break;

            case RENDEZVOUS_SESSION_DELETE:
            case DAEMON_REGISTRATION:
            case ADDRESS_CANDIDATES:
                // Nothing to be done
                QCC_DbgPrintf(("DiscoveryManager::HandleOnDemandMessageResponse(): Nothing to be done"));
                break;

            case INVALID_MESSAGE:
            default:
                status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
                QCC_LogError(status, ("DiscoveryManager::HandleOnDemandMessageResponse(): %s", QCC_StatusText(status)));
                break;
            }
        }
    }

    return status;
}

void DiscoveryManager::HandleOnDemandConnectionResponse(HttpConnection::HTTPResponse& response)
{
    QCC_DbgPrintf(("DiscoveryManager::HandleOnDemandConnectionResponse()"));

    QStatus status;

    /* Check the status code in the response */
    if (response.statusCode == HttpConnection::HTTP_STATUS_OK) {

        /* Handle the response */
        if (response.payloadPresent) {

            /* If the sent message was the the Client Login message, handle it accordingly */
            if (LastOnDemandMessageSent.messageType == CLIENT_LOGIN) {
                status = HandleClientLoginResponse(response.payload);

                if (status != ER_OK) {
                    Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                    if (ProximityScanner) {
                        /* Stop the proximity scan before start to rule out any race conditions */
                        ProximityScanner->StopScan();
                    }
#endif

                    ForceInterfaceUpdateFlag = true;
                }
            } else if (LastOnDemandMessageSent.messageType == TOKEN_REFRESH) {
                status = HandleTokenRefreshResponse(response.payload);

                if (status != ER_OK) {
                    Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                    if (ProximityScanner) {
                        /* Stop the proximity scan before start to rule out any race conditions */
                        ProximityScanner->StopScan();
                    }
#endif

                    ForceInterfaceUpdateFlag = true;
                }
            } else {

                status = HandleOnDemandMessageResponse(response.payload);

                if (status != ER_OK) {
                    Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                    if (ProximityScanner) {
                        /* Stop the proximity scan before start to rule out any race conditions */
                        ProximityScanner->StopScan();
                    }
#endif

                    ForceInterfaceUpdateFlag = true;
                }

            }

        } else {

            /* We can receive a 200 OK with no payload on the On Demand connection only if the sent request was a
             * DELETE request */
            if (LastOnDemandMessageSent.httpMethod != HttpConnection::METHOD_DELETE) {

                status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
                QCC_LogError(status, ("DiscoveryManager::HandleOnDemandConnectionResponse(): Response with no payload received for a message that was not a DELETE request"));

                /* All HTTP_STATUS_OK responses over the On Demand connection must have a payload. If we get a HTTP_STATUS_OK response
                 * without a payload, there is an issue. So we re-setup the connection */
                Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
                if (ProximityScanner) {
                    /* Stop the proximity scan before start to rule out any race conditions */
                    ProximityScanner->StopScan();
                }
#endif

                ForceInterfaceUpdateFlag = true;
            }
        }

    } else if (response.statusCode == HttpConnection::HTTP_UNAUTHORIZED_REQUEST) {

        status = ER_RENDEZVOUS_SERVER_ERR401_UNAUTHORIZED_REQUEST;
        QCC_LogError(status, ("DiscoveryManager::HandleOnDemandConnectionResponse(): %s", QCC_StatusText(status)));

        if (!ClientAuthenticationRequiredFlag) {
            /* Disconnect */
            Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
            if (ProximityScanner) {
                /* Stop the proximity scan before start to rule out any race conditions */
                ProximityScanner->StopScan();
            }
#endif

            /* We need to re-authenticate with the Server */
            ClientAuthenticationRequiredFlag = true;

            if (InterfaceUpdateAlarm) {
                DiscoveryManagerTimer.RemoveAlarm(*InterfaceUpdateAlarm);
                delete InterfaceUpdateAlarm;
                InterfaceUpdateAlarm = NULL;
            }

            InterfaceUpdateAlarm = new Alarm(INTERFACE_UPDATE_MIN_INTERVAL, this, 0, NULL);
            status = DiscoveryManagerTimer.AddAlarm(*InterfaceUpdateAlarm);
        }

    } else {

        status = ER_RENDEZVOUS_SERVER_UNRECOVERABLE_ERROR;
        QCC_LogError(status, ("DiscoveryManager::HandleOnDemandConnectionResponse(): %s", QCC_StatusText(status)));

        /* If any other http status code is received, we just disconnect and reconnect after INTERFACE_UPDATE_MIN_INTERVAL */
        Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
        if (ProximityScanner) {
            /* Stop the proximity scan before start to rule out any race conditions */
            ProximityScanner->StopScan();
        }
#endif

        if (InterfaceUpdateAlarm) {
            DiscoveryManagerTimer.RemoveAlarm(*InterfaceUpdateAlarm);
            delete InterfaceUpdateAlarm;
            InterfaceUpdateAlarm = NULL;
        }

        InterfaceUpdateAlarm = new Alarm(INTERFACE_UPDATE_MIN_INTERVAL, this, 0, NULL);
        status = DiscoveryManagerTimer.AddAlarm(*InterfaceUpdateAlarm);

        if (status != ER_OK) {
            /* We do not take any action if we are not able to add the alarm to the timer as if some issue comes up with the
             * connection we handle it resetting up the connection */
            QCC_LogError(status, ("DiscoveryManager::HandleOnDemandConnectionResponse(): Unable to add InterfaceUpdateAlarm to DiscoveryManagerTimer"));
        }
    }

    /* Reset SentMessageOverOnDemandConnection to indicate that we received a response */
    SentMessageOverOnDemandConnection = false;
}

QStatus DiscoveryManager::SendClientLoginFirstRequest(void)
{
    QCC_DbgPrintf(("DiscoveryManager::SendClientLoginFirstRequest()"));

    RendezvousMessage message;

    message.httpMethod = HttpConnection::METHOD_POST;
    message.messageType = CLIENT_LOGIN;

    ClientLoginRequest* loginRequest = new ClientLoginRequest();

    loginRequest->firstMessage = true;
    loginRequest->daemonID = PersistentIdentifier;
    loginRequest->mechanism = SCRAM_SHA_1_MECHANISM;

    /* Reset the SCRAMAuthModule to clear the obsolete values */
    SCRAMAuthModule.Reset();

    /* Get the user credentials from the Client Login Interface */
    GetUserCredentials();

    /* Set the user credentials in the SCRAM module */
    SCRAMAuthModule.SetUserCredentials(userCredentials.userName, userCredentials.userPassword);

    loginRequest->message = SCRAMAuthModule.GenerateClientLoginFirstSASLMessage();

    message.interfaceMessage = static_cast<InterfaceMessage*>(loginRequest);

    /* Send the message to the Server */
    QStatus status = SendMessage(message);

    if (status == ER_OK) {
        QCC_DbgPrintf(("DiscoveryManager::SendClientLoginFirstRequest(): Successfully sent the Client Registration First Message to the Server"));
    } else {
        status = ER_UNABLE_TO_SEND_MESSAGE_TO_RENDEZVOUS_SERVER;
        QCC_LogError(status, ("DiscoveryManager::SendClientLoginFirstRequest(): Unable to send the Client Registration First Message to the Server"));
    }

    return status;
}

QStatus DiscoveryManager::SendClientLoginFinalRequest(void)
{
    QCC_DbgPrintf(("DiscoveryManager::SendClientLoginFinalRequest()"));

    RendezvousMessage message;

    message.httpMethod = HttpConnection::METHOD_POST;
    message.messageType = CLIENT_LOGIN;

    ClientLoginRequest* loginRequest = new ClientLoginRequest();

    loginRequest->firstMessage = false;
    loginRequest->daemonID = PersistentIdentifier;
    if (PeerID.empty()) {
        loginRequest->clearClientState = true;
    }
    loginRequest->mechanism = SCRAM_SHA_1_MECHANISM;

    loginRequest->message = SCRAMAuthModule.GenerateClientLoginFinalSASLMessage();

    message.interfaceMessage = static_cast<InterfaceMessage*>(loginRequest);

    /* Send the message to the Server */
    QStatus status = SendMessage(message);

    if (status == ER_OK) {
        QCC_DbgPrintf(("DiscoveryManager::SendClientLoginFirstRequest(): Successfully sent the Client Registration Final Message to the Server"));
    } else {
        status = ER_UNABLE_TO_SEND_MESSAGE_TO_RENDEZVOUS_SERVER;
        QCC_LogError(status, ("DiscoveryManager::SendClientLoginFirstRequest(): Unable to send the Client Registration Final Message to the Server"));
    }

    return status;
}

void DiscoveryManager::HandleUnsuccessfulClientAuthentication(SASLError error)
{
    QCC_DbgPrintf(("DiscoveryManager::HandleUnsuccessfulClientAuthentication(): error = %d", error));

    if ((error == DEACTIVATED_USER) || (error == UNKNOWN_USER)) {
        QStatus status = ER_RENDEZVOUS_SERVER_UNKNOWN_USER;

        if (error == DEACTIVATED_USER) {
            status = ER_RENDEZVOUS_SERVER_DEACTIVATED_USER;
        }
        // Tell user to set up an account with the server
        QCC_LogError(status, ("DiscoveryManager::HandleUnsuccessfulClientAuthentication(): %s", QCC_StatusText(status)));

        /* Set the ClientAuthenticationFailed so that we don't attempt a reconnect unless the Advertise/Search
         * list has changed */
        ClientAuthenticationFailed = true;
    }

    // Disconnect from rendezvous
    Disconnect();

#ifdef ENABLE_PROXIMITY_FRAMEWORK
    if (ProximityScanner) {
        /* Stop the proximity scan before start to rule out any race conditions */
        ProximityScanner->StopScan();
    }
#endif
}

QStatus DiscoveryManager::HandleUpdatesToServer(void)
{
    QCC_DbgPrintf(("DiscoveryManager::HandleUpdatesToServer(): LastSentUpdateMessage(%s) RendezvousSessionActiveFlag(%d)",
                   PrintMessageType(LastSentUpdateMessage).c_str(), RendezvousSessionActiveFlag));

    MessageType currentMessageType = INVALID_MESSAGE;
    QStatus status = ER_OK;

    if (LastSentUpdateMessage == INVALID_MESSAGE) {
        currentMessageType = ADVERTISEMENT;
    } else if (LastSentUpdateMessage == ADVERTISEMENT) {
        currentMessageType = SEARCH;
    } else if (LastSentUpdateMessage == SEARCH) {
        currentMessageType = PROXIMITY;
    } else {
        status = ER_FAIL;
        currentMessageType = INVALID_MESSAGE;
        QCC_LogError(status, ("DiscoveryManager::HandleUpdatesToServer(): Cannot handle messageType(%s) in this function",
                              PrintMessageType(LastSentUpdateMessage).c_str()));
    }

    if (currentMessageType != INVALID_MESSAGE) {
        status = UpdateInformationOnServer(currentMessageType, RendezvousSessionActiveFlag);
    }

    if (status == ER_OK) {
        LastSentUpdateMessage = currentMessageType;
    }

    return status;
}

void DiscoveryManager::HandleSuccessfulClientAuthentication(ClientLoginFinalResponse response)
{
    QCC_DbgPrintf(("DiscoveryManager::HandleSuccessfulClientAuthentication()"));

    /* Set the PeerID and PeerAddr */
    PeerID = response.peerID;
    PeerAddr = response.peerAddr;

    if (response.daemonRegistrationRequired) {
        /* Set the RegisterDaemonWithServer flag so that the DiscoveryManager thread may
         * send the Daemon Registration Message to the Server*/
        RegisterDaemonWithServer = true;
    }

    /* Set the UpdateInformationOnServerFlag flag so that the DiscoveryManager thread may
     * update the information on the Server as per the RendezvousSessionActiveFlag */
    RendezvousSessionActiveFlag = response.sessionActive;
    UpdateInformationOnServerFlag = true;
    LastSentUpdateMessage = INVALID_MESSAGE;

    /* Set the TKeepAlive to the value sent by the Server */
    SetTKeepAlive(response.configData.Tkeepalive);
}

QStatus DiscoveryManager::HandleClientLoginResponse(Json::Value payload)
{
    QStatus status = ER_OK;

    QCC_DbgPrintf(("DiscoveryManager::HandleClientLoginResponse()"));

    if (LastOnDemandMessageSent.messageType != CLIENT_LOGIN) {
        status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
        QCC_LogError(status, ("DiscoveryManager::HandleClientLoginResponse(): Sent message was not a client login request"));
    } else {
        ClientLoginRequest* loginRequest = static_cast<ClientLoginRequest*>(LastOnDemandMessageSent.interfaceMessage);

        QCC_DbgPrintf(("DiscoveryManager::HandleClientLoginResponse(): firstMessage = %d", loginRequest->firstMessage));

        /* Depending on whether the sent request was the initial request or the final request, handle the response
         * accordingly */
        if (loginRequest->firstMessage) {
            ClientLoginFirstResponse response;
            status = ParseClientLoginFirstResponse(payload, response);

            if (status == ER_OK) {
                status = SCRAMAuthModule.ValidateClientLoginFirstResponse(response.message);

                if (status == ER_OK) {
                    if (SCRAMAuthModule.IsErrorPresentInServerFirstResponse()) {
                        HandleUnsuccessfulClientAuthentication(SCRAMAuthModule.GetErrorInServerFirstResponse());
                    } else {
                        /* Send the client login final message */
                        SendClientLoginFinalRequest();
                    }
                } else {
                    status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
                    QCC_LogError(status, ("DiscoveryManager::HandleClientLoginResponse(): ValidateClientLoginFirstResponse() failed"));
                }
            } else {
                status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
                QCC_LogError(status, ("DiscoveryManager::HandleClientLoginResponse(): ParseClientLoginFirstResponse failed"));
            }
        } else {
            ClientLoginFinalResponse response;
            status = ParseClientLoginFinalResponse(payload, response);

            if (status == ER_OK) {
                status = SCRAMAuthModule.ValidateClientLoginFinalResponse(response);

                if (status == ER_OK) {
                    if (SCRAMAuthModule.IsErrorPresentInServerFinalResponse()) {
                        HandleUnsuccessfulClientAuthentication(SCRAMAuthModule.GetErrorInServerFinalResponse());
                    } else {
                        HandleSuccessfulClientAuthentication(response);
                        /* Clear the ClientAuthenticationRequiredFlag if we could send the client login first message successfully to
                         * the Server */
                        ClientAuthenticationRequiredFlag = false;
                    }
                } else {
                    status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
                    QCC_LogError(status, ("DiscoveryManager::HandleClientLoginResponse(): ValidateClientLoginFirstResponse() failed"));
                }
            } else {
                status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
                QCC_LogError(status, ("DiscoveryManager::HandleClientLoginResponse(): ParseClientLoginFirstResponse failed"));
            }
        }
    }

    return status;
}

QStatus DiscoveryManager::HandleTokenRefreshResponse(Json::Value payload)
{
    QStatus status = ER_OK;

    QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse()"));

    if (LastOnDemandMessageSent.messageType != TOKEN_REFRESH) {
        status = ER_INVALID_ON_DEMAND_CONNECTION_MESSAGE_RESPONSE;
        QCC_LogError(status, ("DiscoveryManager::HandleTokenRefreshResponse(): Sent message was not a token refresh message"));
    } else {

        TokenRefreshResponse response;
        status = ParseTokenRefreshResponse(payload, response);

        if (status == ER_OK) {
            TokenRefreshMessage* refreshMsg = static_cast<TokenRefreshMessage*>(LastOnDemandMessageSent.interfaceMessage);

            QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse(): client = %d", refreshMsg->client));

            if (refreshMsg->client) {
                QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse(): Trying to invoke the Token Refresh callback for service on Daemon with GUID %s\n",
                               refreshMsg->remotePeerAddress.c_str()));

                multimap<String, SearchResponseInfo>::iterator it;

                DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);

                map<String, RemoteDaemonStunInfo>::iterator stun_it = StunAndTurnServerInfo.find(refreshMsg->remotePeerAddress);
                if (stun_it != StunAndTurnServerInfo.end()) {

                    // We found the entry
                    stun_it->second.stunInfo.acct = response.acct;
                    stun_it->second.stunInfo.pwd = response.pwd;
                    stun_it->second.stunInfo.expiryTime = response.expiryTime;
                    stun_it->second.stunInfo.recvTime = response.recvTime;

                    refreshMsg->tokenRefreshListener->SetTokens(response.acct, response.pwd, response.recvTime, response.expiryTime);

                    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                    QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse(): Invoked the token refresh callback\n"));
                    return ER_OK;
                }

                DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

                QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse(): Did not find an entry corresponding to the GUID %s\n", refreshMsg->remotePeerAddress.c_str()));

                return ER_FAIL;
            } else {

                QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse(): Trying to retrieve the STUN server info for client on Daemon with GUID %s\n",
                               refreshMsg->remotePeerAddress.c_str()));

                multimap<String, SessionEntry>::iterator it;

                DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
                for (it = IncomingICESessions.begin(); it != IncomingICESessions.end(); it++) {
                    if (((it->first) == refreshMsg->remotePeerAddress) && ((it->second).STUNInfoPresent)) {

                        (it->second).STUNInfo.acct = response.acct;
                        (it->second).STUNInfo.pwd = response.pwd;
                        (it->second).STUNInfo.expiryTime = response.expiryTime;
                        (it->second).STUNInfo.recvTime = response.recvTime;

                        refreshMsg->tokenRefreshListener->SetTokens(response.acct, response.pwd, response.recvTime, response.expiryTime);

                        DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
                        QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse(): Invoked the token refresh callback\n"));
                        return ER_OK;
                    }
                }

                DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);

                QCC_DbgPrintf(("DiscoveryManager::HandleTokenRefreshResponse(): Did not find an entry corresponding to the matchID\n"));

                return ER_FAIL;
            }
        } else {
            status = ER_FAIL;
            QCC_LogError(status, ("DiscoveryManager::HandleTokenRefreshResponse(): Unable to parse the token refresh response successfully"));
        }
    }

    return status;
}

QStatus DiscoveryManager::PrepareOutgoingMessage(RendezvousMessage message, HttpConnection::Method& httpMethod, String& uri, bool& contentPresent, String& content)
{
    QStatus status = ER_OK;

    QCC_DbgPrintf(("DiscoveryManager::PrepareOutgoingMessage(): messageType(%s)", PrintMessageType(message.messageType).c_str()));

    httpMethod = message.httpMethod;

    if (message.messageType == ADVERTISEMENT) {
        uri = GetAdvertisementUri(PeerID);

        if (httpMethod != HttpConnection::METHOD_DELETE) {
            AdvertiseMessage* advertise = static_cast<AdvertiseMessage*>(message.interfaceMessage);
            content = GenerateJSONAdvertisement(*advertise);
            contentPresent = true;
        }
    } else if (message.messageType == SEARCH) {
        uri = GetSearchUri(PeerID);

        if (httpMethod != HttpConnection::METHOD_DELETE) {
            SearchMessage* search = static_cast<SearchMessage*>(message.interfaceMessage);
            content = GenerateJSONSearch(*search);
            contentPresent = true;
        }
    } else if (message.messageType == PROXIMITY) {

        /* Proximity is sent only using POST or PUT HTTP method */
        if (httpMethod != HttpConnection::METHOD_DELETE) {
            uri = GetProximityUri(PeerID);
            ProximityMessage* proximityMsg = static_cast<ProximityMessage*>(message.interfaceMessage);
            content = GenerateJSONProximity(*proximityMsg);
            contentPresent = true;
        } else {
            status = ER_INVALID_HTTP_METHOD_USED_FOR_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
            QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): DELETE HTTP Method cannot be used for "
                                  "sending proximity message"));
            return status;
        }
    } else if (message.messageType == ADDRESS_CANDIDATES) {

        /* Address Candidates is sent only using POST HTTP method */
        if (httpMethod == HttpConnection::METHOD_POST) {
            ICECandidatesMessage* adressCandMsg = static_cast<ICECandidatesMessage*>(message.interfaceMessage);
            uri = GetAddressCandidatesUri(PeerID, adressCandMsg->destinationPeerID, adressCandMsg->requestToAddSTUNInfo);
            content = GenerateJSONCandidates(*adressCandMsg);
            contentPresent = true;
        } else {
            status = ER_INVALID_HTTP_METHOD_USED_FOR_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
            QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): HTTP Methods other than POST cannot be used for "
                                  "sending address candidates message"));
            return status;
        }
    } else if (message.messageType == RENDEZVOUS_SESSION_DELETE) {
        /* Rendezvous Session Delete is sent only using DELETE HTTP method */
        if (httpMethod == HttpConnection::METHOD_DELETE) {
            uri = GetRendezvousSessionDeleteUri(PeerID);
        } else {
            status = ER_INVALID_HTTP_METHOD_USED_FOR_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
            QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): HTTP Methods other than DELETE cannot be used for "
                                  "sending Rendezvous Session Delete message"));
            return status;
        }
    } else if (message.messageType == GET_MESSAGE) {
        /* GET Messages is sent only using GET HTTP method */
        if (httpMethod == HttpConnection::METHOD_GET) {
            uri = GetGETUri(PeerID);
        } else {
            status = ER_INVALID_HTTP_METHOD_USED_FOR_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
            QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): HTTP Methods other than GET cannot be used for "
                                  "sending GET Messages"));
            return status;
        }
    } else if (message.messageType == CLIENT_LOGIN) {
        /* Client Login is sent only using POST HTTP method */
        if (httpMethod == HttpConnection::METHOD_POST) {
            uri = GetClientLoginUri();
            ClientLoginRequest* loginMsg = static_cast<ClientLoginRequest*>(message.interfaceMessage);
            content = GenerateJSONClientLoginRequest(*loginMsg);
            contentPresent = true;
        } else {
            status = ER_INVALID_HTTP_METHOD_USED_FOR_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
            QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): HTTP Methods other than POST cannot be used for "
                                  "sending client login request"));
            return status;
        }
    } else if (message.messageType == DAEMON_REGISTRATION) {
        /* Daemon Registration is sent only using POST HTTP method */
        if (httpMethod == HttpConnection::METHOD_POST) {
            uri = GetDaemonRegistrationUri(PeerID);
            DaemonRegistrationMessage* regMsg = static_cast<DaemonRegistrationMessage*>(message.interfaceMessage);
            content = GenerateJSONDaemonRegistrationMessage(*regMsg);
            contentPresent = true;
        } else {
            status = ER_INVALID_HTTP_METHOD_USED_FOR_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
            QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): HTTP Methods other than POST cannot be used for "
                                  "sending Daemon Registration message"));
            return status;
        }
    } else if (message.messageType == TOKEN_REFRESH) {
        /* Token Refresh Message is sent only using GET HTTP method */
        if (httpMethod == HttpConnection::METHOD_GET) {
            uri = GetTokenRefreshUri(PeerID);
        } else {
            status = ER_INVALID_HTTP_METHOD_USED_FOR_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
            QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): HTTP Methods other than GET cannot be used for "
                                  "sending Token Refresh message"));
            return status;
        }
    } else {
        status = ER_INVALID_RENDEZVOUS_SERVER_INTERFACE_MESSAGE;
        QCC_LogError(status, ("DiscoveryManager::PrepareOutgoingMessage(): %s", QCC_StatusText(status)));
        return status;
    }

    QCC_DbgPrintf(("DiscoveryManager::PrepareOutgoingMessage(): uri(%s)", uri.c_str()));

    return status;
}

void DiscoveryManager::SetTKeepAlive(uint32_t tsecs)
{
    QCC_DbgPrintf(("DiscoveryManager::SetTKeepAlive(): tsecs = %d", tsecs));

    /* If tsecs is less than T_KEEP_ALIVE_MIN_IN_SECS, set it to T_KEEP_ALIVE_MIN_IN_SECS */
    if (tsecs < T_KEEP_ALIVE_MIN_IN_SECS) {
        tsecs = T_KEEP_ALIVE_MIN_IN_SECS;
    }

    T_KEEP_ALIVE_IN_MS = tsecs * T_KEEP_ALIVE_BUFFER_MULTIPLE * MS_IN_A_SECOND;

    QCC_DbgPrintf(("DiscoveryManager::SetTKeepAlive(): T_KEEP_ALIVE_IN_MS = %d", T_KEEP_ALIVE_IN_MS));
}

QStatus DiscoveryManager::SendDaemonRegistrationMessage(void)
{
    QCC_DbgPrintf(("DiscoveryManager::SendDaemonRegistrationMessage()"));

    /* Construct the Daemon Registration Message */
    RendezvousMessage message;

    message.httpMethod = HttpConnection::METHOD_POST;
    message.messageType = DAEMON_REGISTRATION;

    DaemonRegistrationMessage* regMsg = new DaemonRegistrationMessage();

    regMsg->daemonID = PersistentIdentifier;
    regMsg->daemonVersion = String(GetVersion());

    // PPN - Populate later
    regMsg->devMake = String();
    regMsg->devModel = String();
    regMsg->osVersion = String();

    regMsg->osType = qcc::GetSystemOSType();

    message.interfaceMessage = static_cast<InterfaceMessage*>(regMsg);

    /* Send the message to the Server */
    QStatus status = SendMessage(message);

    if (status == ER_OK) {
        QCC_DbgPrintf(("DiscoveryManager::SendDaemonRegistrationMessage(): Successfully sent the Daemon Registration Message to the Server"));
    } else {
        status = ER_UNABLE_TO_SEND_MESSAGE_TO_RENDEZVOUS_SERVER;
        QCC_LogError(status, ("DiscoveryManager::SendDaemonRegistrationMessage(): Unable to send the Daemon Registration Message to the Server"));
    }

    return status;
}

uint32_t DiscoveryManager::GetWaitTimeOut(void)
{
    QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut()"));

    uint32_t timeout = Event::WAIT_FOREVER;
    uint32_t tNow = GetTimestamp();
    bool setTimeout = false;

    QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): timeout= 0x%x tNow = 0x%x", timeout, tNow));

    if (PersistentMessageSentTimeStamp) {
        QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): PersistentMessageSentTimeStamp"));
        if ((GetTKeepAlive() + PersistentMessageSentTimeStamp) <= tNow) {
            QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): GetTKeepAlive() = 0x%x PersistentMessageSentTimeStamp = 0x%x",
                           GetTKeepAlive(), PersistentMessageSentTimeStamp));
            timeout = 0;
        } else {
            timeout = (GetTKeepAlive() + PersistentMessageSentTimeStamp) - tNow;
            QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): timeout = 0x%x", timeout));
            setTimeout = true;
        }
    }

    if (!setTimeout) {
        if (SentMessageOverOnDemandConnection) {
            QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): SentMessageOverOnDemandConnection"));
            if (OnDemandMessageSentTimeStamp) {
                QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): OnDemandMessageSentTimeStamp"));
                if ((GetTKeepAlive() + OnDemandMessageSentTimeStamp) <= tNow) {
                    QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): GetTKeepAlive() = 0x%x OnDemandMessageSentTimeStamp = 0x%x",
                                   GetTKeepAlive(), OnDemandMessageSentTimeStamp));
                    timeout = 0;
                } else {
                    timeout = (GetTKeepAlive() + OnDemandMessageSentTimeStamp) - tNow;
                    QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): timeout = 0x%x", timeout));
                }
            }
        }
    }

    QCC_DbgPrintf(("DiscoveryManager::GetWaitTimeOut(): timeout = %d", timeout));

    return timeout;
}

void DiscoveryManager::AlarmTriggered(const qcc::Alarm& alarm, QStatus status)
{
    QCC_DbgPrintf(("DiscoveryManager::AlarmTriggered()"));

    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
    /* Set the ForceInterfaceUpdateFlag to update the interfaces and set the wake event to wake the DiscoveryManager thread */
    ForceInterfaceUpdateFlag = true;
    WakeEvent.SetEvent();
    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
}

class ClientLoginBusListener : public BusListener, public SessionListener {
  public:
    ClientLoginBusListener() : BusListener(), sessionId(0) { }

    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        QCC_DbgPrintf(("ClientLoginBusListener::FoundAdvertisedName(): Found the service"));
    }

    SessionId GetSessionId() const { return sessionId; }

  private:
    SessionId sessionId;
};

void DiscoveryManager::GetUserCredentials(void)
{
    QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials()"));

    String userName = String("");
    String password = String(" ");

    QStatus status;

    /* Read the user credentials from the Client Login Interface */
    ClientLoginBusListener* clientLoginBusListener;

    clientLoginBusListener = new ClientLoginBusListener();
    bus.RegisterBusListener(*clientLoginBusListener);

    bool hasOwer;

    while (true) {
        status = bus.NameHasOwner(ClientLoginServiceName.c_str(), hasOwer);
        if (ER_OK != status) {
            QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): NameHasOwner failed"));
        }
        if (hasOwer) {
            QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): Successfully connected to %s", ClientLoginServiceName.c_str()));
            break;
        } else {
            QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): No %s owner found yet", ClientLoginServiceName.c_str()));
            return;
        }
    }

    ProxyBusObject* remoteObj = new ProxyBusObject(bus, ClientLoginServiceName.c_str(), ClientLoginServiceObject.c_str(), 0);

    status = remoteObj->IntrospectRemoteObject();
    if (ER_OK != status) {
        QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): Problem introspecting the remote object %s", ClientLoginServiceObject.c_str()));
    } else {
        QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): Introspection on the remote object %s successful", ClientLoginServiceObject.c_str()));
    }

    // Call the remote method GetClientAccountName on the service
    Message userNameReply(bus);
    status = remoteObj->MethodCall(ClientLoginServiceName.c_str(), GetAccountNameMethod.c_str(), NULL, 0, userNameReply, 35000);
    if (ER_OK != status) {
        QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): Issue calling method %s on the remote object", GetAccountNameMethod.c_str()));
        qcc::String errorMsg;
        userNameReply->GetErrorName(&errorMsg);
        QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): Call to %s returned error message : %s", GetAccountNameMethod.c_str(), errorMsg.c_str()));
        return;
    } else {
        QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): Method call %s was successful", GetAccountNameMethod.c_str()));
    }

    MsgArg* userNameArg;
    size_t userNameArgSize;
    const MsgArg* userNameArgs = userNameReply->GetArg(0);
    status = userNameArgs->Get("s", &userNameArgSize, &userNameArg);
    if (ER_OK != status) {
        QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): Error while unmarshalling the string received from the service %s", ClientLoginServiceName.c_str()));
    } else {
        status = userNameArg->Get("s", &userName);
        if (ER_OK != status) {
            QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): Error while getting the value for expected signature = %s", userNameArg->Signature().c_str()));
        } else {
            QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): userName = %s", userName.c_str()));
        }
    }

    // Call the remote method GetClientAccountPassword on the service
    Message passwordReply(bus);
    status = remoteObj->MethodCall(ClientLoginServiceName.c_str(), GetAccountPasswordMethod.c_str(), NULL, 0, passwordReply, 35000);
    if (ER_OK != status) {
        QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): Issue calling method %s on the remote object", GetAccountNameMethod.c_str()));
        qcc::String errorMsg;
        passwordReply->GetErrorName(&errorMsg);
        QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): Call to %s returned error message : %s", GetAccountPasswordMethod.c_str(), errorMsg.c_str()));
        return;
    } else {
        QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): Method call %s was successful", GetAccountPasswordMethod.c_str()));
    }

    MsgArg* passwordArg;
    size_t passwordArgSize;
    const MsgArg* passwordArgs = passwordReply->GetArg(0);
    status = passwordArgs->Get("s", &passwordArgSize, &passwordArg);
    if (ER_OK != status) {
        QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): Error while unmarshalling the string received from the service %s", ClientLoginServiceName.c_str()));
    } else {
        status = passwordArg->Get("s", &password);
        if (ER_OK != status) {
            QCC_LogError(status, ("DiscoveryManager::GetUserCredentials(): Error while getting the value for expected signature = %s", passwordArg->Signature().c_str()));
        } else {
            QCC_DbgPrintf(("DiscoveryManager::GetUserCredentials(): password = %s", password.c_str()));
        }
    }

    userCredentials.SetCredentials(userName, password);
}

void DiscoveryManager::ComposeAndQueueTokenRefreshMessage(TokenRefreshMessage refreshMessage)
{
    QCC_DbgPrintf(("DiscoveryManager::ComposeAndQueueTokenRefreshMessage()"));

    /* Construct the Daemon Registration Message */
    RendezvousMessage message;

    message.httpMethod = HttpConnection::METHOD_GET;
    message.messageType = TOKEN_REFRESH;

    TokenRefreshMessage* refMsg = new TokenRefreshMessage();
    *refMsg = refreshMessage;

    message.interfaceMessage = static_cast<InterfaceMessage*>(refMsg);

    DiscoveryManagerMutex.Lock(MUTEX_CONTEXT);
    QueueMessage(message);
    DiscoveryManagerMutex.Unlock(MUTEX_CONTEXT);
}

QStatus DiscoveryManager::Stop(void)
{

    QCC_DbgHLPrintf(("DiscoveryManager::Stop()"));

    /*
     * Tell the Run() thread to shut down through the thread
     * base class.
     */
    QStatus status = Thread::Stop();
    if (status != ER_OK) {
        QCC_LogError(status, ("DiscoveryManager::Stop(): Failed to Stop() Run() thread"));
        return status;
    }

    return ER_OK;
}

QStatus DiscoveryManager::Join(void)
{
    QCC_DbgHLPrintf(("DiscoveryManager::Join()"));
    /*
     * Wait for the Run() thread to exit.
     */
    QStatus status = Thread::Join();
    if (status != ER_OK) {
        QCC_LogError(status, ("DiscoveryManager::Join(): Failed to Join() Run() thread"));
        return status;
    }

    return ER_OK;
}

void DiscoveryManager::GetRendezvousConnIPAddresses(IPAddress& onDemandAddress, IPAddress& persistentAddress)
{
    if (Connection) {
        QCC_DbgPrintf(("DiscoveryManager::GetRendezvousConnIPAddresses(): Connected to the Server"));
        Connection->GetRendezvousConnIPAddresses(onDemandAddress, persistentAddress);
    } else {
        QCC_DbgPrintf(("DiscoveryManager::GetRendezvousConnIPAddresses(): Not connected to the Server"));
    }
}

} // namespace ajn
