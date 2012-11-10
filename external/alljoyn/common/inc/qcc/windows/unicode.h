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

// Visual Studio uses a 2 byte wchar_t
/// @cond ALLJOYN_DEV
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
/// @endcond
#endif
