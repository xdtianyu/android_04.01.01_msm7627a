#ifndef _FLETCHER32_H
#define _FLETCHER32_H
/**
 * @file
 *
 * This file implements the Fletcher32 hash function.
 *
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

#ifndef __cplusplus
#error Only include Fletcher32.h in C++ code.
#endif

#include <qcc/platform.h>

#include <Status.h>

namespace ajn {

/**
 * This class implements the Fletcher32 hash function
 */
class Fletcher32 {

  public:

    /**
     * Constructor
     */
    Fletcher32() : fletch1(0xFFFF), fletch2(0xFFFF) { }

    /**
     * Update the running checksum.
     *
     * @param data The data to compute the hash over.
     * @param len  The length of the data (number of uint16_t's)
     *
     * @return The current checksum value.
     */
    uint32_t Update(const uint16_t* data, size_t len) {
        while (data && len) {
            size_t l = (len <= 360) ? len : 360;
            len -= l;
            while (l--) {
                fletch1 += *data++;
                fletch2 += fletch1;
            }
            fletch1 = (fletch1 & 0xFFFF) + (fletch1 >> 16);
            fletch2 = (fletch2 & 0xFFFF) + (fletch2 >> 16);
        }
        return (fletch2 << 16) | (fletch1 & 0xFFFF);
    }

  private:

    uint32_t fletch1;
    uint32_t fletch2;
};


}

#endif
