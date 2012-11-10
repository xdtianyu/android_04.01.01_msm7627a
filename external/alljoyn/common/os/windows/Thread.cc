/**
 * @file
 *
 * Define a class that abstracts Windows process/threads.
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

#include <algorithm>
#include <assert.h>
#include <process.h>
#include <map>

#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Debug.h>
#include <qcc/Thread.h>
#include <qcc/Mutex.h>

#include <Status.h>

using namespace std;

/** @internal */
#define QCC_MODULE "THREAD"

namespace qcc {

static uint32_t started = 0;
static uint32_t running = 0;
static uint32_t stopped = 0;


/** Maximum number of milliseconds to wait between calls to select to check for thread death */
static const uint32_t MAX_SELECT_WAIT_MS = 10000;

/** Lock that protects global list of Threads and their handles */
Thread::ThreadListLock Thread::threadListLock;
Mutex* Thread::ThreadListLock::m_mutex = NULL;
bool Thread::ThreadListLock::m_destructed = false;

/** Thread list */
map<ThreadHandle, Thread*> Thread::threadList;

QStatus Sleep(uint32_t ms) {
    ::sleep(ms);
    return ER_OK;
}

Thread* Thread::GetThread()
{
    Thread* ret = NULL;
    unsigned int id = GetCurrentThreadId();

    /* Find thread on threadList */
    threadListLock.Lock();
    map<ThreadHandle, Thread*>::const_iterator iter = threadList.find((ThreadHandle)id);
    if (iter != threadList.end()) {
        ret = iter->second;
    }
    threadListLock.Unlock();
    /*
     * If the current thread isn't on the list, then create an external (wrapper) thread
     */
    if (NULL == ret) {
        char name[32];
        snprintf(name, sizeof(name), "external%d", id);
        /* TODO @@ Memory leak */
        ret = new Thread(name, NULL, true);
    }

    return ret;
}

const char* Thread::GetThreadName()
{
    Thread* thread = NULL;
    unsigned int id = GetCurrentThreadId();

    /* Find thread on threadList */
    threadListLock.Lock();
    map<ThreadHandle, Thread*>::const_iterator iter = threadList.find((ThreadHandle)id);
    if (iter != threadList.end()) {
        thread = iter->second;
    }
    threadListLock.Unlock();
    /*
     * If the current thread isn't on the list, then don't create an external (wrapper) thread
     */
    if (thread == NULL) {
        return "external";
    }

    return thread->GetName();
}

void Thread::CleanExternalThreads()
{
    threadListLock.Lock();
    map<ThreadHandle, Thread*>::iterator it = threadList.begin();
    while (it != threadList.end()) {
        if (it->second->isExternal) {
            delete it->second;
            threadList.erase(it++);
        } else {
            ++it;
        }
    }
    threadListLock.Unlock();
}

Thread::Thread(qcc::String name, Thread::ThreadFunction func, bool isExternal) :
#ifndef NDEBUG
    lockTrace(this),
#endif
    state(isExternal ? RUNNING : DEAD),
    isStopping(false),
    function(isExternal ? NULL : func),
    handle(isExternal ? GetCurrentThread() : 0),
    exitValue(NULL),
    arg(NULL),
    threadId(isExternal ? GetCurrentThreadId() : 0),
    listener(NULL),
    isExternal(isExternal),
    alertCode(0),
    auxListeners(),
    auxListenersLock()
{
    /* qcc::String is not thread safe.  Don't use it here. */
    funcName[0] = '\0';
    strncpy(funcName, name.c_str(), sizeof(funcName));
    funcName[sizeof(funcName) - 1] = '\0';

    /*
     * External threads are already running so just add them to the thread list.
     */
    if (isExternal) {
        threadListLock.Lock();
        threadList[(ThreadHandle)threadId] = this;
        threadListLock.Unlock();
    }
    QCC_DbgHLPrintf(("Thread::Thread() [%s,%x]", funcName, this));
}

Thread::~Thread(void)
{
    if (IsRunning()) {
        Stop();
        Join();
    } else if (!isExternal && handle) {
        CloseHandle(handle);
        handle = 0;
        ++stopped;
    }
    QCC_DbgHLPrintf(("Thread::~Thread() [%s,%x] started:%d running:%d stopped:%d", GetName(), this, started, running, stopped));
}


ThreadInternalReturn STDCALL Thread::RunInternal(void* threadArg)
{
    Thread* thread(reinterpret_cast<Thread*>(threadArg));

    assert(thread != NULL);
    assert(thread->state = STARTED);
    assert(!thread->isExternal);

    if (thread->state != STARTED) {
        return 0;
    }

    ++started;

    /* Add this Thread to list of running threads */
    threadListLock.Lock();
    threadList[(ThreadHandle)thread->threadId] = thread;
    thread->state = RUNNING;
    threadListLock.Unlock();

    if (NULL == thread->handle) {
        QCC_DbgPrintf(("Starting thread had NULL thread handle, exiting..."));
    }

    /* Start the thread if it hasn't been stopped and is fully initialized */
    if (!thread->isStopping && NULL != thread->handle) {
        QCC_DbgPrintf(("Starting thread: %s", thread->funcName));
        ++running;
        thread->exitValue  = thread->Run(thread->arg);
        --running;
        QCC_DbgPrintf(("Thread function exited: %s --> %p", thread->funcName, thread->exitValue));
    }

    unsigned retVal = (unsigned)thread->exitValue;
    uint32_t threadId = thread->threadId;

    thread->state = STOPPING;
    thread->stopEvent.ResetEvent();

    /*
     * The following block must be in its own scope because microsoft STL's ITERATOR_DEBUG_LEVEL==2
     * falsely concludes that the iterator defined below (without its own scope) is still in scope
     * when auxListener's destructor runs from within ~Thread. Go Microsoft.
     */
    {
        /* Call aux listeners before main listener since main listner may delete the thread */
        thread->auxListenersLock.Lock();

        ThreadListeners::iterator it = thread->auxListeners.begin();
        while (it != thread->auxListeners.end()) {
            ThreadListener* listener = *it;
            listener->ThreadExit(thread);
            it = thread->auxListeners.upper_bound(listener);
        }
        thread->auxListenersLock.Unlock();
    }

    /*
     * Call thread exit callback if specified. Note that ThreadExit may dellocate the thread so the
     * members of thread may not be accessed after this call
     */
    if (thread->listener) {
        thread->listener->ThreadExit(thread);
    }

    /* This also means no QCC_DbgPrintf as they try to get context on the current thread */

    /* Remove this Thread from list of running threads */
    threadListLock.Lock();
    threadList.erase((ThreadHandle)threadId);
    threadListLock.Unlock();

    _endthreadex(retVal);
    return retVal;
}

static const uint32_t stacksize = 80 * 1024;

QStatus Thread::Start(void* arg, ThreadListener* listener)
{
    QStatus status = ER_OK;

    /* Check that thread can be started */
    if (isExternal) {
        status = ER_EXTERNAL_THREAD;
    } else if (isStopping) {
        status = ER_THREAD_STOPPING;
    } else if (IsRunning()) {
        status = ER_THREAD_RUNNING;
    }

    if (status != ER_OK) {
        QCC_LogError(status, ("Thread::Start() [%s]", funcName));
    } else {
        QCC_DbgTrace(("Thread::Start() [%s]", funcName));
        /*  Reset the stop event so the thread doesn't start out alerted. */
        stopEvent.ResetEvent();
        /* Create OS thread */
        this->arg = arg;
        this->listener = listener;

        state = STARTED;
        handle = reinterpret_cast<HANDLE>(_beginthreadex(NULL, stacksize, RunInternal, this, 0, &threadId));
        if (handle == 0) {
            state = DEAD;
            isStopping = false;
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Creating thread"));
        }
    }
    return status;
}


QStatus Thread::Stop(void)
{
    /* Cannot stop external threads */
    if (isExternal) {
        QCC_LogError(ER_EXTERNAL_THREAD, ("Cannot stop an external thread"));
        return ER_EXTERNAL_THREAD;
    } else if ((state == DEAD) || (state == INITIAL)) {
        QCC_DbgPrintf(("Thread::Stop() thread is dead [%s]", funcName));
        return ER_OK;
    } else {
        QCC_DbgTrace(("Thread::Stop() %x [%s]", handle, funcName));
        isStopping = true;
        return stopEvent.SetEvent();
    }
}

QStatus Thread::Alert()
{
    if (state == DEAD) {
        return ER_DEAD_THREAD;
    }
    QCC_DbgTrace(("Thread::Alert() [%s:%srunning]", funcName, IsRunning() ? " " : " not "));
    return stopEvent.SetEvent();
}

QStatus Thread::Alert(uint32_t alertCode)
{
    this->alertCode = alertCode;
    if (state == DEAD) {
        return ER_DEAD_THREAD;
    }
    QCC_DbgTrace(("Thread::Alert() [%s run: %s]", funcName, IsRunning() ? "true" : "false"));
    return stopEvent.SetEvent();
}

QStatus Thread::Join(void)
{

    assert(!isExternal);

    QStatus status = ER_OK;
    bool self = (threadId == GetCurrentThreadId());

    QCC_DbgTrace(("Thread::Join() [%s run: %s]", funcName, IsRunning() ? "true" : "false"));

    /*
     * Nothing to join if the thread is dead
     */
    if (state == DEAD) {
        QCC_DbgPrintf(("Thread::Join() thread is dead [%s]", funcName));
        isStopping = false;
        return ER_DEAD_THREAD;
    }
    /*
     * There is a race condition where the underlying OS thread has not yet started to run. We need
     * to wait until the thread is actually running before we can join it.
     */
    while (state == STARTED) {
        ::sleep(5);
    }

    QCC_DbgPrintf(("[%s - %x] %s thread %x [%s - %x]",
                   self ? funcName : GetThread()->funcName,
                   self ? threadId : GetThread()->threadId,
                   self ? "Closing" : "Joining",
                   threadId, funcName, threadId));
    /*
     * make local copy of handle so it is not deleted if two threads are in
     * Join at the same time.
     */
    HANDLE goner = handle;
    if (goner) {
        DWORD ret;
        handle = 0;
        if (self) {
            ret = WAIT_OBJECT_0;
        } else {
            ret = WaitForSingleObject(goner, INFINITE);
        }
        if (ret != WAIT_OBJECT_0) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Joining thread: %d", ret));
        }
        CloseHandle(goner);
        ++stopped;
    }
    isStopping = false;
    state = DEAD;
    QCC_DbgPrintf(("%s thread %s", self ? "Closed" : "Joined", funcName));
    return status;
}

void Thread::AddAuxListener(ThreadListener* listener)
{
    auxListenersLock.Lock();
    auxListeners.insert(listener);
    auxListenersLock.Unlock();
}

void Thread::RemoveAuxListener(ThreadListener* listener)
{
    auxListenersLock.Lock();
    ThreadListeners::iterator it = auxListeners.find(listener);
    if (it != auxListeners.end()) {
        auxListeners.erase(it);
    }
    auxListenersLock.Unlock();
}

ThreadReturn STDCALL Thread::Run(void* arg)
{
    assert(NULL != function);
    return (*function)(arg);
}

}    /* namespace */
