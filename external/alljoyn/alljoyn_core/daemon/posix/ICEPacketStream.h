/**
 * @file
 * ICEPacketStream is a UDP based implementation of the PacketStream interface.
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
#ifndef _ALLJOYN_ICEPACKETSTREAM_H
#define _ALLJOYN_ICEPACKETSTREAM_H

#include <qcc/platform.h>
#include <sys/socket.h>
#include <qcc/String.h>
#include <qcc/IPAddress.h>
#include <Status.h>
#include "Packet.h"
#include "PacketStream.h"
#include "StunMessage.h"
#include "Stun.h"
#include "StunAttribute.h"
#include "ICESession.h"
#include "ICECandidatePair.h"

namespace ajn {

/**
 * ICEPacketStream is a UDP based implementation of the PacketStream interface.
 */
class ICEPacketStream : public PacketStream {
  public:

    /* The value of TURN_ACCT_TOKEN_MAX_SIZE + TURN_ACCT_TOKEN_ATTRIBUTE_HEADER_SIZE accounts for the max size of the
     * TURN username attribute. As this is something that is sent by the server during run time, there is no way
     * for us to know that statically during initialization. */
    static const uint32_t STUN_OVERHEAD = StunMessage::HEADER_SIZE + StunAttributeXorPeerAddress::ATTR_SIZE_WITH_HEADER +
                                          StunAttribute::ATTR_HEADER_SIZE + StunAttributeMessageIntegrity::ATTR_SIZE_WITH_HEADER +
                                          StunAttributeFingerprint::ATTR_SIZE_WITH_HEADER + TURN_ACCT_TOKEN_MAX_SIZE +
                                          TURN_ACCT_TOKEN_ATTRIBUTE_HEADER_SIZE;

    /** Construct a PacketDest from a addr,port */
    static PacketDest GetPacketDest(const qcc::IPAddress& addr, uint16_t port);

    /** Constructor */
    ICEPacketStream();

    /** Constructor */
    ICEPacketStream(ICESession& iceSession, Stun& stunPtr, const ICECandidatePair& selectedPair);

    /** Copy constructor */
    ICEPacketStream(const ICEPacketStream& other);

    /* Assignment */
    ICEPacketStream& operator=(const ICEPacketStream& other);

    /** Destructor */
    ~ICEPacketStream();

    /**
     * Start the PacketStream.
     */
    QStatus Start();

    /**
     * Stop the PacketStream.
     */
    QStatus Stop();

    /**
     * Return true iff ICEPacketStream has a usable socket.
     * @return true iff ICEPacketStream has a usable socket.
     */
    bool HasSocket() { return sock != SOCKET_ERROR; }

    /**
     * Get the PacketEngineAcceptCB timeout alarm.
     *
     * @return Alarm used to indicate PacketEngineAccept timeout.
     */
    const qcc::Alarm& GetTimeoutAlarm() const { return timeoutAlarm; }

    /**
     * Set the PacketEngineAcceptCB timeout alarm.
     *
     * @param timeoutAlarm    Alarm used to indicate PacketEngine accept timeout.
     */
    void SetTimeoutAlarm(const qcc::Alarm& timeoutAlarm) { this->timeoutAlarm = timeoutAlarm; }

    /**
     * Get UDP port.
     */
    uint16_t GetPort() const { return port; }

    /**
     * Get UDP IP addr.
     */
    qcc::String GetIPAddr() const;

    /**
     * Pull bytes from the source.
     * The source is exhausted when ER_NONE is returned.
     *
     * @param buf          Buffer to store pulled bytes
     * @param reqBytes     Number of bytes requested to be pulled from source.
     * @param actualBytes  Actual number of bytes retrieved from source.
     * @param sender       Source type specific representation of the sender of the packet.
     * @param timeout      Time to wait to pull the requested bytes.
     * @return   ER_OK if successful. ER_NONE if source is exhausted. Otherwise an error.
     */
    QStatus PullPacketBytes(void* buf, size_t reqBytes, size_t& actualBytes, PacketDest& sender, uint32_t timeout = qcc::Event::WAIT_FOREVER);

    /**
     * Get the Event indicating that data is available when signaled.
     *
     * @return Event that is signaled when data is available.
     */
    qcc::Event& GetSourceEvent() { return *sourceEvent; }

    /**
     * Get the mtuWithStunOverhead size for this PacketSource.
     *
     * @return MTU of PacketSource
     */
    size_t GetSourceMTU() { return usingTurn ? mtuWithStunOverhead : interfaceMtu; }

