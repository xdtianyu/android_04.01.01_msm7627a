/**
 * @file
 * This file defines the Rendezvous Server Interface Messages and Responses.
 * It also includes some worker functions that help in the generation and
 * parsing of JSON format interface messages.
 */

/******************************************************************************
 * Copyright 2012, Qualcomm Innovation Center, Inc.
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

#ifndef RENDEZVOUSSERVERINTERFACE_H_
#define RENDEZVOUSSERVERINTERFACE_H_

#ifndef __cplusplus
#error Only include RendezvousServerInterface.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/GUID.h>
#include <qcc/IPAddress.h>
#include <qcc/StringUtil.h>
#include <JSON/json.h>
#include <qcc/Util.h>
#include <qcc/Timer.h>
#include <list>

#include "HttpConnection.h"
#include "TokenRefreshListener.h"

using namespace qcc;
using namespace std;

namespace ajn {

/**
 * The Rendezvous interface protocol version.
 */
const String RendezvousProtocolVersion = String("v1");

/**
 * The Rendezvous interface protocol version.
 */
const String RendezvousServerAddress = String("/rdv/");

/**
 * The Advertisement URI.
 */
const String AdvertisementUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/advertisement");

/**
 * The Search URI.
 */
const String SearchUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/search");

/**
 * The Proximity URI.
 */
const String ProximityUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/proximity");

/**
 * The Address Candidates URI without the request to add STUN information.
 */
const String AddressCandidatesUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/candidates/dest/%s");

/**
 * The Address Candidates URI with the request to add STUN information.
 */
const String AddressCandidatesWithSTUNUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/candidates/dest/%s/addSTUN");

/**
 * The Rendezvous Session Delete URI.
 */
const String RendezvousSessionDeleteUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s");

/**
 * The GET messages URI.
 */
const String GETUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/messages");

/**
 * The Client Login URI.
 */
const String ClientLoginUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/login");

/**
 * The Daemon registration URI.
 */
const String DaemonRegistrationUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/daemon-reg");

/**
 * The refresh time-expiry token call.
 */
const String TokenRefreshUri = RendezvousServerAddress + RendezvousProtocolVersion + String("/peer/%s/token");

/* Buffer time to subtract from the token expiry time specified by the Rendezvous Server so that we try to get new tokens
 * before the old tokens actually expire at the Server */
const uint32_t TURN_TOKEN_EXPIRY_TIME_BUFFER_IN_SECONDS = 60;

/* Acceptable max size of the TURN token in bytes */
const uint32_t TURN_ACCT_TOKEN_MAX_SIZE = 90;

/* Attribute size of the TURN account token */
const uint32_t TURN_ACCT_TOKEN_ATTRIBUTE_HEADER_SIZE = 10;

/**
 * Base InterfaceMessage class
 */
class InterfaceMessage { };

/**
 * Base InterfaceMessage class
 */
class InterfaceResponse { };

/**
 * The structure defining the attributes associated with an
 * Advertisement. Currently the fields are not defined in the
 * interface document and is just a place holder.
 */
typedef struct _AdvertisementAttributes {
    uint32_t undefined;

    _AdvertisementAttributes() : undefined(0xffffffff) { }
} AdvertisementAttributes;

/**
 * The structure defining the application meta data for the peer.
 * Currently the fields are not defined in the interface document
 * and is just a place holder.
 */
typedef struct _PeerInfo {
    uint32_t undefined;

    _PeerInfo() : undefined(0xffffffff) { }
} PeerInfo;

/**
 * The structure defining the components of a single
 * Advertisement.
 */
typedef struct _Advertisement {
    /**
     * The service name to be advertised
     */
    String service;

    /**
     * The attributes associated with the service
     */
    AdvertisementAttributes attribs;

} Advertisement;

/**
 * The message used by an AllJoyn Daemon to advertise
 * services to the Rendezvous Server.
 */
class AdvertiseMessage : public InterfaceMessage {
  public:
    /**
     * The application meta data for the peer
     */
    PeerInfo peerInfo;

    /**
     * The array of advertisements
     */
    list<Advertisement> ads;

};


/**
 * The generic response structure received from the Rendezvous Server.
 */
