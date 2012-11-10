/**
 * @file
 *
 * Configuration helper
 */

/******************************************************************************
 * Copyright 2012, Qualcomm Innovation Center, Inc.
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

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/StringSource.h>
#include <qcc/StringUtil.h>

#include <Status.h>

#include "DaemonConfig.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;
using namespace ajn;

DaemonConfig* DaemonConfig::singleton = NULL;

DaemonConfig::DaemonConfig() : config(NULL)
{
}

DaemonConfig::~DaemonConfig()
{
    delete config;
}

DaemonConfig* DaemonConfig::Load(qcc::Source& configSrc)
{
    if (!singleton) {
        singleton = new DaemonConfig();
    }
    XmlParseContext xmlParseCtx(configSrc);

    if (singleton->config) {
        delete singleton->config;
        singleton->config = NULL;
    }

    QStatus status = XmlElement::Parse(xmlParseCtx);
    if (status == ER_OK) {
        singleton->config = xmlParseCtx.DetachRoot();
    } else {
        delete singleton;
        singleton = NULL;
    }
    return singleton;
}

DaemonConfig* DaemonConfig::Load(const char* configXml)
{
    qcc::StringSource src(configXml);
    return Load(src);
}

uint32_t DaemonConfig::Get(const char* key, uint32_t defaultVal)
{
    return StringToU32(Get(key), 10, defaultVal);
}

qcc::String DaemonConfig::Get(const char* key, const char* defaultVal)
{
    qcc::String path = key;
    std::vector<const XmlElement*> elems = config->GetPath(path);
    if (elems.size() > 0) {
        size_t pos = path.find_first_of('@');
        if (pos != String::npos) {
            return elems[0]->GetAttribute(path.substr(pos + 1));
        } else {
            return elems[0]->GetContent();
        }
    }
    return defaultVal ? defaultVal : "";
}

std::vector<qcc::String> DaemonConfig::GetList(const char* key)
{
    std::vector<qcc::String> result;
    std::vector<const XmlElement*> elems = config->GetPath(key);
    for (size_t i = 0; i < elems.size(); ++i) {
        result.push_back(elems[i]->GetContent());
    }
    return result;
}
