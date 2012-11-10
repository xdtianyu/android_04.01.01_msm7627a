#ifndef _ALLJOYN_PROTECTED_KEYSTORE_LISTENER_H
#define _ALLJOYN_PROTECTED_KEYSTORE_LISTENER_H
/**
 * @file
 * This file defines a wrapper class for ajn::KeyStoreListener that protects against asynchronous
 * deregistration of the listener instance.
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
#ifndef __cplusplus
#error Only include ProtectedKeyStoreListener.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/Mutex.h>
#include <qcc/String.h>
#include <qcc/Thread.h>

#include <alljoyn/KeyStoreListener.h>

#include <Status.h>

namespace ajn {


/**
 * This class adds a level of indirection to an AuthListener so the actual AuthListener can
 * asynchronously be set or removed safely. If the
 */
class ProtectedKeyStoreListener : public KeyStoreListener {
  public:

    ProtectedKeyStoreListener(KeyStoreListener* kslistener) : listener(kslistener), refCount(0) { }
    /**
     * Virtual destructor for derivable class.
     */
    virtual ~ProtectedKeyStoreListener() {
        lock.Lock(MUTEX_CONTEXT);
        /*
         * Clear the current listener to prevent any more calls to this listener.
         */
        this->listener = NULL;
        /*
         * Poll and sleep until the current listener is no longer in use.
         */
        while (refCount) {
            lock.Unlock(MUTEX_CONTEXT);
            qcc::Sleep(10);
            lock.Lock(MUTEX_CONTEXT);
        }
        /*
         * Now set the new listener
         */
        this->listener = listener;
        lock.Unlock(MUTEX_CONTEXT);
    }

    /**
     * Simply wraps the call of the same name to the inner KeyStoreListener
     */
    QStatus LoadRequest(KeyStore& keyStore)
    {
        QStatus status = ER_FAIL;
        lock.Lock(MUTEX_CONTEXT);
        KeyStoreListener* listener = this->listener;
        ++refCount;
        lock.Unlock(MUTEX_CONTEXT);
        if (listener) {
            status = listener->LoadRequest(keyStore);
        }
        lock.Lock(MUTEX_CONTEXT);
        --refCount;
        lock.Unlock(MUTEX_CONTEXT);
        return status;
    }

    /**
     * Simply wraps the call of the same name to the inner KeyStoreListener
     */
    QStatus StoreRequest(KeyStore& keyStore)
    {
        QStatus status = ER_FAIL;
        lock.Lock(MUTEX_CONTEXT);
        KeyStoreListener* listener = this->listener;
        ++refCount;
        lock.Unlock(MUTEX_CONTEXT);
        if (listener) {
            status = listener->StoreRequest(keyStore);
        }
        lock.Lock(MUTEX_CONTEXT);
        --refCount;
        lock.Unlock(MUTEX_CONTEXT);
        return status;
    }

  private:

    /*
     * The inner listener that is being protected.
     */
    KeyStoreListener* listener;

    /*
     * Reference count so we know when the inner listener is no longer in use.
     */
    qcc::Mutex lock;
    int32_t refCount;
};
}
#endif