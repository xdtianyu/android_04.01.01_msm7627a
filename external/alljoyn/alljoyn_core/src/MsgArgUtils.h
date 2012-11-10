#ifndef _ALLJOYN_MSGARGUTILS_H
#define _ALLJOYN_MSGARGUTILS_H
/**
 * @file
 * This file defines a set of utitilies for message bus data types and values
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

#ifndef __cplusplus
#error Only include MsgArgUtils.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <stdarg.h>
#include <Status.h>

namespace ajn {

class MsgArgUtils {

  public:

    /**
     * Set an array of MsgArgs by applying the Set() method to each MsgArg in turn.
     *
     * @param args     An array of MsgArgs to set.
     * @param numArgs  [in,out] On input the size of the args array. On output the number of MsgArgs
     *                 that were set. There must be at least enought MsgArgs to completely
     *                 initialize the signature.
     *                 there should at least enough.
     * @param signature   The signature for MsgArg values
     * @param argp        A va_list of one or more values to initialize the MsgArg list.
     *
     * @return - ER_OK if the MsgArgs were successfully set.
     *         - ER_BUS_TRUNCATED if the signature was longer than expected.
     *         - Otherwise an error status.
     */
    static QStatus SetV(MsgArg* args, size_t& numArgs, const char* signature, va_list* argp);

};

}

#endif
