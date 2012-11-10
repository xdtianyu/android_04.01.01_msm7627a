/**
 * @file
 *
 * Define a class that abstracts Windows mutexs.
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
#ifndef _OS_QCC_MUTEX_H
#define _OS_QCC_MUTEX_H

#include <qcc/platform.h>

#include <qcc/String.h>
#include <qcc/windows/utility.h>

#include <Status.h>


namespace qcc {

#ifndef NDEBUG
#define MUTEX_CONTEXT __FILE__, __LINE__
#else
#define MUTEX_CONTEXT
#endif

/**
 * The Windows implementation of a Mutex abstraction class.
 */
class Mutex {
  public:

    /**
     * Constructor
     */
    Mutex() : initialized(false) { Init(); }

    /**
     * Destructor
     */
    ~Mutex();

    /**
     * Acquires a lock on the mutex.  If another thread is holding the lock,
     * then this function will block until the other thread has released its
     * lock.
     *
     * @return  ER_OK if the lock was acquired, ER_OS_ERROR if the underlying
     *          OS reports an error.
     */
    QStatus Lock(const char* file, uint32_t line);
    QStatus Lock(void);

    /**
     * Releases a lock on the mutex.  This will only release a lock for the
     * current thread if that thread was the one that aquired the lock in the
     * first place.
     *
     * @return  ER_OK if the lock was acquired, ER_OS_ERROR if the underlying
     *          OS reports an error.
     */
    QStatus Unlock(const char* file, uint32_t line);
    QStatus Unlock(void);

    /**
     * Attempt to acquire a lock on a mutex. If another thread is holding the lock
     * this function return false otherwise the lock is acquired and the function returns true.
     *
     * @return  True if the lock was acquired.
     */
    bool TryLock();

    /**
     * Mutex copy constructor creates a new mutex.
     */
    Mutex(const Mutex& other) { Init(); }

    /**
     * Mutex assignment operator.
     */
    Mutex& operator=(const Mutex& other) { Init(); return *this; }

  private:
    bool initialized;
    CRITICAL_SECTION mutex; ///< Mutex variable.
    void Init();            ///< initialize a mutex

};

} /* namespace */

#endif
