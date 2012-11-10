/**
 * @file
 * ProximityScanner is the platform agnostic header file that the ProximityScanner includes in
 * its code
 * It contains the platform specific header files which are then included in the source
 * depending on the platform
 */

/******************************************************************************
 * Copyright 2012, Qualcomm Innovation Center, Inc.
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
#ifndef _ALLJOYN_PROXIMITYSCANNER_H
#define _ALLJOYN_PROXIMITYSCANNER_H

#include <qcc/platform.h>

#if defined(QCC_OS_GROUP_POSIX) || (QCC_OS_GROUP_ANDROID)
#include <posix/ProximityScanner.h>
#elif defined(QCC_OS_GROUP_WINDOWS)
#include <windows/ProximityScanner.h>
#else
#error No OS GROUP defined.
#endif

#endif