    /**
     * Push zero or more bytes into the sink.
     *
     * @param buf          Buffer containing data bytes to be sent
     * @param numBytes     Number of bytes from buf to send to sink. (Must be less that or equal to MTU of PacketSink.)
     * @param dest         Destination for packet bytes.
     * @return   ER_OK if successful.
     */
    QStatus PushPacketBytes(const void* buf, size_t numBytes, PacketDest& dest);

    /**
     * Get the Event that indicates when data can be pushed to sink.
     *
     * @return Event that is signaled when sink can accept more bytes.
     */
    qcc::Event& GetSinkEvent() { return *sinkEvent; }

    /**
     * Get the mtuWithStunOverhead size for this PacketSink.
     *
     * @return MTU of PacketSink
     */
    size_t GetSinkMTU() { return usingTurn ? mtuWithStunOverhead : interfaceMtu; }

    /**
     * Human readable form of UDPPacketDest.
     */
    qcc::String ToString(const PacketDest& dest) const;

    /**
     * Get HMAC key
     *
     * @return hmac key (from ICESession)
     */
    const qcc::String& GetHmacKey() const { return hmacKey; }

    /**
     * Get ICE destination address.
     *
     * @return ICE negociated IPAddress.
     */
    const IPAddress& GetICERemoteAddr() const { return remoteAddress; }

    /**
     * Get ICE destination port
     *
     * @return ICE negociated IP port.
     */
    uint16_t GetICERemotePort() const { return remotePort; }

    /**
     * Return the TURN server's refresh period.
     * This call returns 0 unless the candidate type is Relayed_Candidate.
     *
     * @return TURN server refresh period in milliseconds.
     */
    uint32_t GetTurnRefreshPeriod()
    {
        uint32_t refreshPeriod;
        turnRefreshPeriodUpdateLock.Lock();
        refreshPeriod = turnRefreshPeriod;
        turnRefreshPeriodUpdateLock.Unlock();
        return refreshPeriod;
    }

    /**
     * Return the timestamp of the last TURN server's refresh.
     *
     * @return time of last TURN server refresh.
     */
    uint64_t GetTurnRefreshTimestamp() const { return turnRefreshTimestamp; }

    /**
     * Return the username used for TURN server authentication.
     *
     * @return TURN server username
     */
    qcc::String GetTurnUsername() const { return turnUsername; }

    /**
     * Return the username used for TURN server authentication.
     *
     * @return TURN server username
     */
    uint32_t GetStunKeepAlivePeriod() const { return stunKeepAlivePeriod; }

    /**
     * Return true iff ICEPacketStream is using the local relay candidate.
     * @return true iff ICEPacketStream is using local relay candidate.
     */
    bool IsLocalTurn() const { return localTurn; }

    /**
     * Return true iff ICEPacketStream is using the local host candidate.
     * @return true iff ICEPacketStream is using the local host candidate.
     */
    bool IsLocalHost() const { return localHost; }

    /**
     * Return true iff ICEPacketStream is using the remote host candidate.
     * @return true iff ICEPacketStream is using the remote host candidate.
     */
    bool IsRemoteHost() const { return remoteHost; }

    /**
     * Compose and send a NAT keepalive message.
     */
    QStatus SendNATKeepAlive(void);

    /**
     * Compose and send a TURN refresh message.
     * @param time  64-bit timestamp.
     */
    QStatus SendTURNRefresh(uint64_t time);


  private:
    qcc::IPAddress ipAddress;
    uint16_t port;
    qcc::IPAddress remoteAddress;
    uint16_t remotePort;
    qcc::IPAddress remoteMappedAddress;
    uint16_t remoteMappedPort;
    qcc::IPAddress turnAddress;
    uint16_t turnPort;
    qcc::IPAddress relayServerAddress;
    uint16_t relayServerPort;
    int sock;
    qcc::Event* sourceEvent;
    qcc::Event* sinkEvent;
    size_t mtuWithStunOverhead;
    size_t interfaceMtu;
    bool usingTurn;
    bool localTurn;
    bool localHost;
    bool remoteHost;
    qcc::String hmacKey;
    qcc::String turnUsername;
    Mutex turnRefreshPeriodUpdateLock;
    uint32_t turnRefreshPeriod;
    uint64_t turnRefreshTimestamp;
    uint32_t stunKeepAlivePeriod;
    Mutex sendLock;
    uint8_t* rxRenderBuf;
    uint8_t* txRenderBuf;
    qcc::Alarm timeoutAlarm;

    /**
     * Compose a STUN message with the passed in data.
     */
    QStatus ComposeStunMessage(const void* buf,
                               size_t numBytes,
                               qcc::ScatterGatherList& msgSG);

    /**
     * Strip STUN overhead from a received message.
     */
    QStatus StripStunOverhead(size_t rcvdBytes, void* dataBuf, size_t dataBufLen, size_t& actualBytes);
};

}  /* namespace */

#endif

