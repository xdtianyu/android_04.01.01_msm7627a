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

#ifndef _PROPERTYDB_H
#define _PROPERTYDB_H

#include <qcc/platform.h>
#include <qcc/ManagedObj.h>
#include <qcc/String.h>
#include <qcc/StringMapKey.h>

namespace ajn {

class _PropertyMap;

class _PropertyDB {
  public:
    _PropertyDB() { }
    virtual ~_PropertyDB();

    void Set(qcc::String module, qcc::String name, qcc::String value);
    qcc::String Get(qcc::String module, qcc::String name);

  private:
    std::unordered_map<qcc::StringMapKey, _PropertyMap*> m_modules;
};

/**
 * Managed object wrapper for property database class.
 */
typedef qcc::ManagedObj<_PropertyDB> PropertyDB;


} // namespace ajn

#endif // _PROPERTYDB_H
