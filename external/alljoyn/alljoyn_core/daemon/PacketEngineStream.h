/**
 * @file
 * PacketEngineStream is an implemenation of qcc::Stream used by PacketEngine.
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
#ifndef _ALLJOYN_PACKETENGINESTREAM_H
#define _ALLJOYN_PACKETENGINESTREAM_H

#include <qcc/platform.h>
#include <qcc/Stream.h>
#include <Status.h>

namespace ajn {

/* Forward Declaration */
class PacketEngine;

/**
 * Stream is a virtual class that defines a standard interface for a streaming source and sink.
 */
class PacketEngineStream : public qcc::Stream {
    friend class PacketEngine;

  public:
    /** Default Constructor */
    PacketEngineStream();

    /** Destructor */
    ~PacketEngineStream();

    /** Copy constructor */
    PacketEngineStream(const PacketEngineStream& other);

    /** Assignment operator */
    PacketEngineStream& operator=(const PacketEngineStream& other);

    /** Equality */
    bool operator==(const PacketEngineStream& other) const;

    uint32_t GetChannelId() const { return chanId; }

    /**
     * Pull bytes from the source.
     * The source is exhausted when ER_NONE is returned.
     *
     * @param buf          Buffer to store pulled bytes
     * @param reqBytes     Number of bytes requested to be pulled from source.
     * @param actualBytes  Actual number of bytes retrieved from source.
     * @param timeout      Time to wait to pull the requested bytes.
     * @return   ER_OK if successful. ER_NONE if source is exhausted. Otherwise an error.
     */
    QStatus PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout = qcc::Event::WAIT_FOREVER);

    /**
     * Get the Event indicating that data is available when signaled.
     *
     * @return Event that is signaled when data is available.
     */
    qcc::Event& GetSourceEvent() { return *sourceEvent; }

    /**
     * Push zero or more bytes into the sink with per-msg time-to-live.
     *
     * @param buf          Buffer to store pulled bytes
     * @param numBytes     Number of bytes from buf to send to sink.
     * @param numSent      Number of bytes actually consumed by sink.
     * @param ttl          Time-to-live in ms or 0 for infinite ttl.
     * @return   ER_OK if successful.
     */
    QStatus PushBytes(const void* buf, size_t numBytes, size_t& numSent, uint32_t ttl);

    /**
     * Push zero or more bytes into the sink with infinite ttl.
     *
     * @param buf          Buffer to store pulled bytes
     * @param numBytes     Number of bytes from buf to send to sink.
     * @param numSent      Number of bytes actually consumed by sink.
     * @return   ER_OK if successful.
     */
    QStatus PushBytes(const void* buf, size_t numBytes, size_t& numSent) {
        return PushBytes(buf, numBytes, numSent, 0);
    }

    /**
     * Get the Event that indicates when data can be pushed to sink.
     *
     * @return Event that is signaled when sink can accept more bytes.
     */
    qcc::Event& GetSinkEvent() { return *sinkEvent; }

    /**
     * Set the send timeout for this sink.
     *
     * @param sendTimeout   Send timeout in ms.
     */
    void SetSendTimeout(uint32_t sendTimeout) { this->sendTimeout = sendTimeout; }

  private:
    PacketEngine* engine;
    uint32_t chanId;
    qcc::Event* sourceEvent;
    qcc::Event* sinkEvent;
    uint32_t sendTimeout;

    PacketEngineStream(PacketEngine& engine, uint32_t chanId, qcc::Event& sourceEvent, qcc::Event& sinkEvent);
};

}  /* namespace */

#endif
