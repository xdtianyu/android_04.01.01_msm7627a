/**
 * @file
 * this file helps handle differences in wchar_t on different OSs
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
#ifndef _PLATFORM_UNICODE_H
#define _PLATFORM_UNICODE_H

/// @cond ALLJOYN_DEV
#if __SIZEOF_WCHAR_T__ == 4
// GCC normally defines a 4-byte wchar_t
/**
 * If wchar_t is defined as 4-bytes this will convert UTF8 to wchar_t
 */
#define ConvertUTF8ToWChar ConvertUTF8toUTF32

/**
 * if wchar_t is defined as 4-bytes this will convert wchar_t to UTF8
 */
#define ConvertWCharToUTF8 ConvertUTF32toUTF8

/**
 * WideUTF is defined as a 4-byte container
 */
#define WideUTF UTF32
#else

// GCC will define a 2 byte wchar_t when running under windows or if given the
// -fshort-wchar option.
/**
 * If wchar_t is defined as 2-bytes this will convert UTF8 to wchar_t
 */
#define ConvertUTF8ToWChar ConvertUTF8toUTF16

/**
 * if wchar_t is defined as 2-bytes this will convert wchar_t to UTF8
 */
#define ConvertWCharToUTF8 ConvertUTF16toUTF8

/**
 * WideUTF is defined as a 2-byte container
 */
#define WideUTF UTF16
#endif
/// @endcond
#endif