class GenericResponse : public InterfaceResponse {
  public:
    /**
     * The peerID of the Daemon that sent the request as a response
     * for which the response was received.
     */
    String peerID;
};

/**
 * The refresh token response received from the Rendezvous Server.
 */
class TokenRefreshResponse : public InterfaceResponse {
  public:
    /**
     * The Relay account name.
     */
    String acct;

    /**
     * The Relay account password.
     */
    String pwd;

    /**
     * It represents the time-stamp when the tokens would expire.
     */
    uint32_t expiryTime;

    /**
     * It represents the time-stamp when the response is received.
     */
    uint32_t recvTime;
};

/**
 * The structure defining the additional filter to be applied on the advertisement
 * as a part of the search. Currently the fields are not defined in the
 * interface document and is just a place holder.
 */
typedef struct _SearchFilter {
    uint32_t undefined;

    _SearchFilter() : undefined(0xffffffff) { }
} SearchFilter;

/**
 * The enum defining the type of match that the daemon wishes to initiate.
 */
typedef enum _SearchMatchType {

    /* ProximityBased search match */
    PROXIMITY_BASED = 0

} SearchMatchType;

/**
 * The structure defining the format of a search.
 */
typedef struct _Search {
    /**
     * The service name to search
     */
    String service;

    /**
     * The type of match that the daemon wishes to initiate
     */
    SearchMatchType matchType;

    /**
     * The additional filter on the advertisement
     */
    SearchFilter filter;

    /**
     * This field identifies the search window for the search. By default search is active until
     * explicitly cleared. This field might be used for temporal matching in future releases.
     */
    uint32_t timeExpiry;

    _Search() : matchType(PROXIMITY_BASED), timeExpiry(0) { }

} Search;

/**
 * The message used by an AllJoyn Daemon to search for
 * services from the Rendezvous Server.
 */
class SearchMessage  : public InterfaceMessage {
  public:
    /**
     * The application meta data for the peer
     */
    PeerInfo peerInfo;

    /**
     * The array of searches
     */
    list<Search> search;

};

/**
 * The structure defining the Wi-Fi related proximity info.
 */
typedef struct _WiFiProximity {

    /**
     * If set to true, the peer is currently attached to the access point
     * with bssid of BSSID.
     */
    bool attached;

    /**
     * BSSID of an access point
     */
    String BSSID;

    /**
     * SSID of an access point
     */
    String SSID;

    _WiFiProximity() : attached(false) { }
} WiFiProximity;

/**
 * The structure defining the Bluetooth related proximity info.
 */
typedef struct _BTProximity {

    /**
     * If set to true, the MAC address is that of the BT device of self.
     */
    bool self;

    /**
     * MAC address of a Bluetooth device.
     */
    String MAC;

    _BTProximity() : self(false) { }
} BTProximity;

/**
 * The structure defining the Bluetooth related proximity info.
 */
class ProximityMessage : public InterfaceMessage {
  public:
    /**
     * The list of Wi-Fi access points that device is seeing.
     */
    list<WiFiProximity> wifiaps;

    /**
     * The list of Bluetooth devices that device is seeing.
     */
    list<BTProximity> BTs;

};

/**
 * The enum defining the different types of ICE address candidates.
 */
typedef enum _ICECandidateType {
    /**
     * Invalid Value.
     */
    INVALID_CANDIDATE = 0,
    /**
     * Host Candidate.
     */
    HOST_CANDIDATE,
    /**
     * Server Reflexive Candidate.
     */
    SRFLX_CANDIDATE,
    /**
     * Peer Reflexive Candidate.
     */
    PRFLX_CANDIDATE,
    /**
     * Relay Candidate.
     */
    RELAY_CANDIDATE
} ICECandidateType;

/**
 * The enum defining the different types of transports used by ICE.
 */
typedef enum _ICETransportType {
    /**
     * Invalid Value.
     */
    INVALID_TRANSPORT = 0,
    /**
     * UDP Transport.
     */
    UDP_TRANSPORT,
    /**
     * TCP Transport.
     */
    TCP_TRANSPORT,
} ICETransportType;

