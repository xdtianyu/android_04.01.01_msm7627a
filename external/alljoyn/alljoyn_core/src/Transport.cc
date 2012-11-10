/**
 * @file
 * Transport is a base class for all Message Bus Transport implementations.
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
#include <qcc/platform.h>
#include <qcc/String.h>
#include "Transport.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

QStatus Transport::ParseArguments(const char* transportName,
                                  const char* args,
                                  map<qcc::String, qcc::String>& argMap)
{
    qcc::String tpNameStr(transportName);
    tpNameStr.push_back(':');
    qcc::String argStr(args);

    /* Skip to the first param */
    size_t pos = argStr.find(tpNameStr);

    if (qcc::String::npos == pos) {
        return ER_BUS_BAD_TRANSPORT_ARGS;
    } else {
        pos += tpNameStr.size();
    }

    size_t endPos = 0;
    while (qcc::String::npos != endPos) {
        size_t eqPos = argStr.find_first_of('=', pos);
        endPos = (eqPos == qcc::String::npos) ? qcc::String::npos : argStr.find_first_of(",;", eqPos);
        if (qcc::String::npos != eqPos) {
            argMap[argStr.substr(pos, eqPos - pos)] = (qcc::String::npos == endPos) ? argStr.substr(eqPos + 1) : argStr.substr(eqPos + 1, endPos - eqPos - 1);
        }
        pos = endPos + 1;
    }
    return ER_OK;
}

}
