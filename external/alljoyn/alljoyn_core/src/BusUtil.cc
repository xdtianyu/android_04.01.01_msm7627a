/**
 * @file
 *
 * This file implements utilities functions
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

#include <ctype.h>

#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>

#include "BusUtil.h"


#define QCC_MODULE "ALLJOYN"


#define MAX_NAME_LEN 256

using namespace qcc;
using namespace std;

namespace ajn {

bool IsLegalUniqueName(const char* str)
{
    if (!str) {
        return false;
    }
    const char* p = str;

    char c = *p++;
    if (c != ':' || !(isalnum(*p) || (*p == '-') || (*p == '_'))) {
        return false;
    }
    p++;

    size_t periods = 0;
    while ((c = *p++)) {
        if (!isalnum(c) && (c != '-') && (c != '_')) {
            if ((c != '.') || (*p == '.') || (*p == 0)) {
                return false;
            }
            periods++;
        }
    }
    return (periods > 0) && ((p - str) <= MAX_NAME_LEN);
}


bool IsLegalBusName(const char* str)
{
    if (!str) {
        return false;
    }
    if (*str == ':') {
        return IsLegalUniqueName(str);
    }
    const char* p = str;
    size_t periods = 0;
    char c = *p++;
    /* Must begin with an alpha character, underscore, or hyphen */
    if (!isalpha(c) && (c != '_') && (c != '-')) {
        return false;
    }
    while ((c = *p++) != 0) {
        if (!isalnum(c) && (c != '_') && (c != '-')) {
            if ((c != '.') || (*p == '.') || (*p == 0) || isdigit(*p)) {
                return false;
            }
            periods++;
        }
    }
    return (periods > 0) && ((p - str) <= MAX_NAME_LEN);
}


bool IsLegalObjectPath(const char* str)
{
    if (!str) {
        return false;
    }
    /* Must begin with slash */
    char c = *str++;
    if (c != '/') {
        return false;
    }
    while ((c = *str++) != 0) {
        if (!isalnum(c) && (c != '_')) {
            if ((c != '/') || (*str == '/') || (*str == 0)) {
                return false;
            }
        }
    }
    return true;
}


bool IsLegalInterfaceName(const char* str)
{
    if (!str) {
        return false;
    }
    const char* p = str;

    /* Must begin with an alpha character or underscore */
    char c = *p++;
    if (!isalpha(c) && (c != '_')) {
        return false;
    }
    size_t periods = 0;
    while ((c = *p++) != 0) {
        if (!isalnum(c) && (c != '_')) {
            if ((c != '.') || (*p == '.') || (*p == 0)) {
                return false;
            }
            periods++;
        }
    }
    return (periods > 0) && ((p - str) <= MAX_NAME_LEN);
}


bool IsLegalErrorName(const char* str)
{
    return IsLegalInterfaceName(str);
}


bool IsLegalMemberName(const char* str)
{
    if (!str) {
        return false;
    }
    const char* p = str;
    char c = *p++;

    if (!isalpha(c) && (c != '_')) {
        return false;
    }
    while ((c = *p++) != 0) {
        if (!isalnum(c) && (c != '_')) {
            return false;
        }
    }
    return (p - str) <= MAX_NAME_LEN;
}


qcc::String BusNameFromObjPath(const char* str)
{
    qcc::String path;

    if (IsLegalObjectPath(str) && (str[1] != 0)) {
        char c = *str++;
        do {
            if (c == '/') {
                c = '.';
            }
            path.push_back(c);
        } while ((c = *str++) != 0);
    }
    return path;
}

}
