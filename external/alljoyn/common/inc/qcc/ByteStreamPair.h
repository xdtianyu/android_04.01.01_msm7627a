/**
 * @file
 *
 * Provides a pair of byte streams that can be used to buffer bi-directional
 * stream traffic between two endpoints.
 */

/******************************************************************************
 *
 *
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

#ifndef _QCC_BYTESTREAMPAIR_H
#define _QCC_BYTESTREAMPAIR_H

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Pipe.h>
#include <qcc/Stream.h>

namespace qcc {

/**
 * Stream implementation returned by ByteStreamPair
 */
class ByteStream : public Stream {
  public:

    /** Construct an ByteStream */
    ByteStream() : source(NULL) { };

    /**
     * Virtual destructor for derivable class.
     */
    virtual ~ByteStream() { }

    /**
     * Get the source.
     *
     * @return Underlying source
     */
    Source* GetSource() { return &pipe; }

    /**
     * Set the source.
     *
     * @param source  Incomming byte source.
     */
    void SetSource(Source* source) { this->source = source; }

    /**
     * Pull bytes from the ByteStream
     * The source is exhausted when ER_NONE is returned.
     *
     * @param buf          Buffer to store pulled bytes
     * @param reqBytes     Number of bytes requested to be pulled from source.
     * @param actualBytes  Actual number of bytes retrieved from source.
     * @return   OI_OK if successful. ER_NONE if source is exhausted. Otherwise an error.
     */
    QStatus PullBytes(void* buf, size_t reqBytes, size_t& actualBytes)
    {
        QStatus status = ER_FAIL;

        if (source) {
            status = source->PullBytes(buf, reqBytes, actualBytes);
        }
        return status;
    }

    /**
     * Push bytes into the sink.
     *
     * @param buf          Buffer to store pulled bytes
     * @param numBytes     Number of bytes from buf to send to sink.
     * @param numSent      Number of bytes actually consumed by sink.
     * @return   ER_OK if successful.
     */
    QStatus PushBytes(const void* buf, size_t numBytes, size_t& numSent)
    {
        return pipe.PushBytes(buf, numBytes, numSent);
    }

  private:
    Source* source;
    Pipe pipe;
};


/**
 * Provides a pair of byte streams that can be used to buffer bi-directional
 * stream traffic between two endpoints.
 */
class ByteStreamPair {
  public:

    /**
     * Construct an ByteStreamPair
     */
    ByteStreamPair()
    {
        first.SetSource(second.GetSource());
        second.SetSource(first.GetSource());
    }

    /**
     * Virtual destructor for derivable class.
     */
    virtual ~ByteStreamPair() { }

    /**
     * Get "first" Stream.
     *
     * @return  Reference to "first" stream.
     */
    Stream& GetFirstStream() { return first; }

    /**
     * Get "first" Stream.
     *
     * @return  Reference to "second" stream.
     */
    Stream& GetSecondStream() { return second; }

  private:

    ByteStream first;    /**< "First" side byte stream */
    ByteStream second;   /**< "Second" side byte stream */
};

}  /* namespace */

#endif
