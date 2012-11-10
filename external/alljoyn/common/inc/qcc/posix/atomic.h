/**
 * @file
 *
 * Define atomic read-modify-write memory options
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
#ifndef _POSIX_QCC_ATOMIC_H
#define _POSIX_QCC_ATOMIC_H

#include <qcc/platform.h>

#ifdef QCC_OS_ANDROID
#include <sys/atomics.h>
#endif

#if defined(QCC_OS_DARWIN)
#include <libkern/OSAtomic.h>
#endif

namespace qcc {

#if defined(QCC_OS_ANDROID)

/**
 * Increment an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be incremented.
 * @return  New value (after increment) of *mem
 */
inline int32_t IncrementAndFetch(volatile int32_t* mem)
{
    /*
     * Androids built in __atomic_inc operation returns the previous value
     * stored in mem. This value has been changed in memory.  Since we expect
     * the value after the increment to be returned we add one to the value
     * returned by __atomic_inc.
     */
    return __atomic_inc(mem) + 1;
}

/**
 * Decrement an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be decremented.
 * @return  New value (after decrement) of *mem
 */
inline int32_t DecrementAndFetch(volatile int32_t* mem)
{
    /*
     * Androids built in __atomic_dec operation returns the previous value
     * stored in mem. This value has been changed in memory.  Since we expect
     * the value after the decrement to be returned we subtract one from the
     * value returned by __atomic_dec.
     */
    return __atomic_dec(mem) - 1;
}

#elif defined(QCC_OS_LINUX)

/**
 * Increment an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be incremented.
 * @return  New value (after increment) of *mem
 */
inline int32_t IncrementAndFetch(volatile int32_t* mem) {
    return __sync_add_and_fetch(mem, 1);
}

/**
 * Decrement an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be decremented.
 * @return  New value (afer decrement) of *mem
 */
inline int32_t DecrementAndFetch(volatile int32_t* mem) {
    return __sync_sub_and_fetch(mem, 1);
}

#elif defined(QCC_OS_DARWIN)

/**
 * Increment an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be incremented.
 * @return  New value (after increment) of *mem
 */
inline int32_t IncrementAndFetch(volatile int32_t* mem) {
    return OSAtomicIncrement32(mem);
}

/**
 * Decrement an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be decremented.
 * @return  New value (afer decrement) of *mem
 */
inline int32_t DecrementAndFetch(volatile int32_t* mem) {
    return OSAtomicDecrement32(mem);
}

#else

/**
 * Increment an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be incremented.
 * @return  New value (afer increment) of *mem
 */
int32_t IncrementAndFetch(volatile int32_t* mem);

/**
 * Decrement an int32_t and return it's new value atomically.
 *
 * @param mem   Pointer to int32_t to be decremented.
 * @return  New value (afer decrement) of *mem
 */
int32_t DecrementAndFetch(volatile int32_t* mem);

#endif

}

#endif
