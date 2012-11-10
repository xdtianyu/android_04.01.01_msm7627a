/**
 * @file
 *
 * This file defines a Sink wrapper/filter that buffers input I/O.
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

#ifndef _QCC_BUFFEREDSINK_H
#define _QCC_BUFFEREDSINK_H

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Event.h>
#include <qcc/Stream.h>

namespace qcc {

/**
 * BufferedSink is an Sink wrapper that attempts to write fixed size blocks
 * to an underyling (wrapped) Sink. It is typically used by Sinks which are
 * slow or otherwise sensitive to small chunk writes.
 */
class BufferedSink : public Sink {
  public:

    /**
     * Construct a BufferedSink
     *
     * @param sink        Raw sink to be buffered.
     * @param minChunk    Preferred minimum write size for underlying sink.
     */
    BufferedSink(Sink& sink, size_t minChunk);

    /** Destructor */
    virtual ~BufferedSink();

    /**
     * Push bytes to the sink.
     *
     * @param buf          Bytes to write to sink.
     * @param numBytes     Number of bytes from buf to send to sink.
     * @param numSent      Number of bytes actually consumed by sink.
     * @return   ER_OK if successful.
     */
    QStatus PushBytes(const void* buf, size_t numBytes, size_t& numSent);

    /**
     * Get the Event indicating that data is available when signaled.
     *
     * @return Event that is signaled when data is available.
     */
    Event& GetSinkEvent() { return event; }

    /**
     * Enable write buffering for Sink types that support write buffering.
     *
     * @return ER_OK if write buffering is supported and was enabled.
     */
    QStatus EnableWriteBuffer() { isBuffered = true; return ER_OK; }

    /**
     * Disable write buffering for Sink types that support write buffering.
     *
     * @return ER_OK if write buffering is supported and was disabled.
     */
    QStatus DisableWriteBuffer() { Flush(); isBuffered = false; return ER_OK; }

    /**
     * Flush any buffered write.
     *
     * @return ER_OK if successful.
     */
    QStatus Flush();

  private:

    /**
     * Copy constructor is private and does nothing
     */
    BufferedSink(const BufferedSink& other) : sink(other.sink), event(other.event), minChunk(other.minChunk), buf(NULL), wrPtr(NULL) { }

    /**
     * Assigment operator is private and does nothing
     */
    BufferedSink& operator=(const BufferedSink& other) { return *this; }

    Sink& sink;                 /**< Underlying raw sink */
    Event& event;               /**< IO event for this buffered source */
    const size_t minChunk;      /**< Chunk size */
    uint8_t* buf;               /**< Heap allocated buffer */
    uint8_t* wrPtr;             /**< Pointer to next write position in buf */
    size_t completeIdx;         /**< Number of bytes already sent from buf */
    bool isBuffered;            /**< true iff write buffering is enabled */
};

}

#endif
