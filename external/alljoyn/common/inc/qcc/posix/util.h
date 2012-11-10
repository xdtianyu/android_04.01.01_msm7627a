#ifndef _ERUTIL_LINUX_H
#define _ERUTIL_LINUX_H
/**
 * @file
 *
 * This file provides platform specific macros for Linux
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

#if defined(QCC_OS_DARWIN)
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>
#else
#include <endian.h>
#endif

/*
 * Make the target's endianness known to the rest of the code in portable manner.
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
/**
 * This target is little endian
 */
#define QCC_TARGET_ENDIAN QCC_LITTLE_ENDIAN
#else
/**
 * This target is big endian
 */
#define QCC_TARGET_ENDIAN QCC_BIG_ENDIAN
#endif


#if defined __GLIBC__

/*
 * GLibC and BSD both define nice helper macros for converting between
 * big/little endian and the host machine's endian.  Unfortunately, for some
 * of those macros, they use slightly different names.  This unifies the names
 * to match BSD names (also used by Android'd Bionic).
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN

#define letoh16(_val) (_val)
#define letoh32(_val) (_val)
#define letoh64(_val) (_val)

#define betoh16(_val) __bswap_16(_val)
#define betoh32(_val) __bswap_32(_val)
#define betoh64(_val) __bswap_64(_val)

#else

#define letoh16(_val) __bswap_16(_val)
#define letoh32(_val) __bswap_32(_val)
#define letoh64(_val) __bswap_64(_val)

#define betoh16(_val) (_val)
#define betoh32(_val) (_val)
#define betoh64(_val) (_val)

#endif

// Undefine GlibC's versions the macros to help prevent writing non-portable code.
#undef le16toh
#undef le32toh
#undef le64toh

#undef be16toh
#undef be32toh
#undef be64toh

// Again follow Android's Bionic example for compatability below
#define __swap16(_val) __bswap_16(_val)
#define __swap32(_val) __bswap_32(_val)
#define __swap64(_val) __bswap_64(_val)

#endif


#if defined(QCC_OS_DARWIN)

/**
 * Swap bytes to convert endianness of a 16 bit integer
 */
#define EndianSwap16(_val) (OSSwapConstInt16(_val))

/**
 * Swap bytes to convert endianness of a 32 bit integer
 */
#define EndianSwap32(_val) (OSSwapConstInt32(_val))

/**
 * Swap bytes to convert endianness of a 64 bit integer
 */
#define EndianSwap64(_val) (OSSwapConstInt64(_val))


#else
/**
 * Swap bytes to convert endianness of a 16 bit integer
 */
#define EndianSwap16(_val) (__swap16(_val))

/**
 * Swap bytes to convert endianness of a 32 bit integer
 */
#define EndianSwap32(_val) (__swap32(_val))

/**
 * Swap bytes to convert endianness of a 64 bit integer
 */
#define EndianSwap64(_val) (__swap64(_val))

#endif

#define ER_DIR_SEPARATOR  "/"


#endif
