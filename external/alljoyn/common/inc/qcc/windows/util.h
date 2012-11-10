#ifndef _QCC_UTIL_WINDOWS_H
#define _QCC_UTIL_WINDOWS_H
/**
 * @file
 *
 * This file provides platform specific macros for Windows
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

/* Windows only runs on little endian machines (for now?) */

#include <stdlib.h>

/**
 * This target is little endian
 */
#define QCC_TARGET_ENDIAN QCC_LITTLE_ENDIAN

/*
 * Define some endian conversion macros to be compatible with posix macros.
 * Macros with the _same_ names are available on BSD (and Android Bionic)
 * systems (and with _similar_ names on GLibC based systems).
 *
 * Don't bother with a version of those macros for big-endian targets for
 * Windows.
 */

#define htole16(_val) (_val)
#define htole32(_val) (_val)
#define htole64(_val) (_val)

#define htobe16(_val) _byteswap_ushort(_val)
#define htobe32(_val) _byteswap_ulong(_val)
#define htobe64(_val) _byteswap_uint64(_val)

#define letoh16(_val) (_val)
#define letoh32(_val) (_val)
#define letoh64(_val) (_val)

#define betoh16(_val) _byteswap_ushort(_val)
#define betoh32(_val) _byteswap_ulong(_val)
#define betoh64(_val) _byteswap_uint64(_val)


/**
 * Swap bytes to convert endianness of a 16 bit integer
 */
#define EndianSwap16(_val) (_byteswap_ushort(_val))

/**
 * Swap bytes to convert endianness of a 32 bit integer
 */
#define EndianSwap32(_val) (_byteswap_ulong(_val))

/**
 * Swap bytes to convert endianness of a 64 bit integer
 */
#define EndianSwap64(_val) (_byteswap_uint64(_val))


#define ER_DIR_SEPARATOR  "\\"


#endif
