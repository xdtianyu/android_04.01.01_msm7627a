/**
 * @file
 * AllJoyn-Daemon module property database classes
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

//
// Even though we're not actually using a map, we have to include it (in
// Windows) so STLPort can figure out where std::nothrow_t comes from.
//
#include <map>

#include "PropertyDB.h"

namespace ajn {

class _PropertyMap {
  public:
    _PropertyMap() { }
    virtual ~_PropertyMap() { }

    void Set(qcc::String name, qcc::String value);
    qcc::String Get(qcc::String name);

  private:
    std::unordered_map<qcc::StringMapKey, qcc::String> m_properties;
};

void _PropertyMap::Set(qcc::String name, qcc::String value)
{
    m_properties[name] = value;
}

qcc::String _PropertyMap::Get(qcc::String name)
{
    std::unordered_map<qcc::StringMapKey, qcc::String>::const_iterator i = m_properties.find(name);
    if (i == m_properties.end()) {
        return "";
    }
    return i->second;
}

_PropertyDB::~_PropertyDB()
{
    for (std::unordered_map<qcc::StringMapKey, _PropertyMap*>::iterator i = m_modules.begin(); i != m_modules.end(); ++i) {
        delete i->second;
    }
}

void _PropertyDB::Set(qcc::String module, qcc::String name, qcc::String value)
{
    std::unordered_map<qcc::StringMapKey, _PropertyMap*>::const_iterator i = m_modules.find(module);
    if (i == m_modules.end()) {
        _PropertyMap* p = new _PropertyMap;
        p->Set(name, value);
        m_modules[module] = p;
        return;
    }
    i->second->Set(name, value);
}

qcc::String _PropertyDB::Get(qcc::String module, qcc::String name)
{
    std::unordered_map<qcc::StringMapKey, _PropertyMap*>::const_iterator i = m_modules.find(module);
    if (i == m_modules.end()) {
        return "";
    }
    return i->second->Get(name);
}

} // namespace ajn {