/**
 * The structure defining the ICE address candidates.
 */
typedef struct _ICECandidates {

    /**
     * The candidate type.
     */
    ICECandidateType type;

    /**
     * The foundation attribute associated with an ICE candidate.
     */
    String foundation;

    /**
     * The component ID associated with an ICE candidate.
     */
    uint16_t componentID;

    /**
     * The transport type.
     */
    ICETransportType transport;

    /**
     * The priority value.
     */
    uint32_t priority;

    /**
     * The IP address of the candidate.
     */
    IPAddress address;

    /**
     * The port number associated of the candidate.
     */
    uint16_t port;

    /**
     * The remote address; only present if candidate type is not HOST_CANDIDATE.
     */
    IPAddress raddress;

    /**
     * The remote port; only present if candidate type is not HOST_CANDIDATE.
     */
    uint16_t rport;

    _ICECandidates() : type(INVALID_CANDIDATE), transport(UDP_TRANSPORT), priority(0), port(0), rport(0) { }

} ICECandidates;

/**
 * The structure defining the ICE address candidates message
 * sent to the Rendezvous Server.
 */
class ICECandidatesMessage  : public InterfaceMessage {
  public:

    /**
     * The user name fragment used by ICE for message integrity.
     */
    String ice_ufrag;

    /**
     * The password used by ICE for message integrity.
     */
    String ice_pwd;

    /**
     * The array of address candidates
     */
    list<ICECandidates> candidates;

    /**
     * If set to true, the Rendezvous Server will
     * be requested to append the STUN server
     * information before passing on this address
     * candidate message to the other peer.
     */
    bool requestToAddSTUNInfo;

    /**
     * The peer ID of the destination daemon to which this message is being sent.
     */
    String destinationPeerID;

    ICECandidatesMessage() : requestToAddSTUNInfo(false) { }

};

/**
 * The enum defining the different types of possible
 * responses that can be received from the
 * Rendezvous Server.
 */
typedef enum _ResponseType {
    /**
     * Invalid Response
     */
    INVALID_RESPONSE = 0,
    /**
     * The search match response
     */
    SEARCH_MATCH_RESPONSE,
    /**
     * The match revoked response
     */
    MATCH_REVOKED_RESPONSE,
    /**
     * The address candidate response
     */
    ADDRESS_CANDIDATES_RESPONSE,
    /**
     * The start ICE checks response
     */
    START_ICE_CHECKS_RESPONSE
} ResponseType;

/**
 * Structure defining the Relay server info.
 */
typedef struct _RelayInfo {
    /**
     * The Relay server address.
     */
    IPAddress address;

    /**
     * The Relay port.
     */
    uint16_t port;

    _RelayInfo() : port(3478) { }

} RelayInfo;

/**
 * Structure defining the STUN server info.
 */
typedef struct _STUNServerInfo {
    /**
     * The STUN server address.
     */
    IPAddress address;

    /**
     * The STUN port.
     */
    uint16_t port;

    /**
     * The STUN & Relay server account name.
     */
    String acct;

    /**
     * The STUN & Relay server account password.
     */
    String pwd;

    /**
     * Time-stamp when the token would expire in milli seconds.
     */
    uint32_t expiryTime;

    /**
     * Time-stamp when the token was received.
     */
    uint32_t recvTime;

    /**
     * If true, valid relay server information is
     * present in the relay field.
     */
    bool relayInfoPresent;

    /**
     * The Relay Server info.
     */
    RelayInfo relay;

    _STUNServerInfo() : port(3478) { }

} STUNServerInfo;

/**
 * Structure defining the search match response message.
 */
class SearchMatchResponse : public InterfaceResponse {
  public:

    /* The unique identifier assigned to a match by the server.
     * It is utilized later to refresh time-expired token. */
    String searchedService;

    /**
     * The service name that has resulted in this match message
     * being sent.
     */
    String service;

    /**
     * The peer address of the Daemon to which the matched service is
     * connected to.
     */
    String peerAddr;

    /**
     * The application meta data for the peer running the matched
     * service.
     */
    PeerInfo peerInfo;

    /**
     * The STUN server info.
     */
    STUNServerInfo STUNInfo;

};

