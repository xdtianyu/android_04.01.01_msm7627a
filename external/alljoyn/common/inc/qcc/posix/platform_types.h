#ifndef _QCC_PLATFORM_TYPES_H
#define _QCC_PLATFORM_TYPES_H
/**
 * @file
 *
 * This file defines basic types for posix platforms.
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

#if defined(QCC_OS_DARWIN)
#include <AvailabilityMacros.h>
#endif
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

namespace qcc {
typedef int SocketFd;      /**< Socket file descriptor type. */
}

#endif
