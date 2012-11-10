/**
 * @file This file defines the class for handling the client and server
 * endpoints for the message bus wire protocol
 */

/******************************************************************************
 * Copyright 2009-2010, Qualcomm Innovation Center, Inc.
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
#include <qcc/GUID.h>

#include <BusEndpoint.h>

using namespace qcc;
using namespace ajn;

String BusEndpoint::GetControllerUniqueName() const {

    /* An endpoint with unique name :X.Y has a controller with a unique name :X.1 */
    String ret = GetUniqueName();
    ret[qcc::GUID128::SHORT_SIZE + 2] = '1';
    ret.resize(qcc::GUID128::SHORT_SIZE + 3);
    return ret;
}
