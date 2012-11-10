/**
 * @file
 *
 * This file implements methods from the Environ class.
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

#include <qcc/platform.h>

#include <map>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#include <qcc/Debug.h>
#include <qcc/Environ.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#define QCC_MODULE "ENVIRON"

using namespace std;
using namespace qcc;

#if defined(QCC_OS_DARWIN)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;   // For Linux, this is all that's needed to access
                         // environment variables.
#endif

Environ* Environ::GetAppEnviron(void)
{
    static Environ* env = NULL;      // Environment variable singleton.
    if (env == NULL) {
        env = new Environ();
    }
    return env;
}

qcc::String Environ::Find(const qcc::String& key, const char* defaultValue)
{
    qcc::String val;
    if (vars.count(key) == 0) {
        char* val = getenv(key.c_str());
        if (val) {
            vars[key] = val;
        }
    }
    val = vars[key];
    if (val.empty() && defaultValue) {
        val = defaultValue;
    }
    return val;
}

void Environ::Preload(const char* keyPrefix)
{
    size_t prefixLen = strlen(keyPrefix);
    for (char** var = environ; *var != NULL; ++var) {
        char* p = *var;
        if (strncmp(p, keyPrefix, prefixLen) == 0) {
            size_t nameLen = prefixLen;
            while (p[nameLen] != '=') {
                ++nameLen;
            }
            qcc::String key(p, nameLen);
            Find(key, NULL);
        }
    }
}

void Environ::Add(const qcc::String& key, const qcc::String& value)
{
    vars[key] = value;
}


QStatus Environ::Parse(Source& source)
{
    QStatus status = ER_OK;
    while (ER_OK == status) {
        qcc::String line;
        status = source.GetLine(line);
        if (ER_OK == status) {
            size_t endPos = line.find_first_of('#');
            if (qcc::String::npos != endPos) {
                line = line.substr(0, endPos);
            }
            size_t eqPos = line.find_first_of('=');
            if (qcc::String::npos != eqPos) {
                qcc::String key = Trim(line.substr(0, eqPos));
                qcc::String val = Trim(line.substr(eqPos + 1));
                vars[key] = val;
                setenv(key.c_str(), val.c_str(), 1);
            }
        }
    }
    return (ER_NONE == status) ? ER_OK : status;
}
