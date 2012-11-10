/**
 * @file
 *
 * Sink implementation which stores data in a qcc::String.
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

#ifndef _QCC_STRINGSINK_H
#define _QCC_STRINGSINK_H

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Stream.h>

namespace qcc {

/**
 * StringSink provides Sink based storage for bytes.
 */
class StringSink : public Sink {
  public:

    /** Destructor */
    virtual ~StringSink() { }

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
        str.append((char*)buf, numBytes);
        numSent = numBytes;
        return ER_OK;
    }

    /**
     * Get reference to sink storage.
     * @return string used to hold Sink data.
     */
    qcc::String& GetString() { return str; }

    /**
     * Clear existing bytes from sink.
     */
    void Clear() { str.clear(); }

  private:
    qcc::String str;    /**< storage for byte stream */
};

}

#endif
