#ifndef _STUNCREDENTIAL_H
#define _STUNCREDENTIAL_H
/**
 * @file
 *
 * This file defines the STUN credential class, used for long and short-term
 * credentials.
 *
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
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
#error Only include StunCredential.h in C++ code.
#endif

#include <string>
#include <qcc/platform.h>
#include "Status.h"

using namespace std;
using namespace qcc;

/** @internal */
#define QCC_MODULE "STUNCREDENTIAL"

/**
 * @class StunCredential
 *
 * This class computes the HMAC key for the MESSAGE-INTEGRITY
 * attribute, per RFC 5389, based on whether the credential
 * is long-term or short-term.
 *
 */
class StunCredential {
  private:

    const String password;

    uint8_t* hmacKey;
    uint8_t keyLength;

    void ComputeShortTermKey(void);

    String SASLprep(const String& in) const;

  public:

    StunCredential(const String& password) :
        password(password), hmacKey(NULL), keyLength(0) { ComputeShortTermKey(); }

    /**
     * Destructor for the StunCredential class.  This will delete the key, if any.
     *
     */
    ~StunCredential(void) { free(hmacKey); }


    QStatus GetKey(uint8_t* keyOut, size_t& len) const;

};


#undef QCC_MODULE
#endif
