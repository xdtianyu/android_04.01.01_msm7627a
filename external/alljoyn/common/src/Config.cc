/**
 * @file
 *
 * This file implements methods from the ERConfig class.
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
#include <map>
#include <stdio.h>

#include <qcc/Debug.h>
#include <qcc/Config.h>
#include <qcc/Environ.h>
#include <qcc/FileStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include <Status.h>

#define QCC_MODULE "CONFIG"

using namespace std;
using namespace qcc;

Config::Config(void)
{
    /** Config file name */

    String iniFileResolved = "ER_INI.dat";

    Environ* env = Environ::GetAppEnviron();

    String dir = env->Find("splicehomedir");
    if (dir.empty()) {
        dir = env->Find("SPLICEHOMEDIR");
    }

#if !defined(NDEBUG)
    // Allow testing using a config file in the current directory.
    // Env var value is insignificant, just existence.
    if (env->Find("SPLICECONFIGINCURRENTDIR").empty() && !dir.empty()) {
        iniFileResolved = dir + "/" + iniFileResolved;
    }
#endif

    FileSource iniSource(iniFileResolved);

    if (!iniSource.IsValid()) {
        QCC_LogError(ER_NONE, ("Unable to open config file %s", iniFileResolved.c_str()));
        // use defaults...
        nameValuePairs["STUNTURN_GATHER_PACING_INTERVAL_MSEC"] = "500";
        nameValuePairs["STUNTURN_SERVER_IP_ADDRESS"] = "10.4.108.55";
        nameValuePairs["STUNTURN_SERVER_UDP_PORT"] = "3478";
        nameValuePairs["STUNTURN_SERVER_TCP_PORT"] = "3478";
        // ...
    } else {
        String line;
        while (ER_OK == iniSource.GetLine(line)) {
            size_t pos = line.find_first_of(';');
            if (String::npos != pos) {
                line = line.substr(0, pos);
            }
            pos = line.find_first_of('=');
            if (String::npos != pos && (line.length() >= pos + 2)) {
                String key = Trim(line.substr(0, pos));
                String val = Trim(line.substr(pos + 1, String::npos));
                nameValuePairs[key] = val;
            }
            line.clear();
        }
    }
}