/**
 * The StartICEChecks response structure received from the Rendezvous Server.
 */
class StartICEChecksResponse : public InterfaceResponse {
  public:
    /**
     * The peerAddress of the remote daemon running the client that received the
     * address candidates from this daemon.
     */
    String peerAddr;
};

/**
 * Structure defining the match revoked message.
 */
class MatchRevokedResponse : public InterfaceResponse {
  public:

    /**
     * The peer address of the Daemon to which the matched service is
     * connected to.
     */
    String peerAddr;

    /**
     * If this flag is set to true, the all services from the peerID
     * are deleted from the discovered list.
     */
    bool deleteAll;

    /**
     * The list of service names being revoked
     */
    list<String> services;

    MatchRevokedResponse() : deleteAll(false) { }

};

/**
 * The structure defining the ICE address candidates message
 * sent to the Rendezvous Server.
 */
class AddressCandidatesResponse : public InterfaceResponse {
  public:

    /**
     * The peer address of the Daemon that sent this Address Candidate Message
     * to the Rendezvous Server.
     */
    String peerAddr;

    /**
     * The user name fragment used by ICE for message integrity.
     */
    String ice_ufrag;

    /**
     * The password used by ICE for message integrity.
     */
    String ice_pwd;

    /**
     * The array of address candidates
     */
    list<ICECandidates> candidates;

    /**
     * If true, valid STUN information is present in
     * STUNInfoPresent.
     */
    bool STUNInfoPresent;

    /**
     * The STUN server info.
     */
    STUNServerInfo STUNInfo;

    AddressCandidatesResponse() : STUNInfoPresent(false) { }

};

/**
 * The structure defining a response received from the Rendezvous Server.
 */
typedef struct _Response {
    /**
     * The response type
     */
    ResponseType type;

    /**
     * The response message
     */
    InterfaceResponse* response;

    _Response() : type(INVALID_RESPONSE) { }
} Response;

/**
 * The structure defining the array of responses received
 * from the Rendezvous Server.
 */
typedef struct _ResponseMessage {
    /**
     * The list of response messages
     */
    list<Response> msgs;

} ResponseMessage;

/**
 * The enum defining the SASL Authentication Mechanism types.
 */
typedef enum _SASLAuthenticationMechanism {

    /*SCRAM-SHA-1 Authentication Mechanism*/
    SCRAM_SHA_1_MECHANISM = 0

} SASLAuthenticationMechanism;

/**
 * The structure defining the Client Login Request.
 */
class ClientLoginRequest : public InterfaceMessage {
  public:
    /**
     * This boolean indicates if this message is the initial message
     * sent from the client to the server in the SASL exchange
     */
    bool firstMessage;

    /**
     * The daemon ID. This is the persistent identifier for the daemon.
     */
    String daemonID;

    /**
     * This is populated in the first log-in message by the client to
     * flush the peer-state at the RDVS Server. The default value is
     * false.
     */
    bool clearClientState;

    /**
     * The SASL authentication mechanism. The current valid choice is SCRAM_SHA_1.
     */
    SASLAuthenticationMechanism mechanism;

    /**
     * The authentication message complaint to RFC5802.
     */
    String message;

    ClientLoginRequest() : firstMessage(false), clearClientState(false), mechanism(SCRAM_SHA_1_MECHANISM) { }

};

/**
 * The structure defining the Config Data.
 */
class ConfigData {
  public:
    /**
     * Boolean indicating if a valid Tkeepalive is present.
     */
    bool TkeepalivePresent;

    /* The keep alive timer value in seconds at the server. */
    uint32_t Tkeepalive;

    void SetTkeepalive(uint32_t value) {
        TkeepalivePresent = true;
        Tkeepalive = value;
    }
};

/**
 * The structure defining the Client Login First Response.
 */
typedef struct _ClientLoginFirstResponse {

    /**
     * The authentication message complaint to RFC5802.
     */
    String message;

} ClientLoginFirstResponse;

/**
 * The structure defining the Client Login Final Response.
 */
