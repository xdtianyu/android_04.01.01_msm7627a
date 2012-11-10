/**
 * @file
 *
 * Utility functions for Windows
 */

/******************************************************************************
 *
 *
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

#include <qcc/platform.h>

#include <windows.h>

#include <qcc/windows/utility.h>

#define QCC_MODULE "UTILITY"


void strerror_r(uint32_t errCode, char* ansiBuf, uint16_t ansiBufSize)
{
    LPTSTR unicodeBuf = 0;

    uint32_t len = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &unicodeBuf,
        ansiBufSize - 1, NULL);
    memset(ansiBuf, 0, ansiBufSize);
    WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)unicodeBuf, -1, ansiBuf, (int)len, NULL, NULL);
    LocalFree(unicodeBuf);
}
