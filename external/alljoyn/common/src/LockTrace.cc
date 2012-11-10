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
#include <qcc/Thread.h>
#include <qcc/LockTrace.h>

#define QCC_MODULE "LOCK_TRACE"

using namespace qcc;

void LockTrace::Acquired(qcc::Mutex* mutex, qcc::String file, uint32_t line)
{
    queue.push_front(Info(mutex, file, line));
}

void LockTrace::Waiting(qcc::Mutex* mutex, qcc::String file, uint32_t line)
{
#ifndef NDEBUG
    QCC_DbgPrintf(("Lock %u requested at %s:%u may be already held by another thread", mutex, file.c_str(), line));
    Thread::DumpLocks();
#endif
}

void LockTrace::Releasing(qcc::Mutex* mutex, qcc::String file, uint32_t line)
{
    if (queue.empty() || (queue.front().mutex != mutex)) {
        // Check lock is actually held
        std::deque<Info>::iterator iter = queue.begin();
        while (iter != queue.end()) {
            if (iter->mutex == mutex) {
                break;
            }
            ++iter;
        }
        if (iter == queue.end()) {
            QCC_LogError(ER_FAIL, ("Lock %u released %s:%u but was not held", mutex, file.c_str(), line));
        } else {
            // Lock released in different order than acquired
            QCC_LogError(ER_WARNING, ("Lock %u released %s:%u in different order than acquired", mutex, file.c_str(), line));
            Dump();
            queue.erase(iter);
        }
    } else {
        queue.pop_front();
    }
}

void LockTrace::Dump()
{
    if (!queue.empty()) {
        std::deque<Info>::iterator iter = queue.begin();
        QCC_DbgPrintf(("Lock trace for thread %s", thread->GetName()));
        while (iter != queue.end()) {
            QCC_DbgPrintf(("   Lock %u held by %s:%u", iter->mutex, iter->file.c_str(), iter->line));
            ++iter;
        }
    }
}