typedef struct _ClientLoginFinalResponse {

    /**
     * The authentication message complaint to RFC5802.
     */
    String message;

    /**
     * Boolean indicating if a valid peerID is present in the response.
     */
    bool peerIDPresent;

    /**
     * Peer identifier used by the daemon for Rendezvous Session.
     */
    String peerID;

    /**
     * Boolean indicating if a valid peerAddr is present in the response.
     */
    bool peerAddrPresent;

    /**
     * Peer identifier used by the daemon for Rendezvous Session.
     */
    String peerAddr;

    /**
     * Boolean indicating if a valid daemonRegistrationRequired is present in the response.
     */
    bool daemonRegistrationRequiredPresent;

    /**
     * True means the daemonID and username pair is not yet registered with the server.
     */
    bool daemonRegistrationRequired;

    /**
     * Boolean indicating if a valid sessionActive is present in the response.
     */
    bool sessionActivePresent;

    /**
     * True means that a session from this daemon is still active in the Server.
     */
    bool sessionActive;

    /**
     * Boolean indicating if a valid configData is present in the response.
     */
    bool configDataPresent;

    /**
     * Configuration values.
     */
    ConfigData configData;

    void SetpeerID(String peerid) {
        peerIDPresent = true;
        peerID = peerid;
    }

    void SetpeerAddr(String peeraddr) {
        peerAddrPresent = true;
        peerAddr = peeraddr;
    }

    void SetdaemonRegistrationRequired(bool value) {
        daemonRegistrationRequiredPresent = true;
        daemonRegistrationRequired = value;
    }

    void SetsessionActive(bool value) {
        sessionActivePresent = true;
        sessionActive = value;
    }

    void SetconfigData(ConfigData value) {
        configDataPresent = true;
        configData = value;
    }

} ClientLoginFinalResponse;

/**
 * The enum defining the SASL error codes.
 */
typedef enum _SASLError {

    /*Invalid unrecognized error*/
    INVALID,

    /*invalid-encoding*/
    INVALID_ENCODING,

    /*extensions-not-supported*/
    EXTENSIONS_NOT_SUPPORTED,

    /*invalid-proof*/
    INVALID_PROOF,

    /*channel-bindings-dont-match*/
    CHANNEL_BINDINGS_DONT_MATCH,

    /*server-does-support-channel-binding*/
    SERVER_DOES_NOT_SUPPORT_CHANNEL_BINDING,

    /*channel-binding-not-supported*/
    CHANNEL_BINDING_NOT_SUPPORTED,

    /*unsupported-channel-binding-type*/
    UNSUPPORTED_CHANNEL_BINDING_TYPE,

    /*unknown-user*/
    UNKNOWN_USER,

    /*invalid-username-encoding*/
    INVALID_USERNAME_ENCODING,

    /*no-resources*/
    NO_RESOURCES,

    /*other-error*/
    OTHER_ERROR,

    /*deactivated-user*/
    DEACTIVATED_USER

} SASLError;

/**
 * The structure defining the authentication message complaint to RFC5802.
 */
