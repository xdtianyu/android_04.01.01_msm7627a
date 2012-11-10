/**
 * @file
 * This file provides access to AllJoyn library version and build information.
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
#ifndef _ALLJOYN_VERSION_H
#define _ALLJOYN_VERSION_H

#include <qcc/platform.h>

/** Macro to compute the version number into a single value. */
#define GenerateVersionValue(_arch, _api, _rel) (((_arch) << 24) | ((_api) << 16) | (_rel))

/** Macro to extract the architecture level from unified version value. */
#define GetVersionArch(_ver) (((_ver) >> 24) & 0xff)

/** Macro to extract the API level from unified version value. */
#define GetVersionAPILevel(_ver) (((_ver) >> 16) & 0xff)

/** Macro to extract the release from unified version value. */
#define GetVersionRelease(_ver) ((_ver) & 0xffff)

namespace ajn {
const char* GetVersion();        /**< Gives the version of AllJoyn Library */
const char* GetBuildInfo();      /**< Gives build information of AllJoyn Library */
uint32_t GetNumericVersion();  /**< Gives the version of AllJoyn Library as a single number */
};

#endif
