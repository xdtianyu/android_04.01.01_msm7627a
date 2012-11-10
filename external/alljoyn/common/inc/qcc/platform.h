#ifndef _QCC_PLATFORM_H
#define _QCC_PLATFORM_H
/**
 * @file
 *
 * This file just wraps including actual OS and toolchain specific header
 * files depding on the OS group setting.
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

#if defined(QCC_OS_GROUP_POSIX)
#include <qcc/posix/platform_types.h>
#include <qcc/posix/unicode.h>
#elif defined(QCC_OS_GROUP_WINDOWS)
#include <qcc/windows/platform_types.h>
#include <qcc/windows/unicode.h>
#include <qcc/windows/mapping.h>
#else
#error No OS GROUP defined.
#endif

#if defined(__GNUC__)

#if (__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define QCC_DEPRECATED(func) func __attribute__((deprecated)) /**< mark a function as deprecated in gcc. */
#else
#define QCC_DEPRECATED(func) func /**< not all gcc versions support the deprecated attribute. */
#endif

#elif defined(_MSC_VER)

#define QCC_DEPRECATED(func) __declspec(deprecated) func /**< mark a function as deprecated in msvc. */

#else // Some unknown compiler

#define QCC_DEPRECATED(func); /**< mark a function as deprecated. */

#endif // Compiler type

#endif // _QCC_PLATFORM_H