typedef struct _SASLMessage {

    /**
     * Boolean indicating if a valid a attribute is present.
     */
    bool aPresent;

    /**
     * This attribute specifies an authorization identity.
     */
    String a;

    /**
     * Boolean indicating if a valid n attribute is present.
     */
    bool nPresent;

    /**
     * This attribute specifies the name of the user whose password is used for authentication.
     */
    String n;

    /**
     * Boolean indicating if a valid m attribute is present.
     */
    bool mPresent;

    /**
     * This attribute is reserved for future extensibility.
     */
    String m;

    /**
     * Boolean indicating if a valid r attribute is present.
     */
    bool rPresent;

    /**
     * This attribute specifies a sequence of random printable ASCII characters excluding ','
     * which forms the nonce used as input to the hash function.
     */
    String r;

    /**
     * Boolean indicating if a valid c attribute is present.
     */
    bool cPresent;

    /**
     * This REQUIRED attribute specifies the base64-encoded GS2 header and channel binding data.
     */
    String c;

    /**
     * Boolean indicating if a valid s attribute is present.
     */
    bool sPresent;

    /**
     * This attribute specifies the base64-encoded salt used by the server for this user.
     */
    String s;

    /**
     * Boolean indicating if a valid i attribute is present.
     */
    bool iPresent;

    /**
     * This attribute specifies an iteration count for the selected hash function and user,
     * and MUST be sent by the server along with the user's salt.
     */
    uint32_t i;

    /**
     * Boolean indicating if a valid p attribute is present.
     */
    bool pPresent;

    /**
     * This attribute specifies a base64-encoded ClientProof.
     */
    String p;

    /**
     * Boolean indicating if a valid v attribute is present.
     */
    bool vPresent;

    /**
     * This attribute specifies a base64-encoded ServerSignature.
     */
    String v;

    /**
     * Boolean indicating if a valid e attribute is present.
     */
    bool ePresent;

    /**
     * This attribute specifies an error that occurred during authentication exchange.
     */
    SASLError e;

    void Set_a(String value) {
        aPresent = true;
        a = value;
    }

    bool is_a_Present() {
        return aPresent;
    }

    void Set_n(String value) {
        nPresent = true;
        n = value;
    }

    bool is_n_Present() {
        return nPresent;
    }

    void Set_m(String value) {
        mPresent = true;
        m = value;
    }

    bool is_m_Present() {
        return mPresent;
    }

    void Set_r(String value) {
        rPresent = true;
        r = value;
    }

    bool is_r_Present() {
        return rPresent;
    }

    void Set_c(String value) {
        cPresent = true;
        c = value;
    }

    bool is_c_Present() {
        return cPresent;
    }

    void Set_s(String value) {
        sPresent = true;
        s = value;
    }

    bool is_s_Present() {
        return sPresent;
    }

    void Set_i(String value) {
        iPresent = true;
        i = StringToU32(value);
    }

    bool is_i_Present() {
        return iPresent;
    }

    void Set_p(String value) {
        pPresent = true;
        p = value;
    }

    bool is_p_Present() {
        return pPresent;
    }

    void Set_v(String value) {
        vPresent = true;
        v = value;
    }

    bool is_v_Present() {
        return vPresent;
    }

    void Set_e(SASLError value) {
        ePresent = true;
        e = value;
    }

    bool is_e_Present() {
        return ePresent;
    }

    void clear() {
        aPresent = false;
        a.erase();
        nPresent = false;
        n.erase();
        mPresent = false;
        m.erase();
        rPresent = false;
        r.erase();
        cPresent = false;
        c.erase();
        sPresent = false;
        s.erase();
        iPresent = false;
        i = 0;
        pPresent = false;
        p.erase();
        vPresent = false;
        v.erase();
        ePresent = false;
        e = INVALID;
    }

} SASLMessage;

/**
 * The structure defining the Daemon Registration Message.
 */
class DaemonRegistrationMessage : public InterfaceMessage {
  public:
    /**
     * The daemon ID.
     */
    String daemonID;

    /**
     * Software version of the daemon.
     */
    String daemonVersion;

    /**
     * Make of the device.
     */
    String devMake;

    /**
     * Model of the device.
     */
    String devModel;

    /**
     * High level operating system on the device.
     */
    OSType osType;

    /**
     * High level OS version.
     */
    String osVersion;

};

/**
 * The structure defining the Token Refresh Message.
 */
class TokenRefreshMessage : public InterfaceMessage {
  public:

    /**
     * True indicates that a client is sending this message.
     */
    bool client;

    /**
     * The remote peer address corresponding to this matchID.
     */
    String remotePeerAddress;

    /* Listener to call back on availability of new refreshed tokens */
    TokenRefreshListener* tokenRefreshListener;
};

/**
 * Worker function used to generate the enum value corresponding
 * to the ICE candidate type.
 */
ICECandidateType GetICECandidateTypeValue(String type);

/**
 * Worker function used to generate the enum value corresponding
 * to the ICE transport type.
 */
ICETransportType GetICETransportTypeValue(String type);

/**
 * Worker function used to generate the string corresponding
 * to the transport type.
 */
String GetICETransportTypeString(ICETransportType type);

/**
 * Worker function used to generate the string corresponding
 * to the ICE candidate type.
 */
String GetICECandidateTypeString(ICECandidateType type);

