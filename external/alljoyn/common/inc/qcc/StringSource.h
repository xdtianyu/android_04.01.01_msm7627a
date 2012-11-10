/**
 * @file
 *
 * Source implementation used to retrieve bytes from qcc::String
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

#ifndef _QCC_STRINGSOURCE_H
#define _QCC_STRINGSOURCE_H

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Event.h>
#include <qcc/Stream.h>

#include <algorithm>

namespace qcc {

/**
 * StringSource provides Source based retrieval from std:string storage.
 */
class StringSource : public Source {
  public:

    /**
     * Construct a StringSource.
     * @param str   Source contents
     */
    StringSource(const qcc::String str) : str(str), outIdx(0) { }

    /**
     * Construct a StringSource from data.
     *
     * @param data  Source contents
     * @param len   The length of the data.
     */
    StringSource(const void* data, size_t len) : outIdx(0) { str.insert(0, (const char*)data, len); }

    /** Destructor */
    virtual ~StringSource() { }

    /**
     * Pull bytes from the source.
     * The source is exhausted when ER_NONE is returned.
     *
     * @param buf          Buffer to store pulled bytes
     * @param reqBytes     Number of bytes requested to be pulled from source.
     * @param actualBytes  Actual number of bytes retrieved from source.
     * @param timeout      Timeout in milliseconds.
     * @return   OI_OK if successful. ER_NONE if source is exhausted. Otherwise an error.
     */
    QStatus PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout = Event::WAIT_FOREVER);

  private:
    qcc::String str;    /**< storage for byte stream */
    size_t outIdx;      /**< index to next byte in str to be returned */
};

}

#endif
