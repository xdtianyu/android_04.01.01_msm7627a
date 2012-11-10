/**
 * @file
 * ClientTransport is an implementation of Transport that listens
 * on an AF_UNIX socket.
 */

/******************************************************************************
 * Copyright 2009-2012, Qualcomm Innovation Center, Inc.
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

#include <map>

#include <alljoyn/Session.h>
#include "ClientTransport.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

/*
 * This platform only supports a bundled daemon so has no client transport
 */
const char* ClientTransport::TransportName = NULL;

QStatus ClientTransport::NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, map<qcc::String, qcc::String>& argMap) const
{
    return ER_FAIL;
}

QStatus ClientTransport::Connect(const char* connectArgs, const SessionOpts& opts, BusEndpoint** newep)
{
    return ER_FAIL;
}

} // namespace ajn