/**
 * Worker function used to generate the string corresponding
 * to the Message Response Type.
 */
String PrintResponseType(ResponseType type);

/**
 * Worker function used to generate an Advertisement in
 * the JSON format.
 */
String GenerateJSONAdvertisement(AdvertiseMessage message);

/**
 * Worker function used to generate a Search in
 * the JSON format.
 */
String GenerateJSONSearch(SearchMessage message);

/**
 * Worker function used to generate a Proximity Message in
 * the JSON format.
 */
String GenerateJSONProximity(ProximityMessage message);

/**
 * Worker function used to generate an ICE Candidates Message in
 * the JSON format.
 */
String GenerateJSONCandidates(ICECandidatesMessage message);

/**
 * Worker function used to parse a generic response
 */
QStatus ParseGenericResponse(Json::Value receivedResponse, GenericResponse& parsedResponse);

/**
 * Worker function used to parse a refresh token response
 */
QStatus ParseTokenRefreshResponse(Json::Value receivedResponse, TokenRefreshResponse& parsedResponse);

/**
 * Worker function used to print a parsed response
 */
void PrintMessageResponse(Response response);

/**
 * Worker function used to parse a messages response
 */
QStatus ParseMessagesResponse(Json::Value receivedResponse, ResponseMessage& parsedResponse);

/**
 * Worker function used to generate the string corresponding
 * to the authentication mechanism type.
 */
String GetSASLAuthMechanismString(SASLAuthenticationMechanism authMechanism);

/**
 * Worker function used to generate an ICE Candidates Message in
 * the JSON format.
 */
String GenerateJSONClientLoginRequest(ClientLoginRequest request);

/**
 * Worker function used to parse the client login first response
 */
QStatus ParseClientLoginFirstResponse(Json::Value receivedResponse, ClientLoginFirstResponse& parsedResponse);

/**
 * Worker function used to parse the client login final response
 */
QStatus ParseClientLoginFinalResponse(Json::Value receivedResponse, ClientLoginFinalResponse& parsedResponse);

/**
 * Worker function used to generate the enum corresponding
 * to the error string.
 */
SASLError GetSASLError(String errorStr);

/**
 * Worker function used to print the string equivalent of a SASL error.
 */
String GetSASLErrorString(SASLError error);

/**
 * Worker function used to set an attribute in the SASL Message.
 */
void SetSASLAttribute(char attribute, String attrVal, String& retMsg);

/**
 * Worker function used to generate a SASL Message string from the SASL attributes.
 */
String GenerateSASLMessage(SASLMessage message, bool firstMessage);

/**
 * Worker function used to parse a SASL Message.
 */
SASLMessage ParseSASLMessage(String message);

/**
 * Worker function used to generate the string corresponding
 * to the OS type.
 */
String GetOSTypeString(OSType type);

/**
 * Worker function used to generate the string corresponding
 * to the Search Match Type type.
 */
String GetSearchMatchTypeString(SearchMatchType type);

/**
 * Worker function used to generate an Daemon Registration Message in
 * the JSON format.
 */
String GenerateJSONDaemonRegistrationMessage(DaemonRegistrationMessage message);

/**
 * Returns the Advertisement message URI.
 */
String GetAdvertisementUri(String peerID);

/**
 * Returns the Search message URI.
 */
String GetSearchUri(String peerID);

/**
 * Returns the Proximity message URI.
 */
String GetProximityUri(String peerID);

/**
 * Returns the Address Candidates message URI.
 */
String GetAddressCandidatesUri(String selfPeerID, String destPeerAddress, bool addStun);

/**
 * Returns the Rendezvous Session Delete message URI.
 */
String GetRendezvousSessionDeleteUri(String peerID);

/**
 * Returns the GET message URI.
 */
String GetGETUri(String peerID);

/**
 * Returns the Client Login URI.
 */
String GetClientLoginUri(void);

/**
 * Returns the Daemon Registration message URI.
 */
String GetDaemonRegistrationUri(String peerID);

/**
 * Returns the refresh token URI.
 */
String GetTokenRefreshUri(String peerID);

}

#endif /* RENDEZVOUSSERVERINTERFACE_H_ */
