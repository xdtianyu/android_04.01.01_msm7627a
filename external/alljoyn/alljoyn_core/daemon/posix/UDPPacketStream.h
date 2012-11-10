/**
 * @file
 * UDPPacketStream is a UDP based implementation of the PacketStream interface.
 */

/******************************************************************************
 * Copyright 2011-2012, Qualcomm Innovation Center, Inc.
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
#ifndef _ALLJOYN_UDPPACKETSTREAM_H
#define _ALLJOYN_UDPPACKETSTREAM_H

#include <qcc/platform.h>
#include <sys/socket.h>
#include <qcc/String.h>
#include <Status.h>
#include "Packet.h"
#include "PacketStream.h"

namespace ajn {

/**
 * UDPPacketStream is a UDP based implemntation of the PacketStream interface.
 */
class UDPPacketStream : public PacketStream {
  public:

    /** Construct a PacketDest from a addr,port */
    static PacketDest GetPacketDest(const qcc::String& addr, uint16_t port);

    /** Constructor */
    UDPPacketStream(const char* ifaceName, uint16_t port);

    /** Destructor */
    ~UDPPacketStream();

    /**
     * Start the PacketStream.
     */
    QStatus Start();

    /**
     * Stop the PacketStream.
     */
    QStatus Stop();

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
     * Get the mtu size for this PacketSource.
     *
     * @return MTU of PacketSource
     */
    size_t GetSourceMTU() { return mtu; }

    /**
     * Push zero or more bytes into the sink.
     *
     * @param buf          Buffer to store pulled bytes
     * @param numBytes     Number of bytes from buf to send to sink. (Must be less that or equal to MTU of PacketSink.)
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
     * Get the mtu size for this PacketSink.
     *
     * @return MTU of PacketSink
     */
    size_t GetSinkMTU() { return mtu; }

    /**
     * Human readable form of UDPPacketDest.
     */
    qcc::String ToString(const PacketDest& dest) const;

  private:
    qcc::String ifaceName;
    uint16_t port;
    int sock;
    qcc::Event* sourceEvent;
    qcc::Event* sinkEvent;
    size_t mtu;
    struct sockaddr sa;
};

}  /* namespace */

#endif

