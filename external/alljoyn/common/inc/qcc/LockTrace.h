#ifndef _QCC_LOCKTRACE_H
#define _QCC_LOCKTRACE_H
/**
 * @file
 *
 * This file defines a class for debugging thread deadlock problems
 */

/******************************************************************************
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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
#include <qcc/String.h>
#include <qcc/Debug.h>
#include <deque>
#include <map>

namespace qcc {

class Thread;

class LockTrace {

  public:

    LockTrace(Thread* thread) : thread(thread) { }

    /**
     * Called when a mutex has been acquired
     *
     * @param mutex  The mutex that was acquired
     * @param file   The file that acquired the mutex
     * @param line   The line in the file that acquired the mutex
     */
    void Acquired(qcc::Mutex* mutex, qcc::String file, uint32_t line);

    /**
     * Called when a thread is waiting to acquire a mutex
     *
     * @param mutex  The mutex that was acquired
     * @param file   The file that acquired the mutex
     * @param line   The line in the file that acquired the mutex
     */
    void Waiting(qcc::Mutex* mutex, qcc::String file, uint32_t line);

    /**
     * Called when a mutex is about to be released
     *
     * @param mutex  The mutex that was released
     * @param file   The file that released the mutex
     * @param line   The line in the file that released the mutex
     */
    void Releasing(qcc::Mutex* mutex, qcc::String file, uint32_t line);

    /**
     * Dump lock trace information for a specific thread
     */
    void Dump();

  private:

    struct Info {
        Info(const qcc::Mutex* mutex, qcc::String file, uint32_t line) : mutex(mutex), file(file), line(line) { }
        const qcc::Mutex* mutex;
        qcc::String file;
        uint32_t line;
    };

    Thread* thread;

    std::deque<Info> queue;

};

};

#endif
