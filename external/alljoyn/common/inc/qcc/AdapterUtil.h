/**
 * @file
 *
 * This file defines the Network Interface adapter utility class.
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

#ifndef _QCC_ADAPTERUTIL_H
#define _QCC_ADAPTERUTIL_H

#include <qcc/platform.h>

#include <assert.h>
#include <vector>

#include <qcc/Mutex.h>
#include <qcc/NetInfo.h>

#include <Status.h>

namespace qcc {

/**
 * AdapterUtil class abstracts the OS-specific Network Interface
 * adapter enumeration.
 */
class AdapterUtil {
  public:
    /// iterator typedef.
    typedef std::vector<qcc::NetInfo>::iterator iterator;

    /// const_iterator typedef.
    typedef std::vector<qcc::NetInfo>::const_iterator const_iterator;

  private:
    static AdapterUtil* singleton;         ///< Singleton reference.

    std::vector<qcc::NetInfo> interfaces;  ///< List of network interface data.

    Mutex lock;                            ///< Mutex to prevent updates while reading.

    bool isMultihomed;                     ///< True iff the host is multi-homed (has more than one network adapter)

    AdapterUtil(void) { ForceUpdate(); }

    ~AdapterUtil(void);

  public:

    /**
     * Method for getting the singleton reference.  This will create the
     * instance if it is not already created.
     *
     * @return  Pointer to the AdapterUtil instance.
     */
    static AdapterUtil* GetAdapterUtil(void)
    {
        if (singleton == NULL) {
            singleton = new AdapterUtil();
        }
        return singleton;
    }

    /**
     * Acquire a mutex on the data.  This must be acquired before
     * accessing the data via iterators.
     *
     * @return  Indication of pass or failure.
     */
    QStatus GetLock(void) { return lock.Lock(); }

    /**
     * Release the lock on the data.  This must be called after
     * completion of reading the data.
     *
     * @return  Indication of pass or failure.
     */
    QStatus ReleaseLock(void) { return lock.Unlock(); }

    /**
     * Get an iterator referencing the first interface.  The caller must have
     * previously acquired a lock before accessing any of the network
     * interface data.
     *
     * @return  A iterator pointing to the begining of the collection.
     */
    const_iterator Begin(void) const { return interfaces.begin(); }

    /**
     * Get an iterator referencing the one past the last interface.  The
     * caller must have previously acquired a lock before accessing any
     * of the network interface data.
     *
     * @return  A iterator pointing to the end of the collection.
     */
    const_iterator End(void) const { return interfaces.end(); }

    /**
     * Force an update to the list of network interfaces.  This will acquire a
     * lock, thus blocking any code that calls AdaperUtil::GetLock().
     * This is to prevent users of this object from encountering problems with
     * corrupt iterators.
     *
     * @return  Indication of success or failure.
     */
    QStatus ForceUpdate(void);

    /**
     * Return whether this host is multi-homed.
     *
     * @return  True iff the host is multi-homed.
     */
    bool IsMultihomed(void) const { return isMultihomed; }

    /**
     * Return whether the interface corresponding the
     * specified address is a VPN.
     *
     * @return  True iff the address is a VPN.
     */
    bool IsVPN(IPAddress addr);

#if !defined(NDEBUG)
    void Shutdown(void) { delete this; }
#endif

};

}

#endif
