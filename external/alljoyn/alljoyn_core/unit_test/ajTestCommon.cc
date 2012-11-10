/******************************************************************************
 *
 *
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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

#include "ajTestCommon.h"

#include <qcc/Environ.h>

qcc::String ajn::getConnectArg() {
    qcc::Environ* env = qcc::Environ::GetAppEnviron();
#if defined(QCC_OS_WINDOWS)
    return env->Find("BUS_ADDRESS", "tcp:addr=127.0.0.1,port=9956");
#else
    return env->Find("BUS_ADDRESS", "unix:abstract=alljoyn");
#endif
}
