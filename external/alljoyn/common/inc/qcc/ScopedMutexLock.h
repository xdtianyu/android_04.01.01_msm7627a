#ifndef _ALLJOYN_SCOPEDMUTEXLOCK_H
#define _ALLJOYN_SCOPEDMUTEXLOCK_H
/**
 * @file
 *
 * This file defines a class that will ensure a mutex is unlocked when the method returns
 *
 */

/******************************************************************************
 * Copyright 2009-2012, Qualcomm Innovation Center, Inc.
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

#include <qcc/Mutex.h>

namespace qcc {

/**
 * An implementation of a scoped mutex lock class.
 */
class ScopedMutexLock {
  public:

    /**
     * Constructor
     *
     * @param lock The lock we want to manage
     */
    ScopedMutexLock(Mutex& lock)
        : lock(lock),
        file(NULL),
        line(0)
    {
        lock.Lock();
    }

    /**
     * Constructor
     *
     * @param lock The lock we want to manage
     * @param file The file for the lock trace
     * @param file The line for the lock trace
     */
    ScopedMutexLock(Mutex& lock, const char* file, uint32_t line)
        : lock(lock),
        file(file),
        line(line)
    {
        lock.Lock(file, line);
    }

    ~ScopedMutexLock()
    {
        if (file) {
            lock.Unlock(file, line);
        } else {
            lock.Unlock();
        }
    }

  private:
    Mutex& lock;
    const char* file;
    const uint32_t line;
};

}

#endif
