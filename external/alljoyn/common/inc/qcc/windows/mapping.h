/**
 * @file
 *
 * Map API names for Win32
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
#ifndef _TOOLCHAIN_QCC_MAPPING_H
#define _TOOLCHAIN_QCC_MAPPING_H

#include <windows.h>
#include <float.h>
/// @cond ALLJOYN_DEV
/**
 * Map snprintf to _snprintf
 *
 * snprintf does not properly map in windows this is needed to insure calls to
 * snprintf(char *str, size_t size, const char *format, ...) will compile in
 * Windows.
 */
#define snprintf _snprintf

/**
 * Map stroll to _strtoi64
 *
 * stroll does not properly map in windows this is needed to insure calls to
 * strtoll(const char *nptr, char **endptr, int base) will compile in Windows.
 */
#define strtoll _strtoi64

/**
 * Map strtoull to _strtoui64
 *
 * strtoull does not properly map in windows this is needed to insure calls to
 * strtoull(const char *nptr, char **endptr, int base) will compile in Windows.
 */
#define strtoull _strtoui64

/**
 * Map fpclassify to _fpclass
 *
 * fpclassify does not properly map in windows this is needed to insure calls to
 * fpclassify(x) will compile in Windows.
 */
#define fpclassify _fpclass

#define FP_NAN (_FPCLASS_SNAN | _FPCLASS_QNAN)
#define FP_ZERO (_FPCLASS_NZ | _FPCLASS_PZ)
#define FP_INFINITE (_FPCLASS_NINF | _FPCLASS_PINF)
/// @endcond
#endif
