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
#ifndef AJTESTCOMMON_H
#define AJTESTCOMMON_H

#include <qcc/String.h>

/*
 * this header file contains a functions that can be used to replace common
 * actions in the test code.
 */
namespace ajn {

/**
 * Obtain the default connection arg for the OS the test is run on.
 * If running on on windows this should be "tcp:addr=127.0.0.1,port=9956"
 * If running on a unix variant this should be "unix:abstract=alljoyn"
 *
 * The environment variable BUS_ADDRESS is specified it will be used in place
 * of the default address
 *
 * @return a qcc::String containing the default connection arg
 */
qcc::String getConnectArg();

}
#endif //AJTESTCOMMON_H
