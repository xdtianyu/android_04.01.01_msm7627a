/**
 * @file
 *
 * Define a class for accessing environment variables.
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
#ifndef _QCC_ENVIRON_H
#define _QCC_ENVIRON_H

#include <qcc/platform.h>
#include <map>
#include <qcc/String.h>
#include <qcc/Stream.h>
#include <Status.h>

namespace qcc {

/**
 * Abstract encapsulation of the system environment variables.
 */
class Environ {
  public:
    /** Environment variable const_iterator */
    typedef std::map<qcc::String, qcc::String>::const_iterator const_iterator;

    /**
     * Create a new envionment (useful when launching other programs).
     */
    Environ(void) { }

    /**
     * Return a pointer to the Environ instance that applies to the running application.
     *
     * @return  Pointer to the environment variable singleton.
     */
    static Environ* GetAppEnviron(void);

    /**
     * Return a specific environment variable
     */
    qcc::String Find(const qcc::String& key, const char* defaultValue = NULL);

    /**
     * Return a specific environment variable
     */
    qcc::String Find(const char* key, const char* defaultValue = NULL) { return Find(qcc::String(key), defaultValue); }

    /**
     * Preload environment variables with the specified prefix
     */
    void Preload(const char* keyPrefix);

    /**
     * Add an environment variable
     */
    void Add(const qcc::String& key, const qcc::String& value);

    /**
     * Parse a env settings file.
     * Each line is expected to be of the form <key> = <value>
     */
    QStatus Parse(Source& source);

    /**
     * Return an iterator to the first environment variable.
     *
     * @return  A const_iterator pointing to the beginning of the environment variables.
     */
    const_iterator Begin(void) const { return vars.begin(); }

    /**
     * Return an iterator to one past the last environment variable.
     *
     * @return  A const_iterator pointing to the end of the environment variables.
     */
    const_iterator End(void) const { return vars.end(); }

    /**
     * Return the number of entries in the environment.
     *
     * @return  Number of entries in the environment.
     */
    size_t Size(void) const { return vars.size(); }

  private:
    std::map<qcc::String, qcc::String> vars;    ///< Environment variable storage.

};

}

#endif
