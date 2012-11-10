#ifndef _ADLER32_H
#define _ADLER32_H
/**
 * @file
 *
 * This file implements the Adler32 hash function.
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
#error Only include Adler32.h in C++ code.
#endif

#include <qcc/platform.h>

#include <Status.h>

namespace ajn {

/**
 * This class implements the Adler32 hash function
 */
class Adler32 {

  public:

    /**
     * Constructor
     */
    Adler32() : adler(1) { }

    /**
     * Update the running hash.
     *
     * @param data The data to compute the hash over.
     * @param len  The length of the data
     *
     * @return The current hash value.
     */
    uint32_t Update(const uint8_t* data, size_t len) {
        while (data && len) {
            size_t l = len % 3800; // Max number of iterations before modulus must be computed
            uint32_t a = adler & 0xFFFF;
            uint32_t b = adler >> 16;
            len -= l;
            while (l--) {
                a += *data++;
                b += a;
            }
            adler = ((b % ADLER_PRIME) << 16) | (a % ADLER_PRIME);
        }
        return adler;
    }

  private:

    /**
     * The largest prime number that will fit in 16 bits
     */
    static const uint32_t ADLER_PRIME = 65521;

    /**
     * The running hash value.
     */
    uint32_t adler;
};

}

#endif
