/**
 * @file
 *
 * This template class provides reference counting based heap allocation
 * for objects.
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
#ifndef _QCC_MANAGEDOBJ_H
#define _QCC_MANAGEDOBJ_H

#include <qcc/platform.h>

#if defined(QCC_OS_ANDROID)
// This must be included before #include <new> for building on Froyo with
// certain versions of STLPort.
#include <stdlib.h>
#endif

#include <new>

#include <stdlib.h>

#include <qcc/atomic.h>

namespace qcc {

/**
 * ManagedObj manages heap allocation and reference counting for a template parameter type T.
 * ManagedObj@<T@> allocates T and sets its reference count to 1 when it is created. Each time the
 * managed object is passed by value or otherwise copied (which is an inexpensive operation), the
 * underlying heap allocated T's reference count is incremented. Each time a ManagedObj instance
 * is destructed, the underlying T reference count is decremented. When the reference count reaches
 * zero, T itself is deallocated using delete.
 */
template <class T>
class ManagedObj {
  private:

    struct ManagedCtx {
        ManagedCtx(int32_t refCount) : refCount(refCount) { }
        int32_t refCount;
    };

    ManagedCtx* context;
    T* object;

  public:

    /** Copy constructor */
    ManagedObj<T>(const ManagedObj<T>&copyMe)
    {
        context = copyMe.context;
        object = copyMe.object;
        IncRef();
    }

    /** non-const Copy constructor needed to avoid ambiguity with ManagedObj<T>(A1& arg) constructor */
    ManagedObj<T>(ManagedObj<T>&copyMe)
    {
        context = copyMe.context;
        object = copyMe.object;
        IncRef();
    }

    /**
     * Create a copy of managed object T.
     *
     * If isDeep is true
     * Create a deep (clone) copy of a managed object.
     * A ManagedObject created using this constructor copies the underlying T
     * object and wraps it in a new ManagedObject with 1 reference.
     *
     * if isDeep is false
     * Do not make a deep copy of the managed object instead make a new reference
     * to the existing object and increment the reference counter by +1.
     *
     * @param other   ManagedObject to make a copy of.
     * @param isDeep  Specify if this is a deep (clone) copy or a normal copy
     */
    ManagedObj<T>(const ManagedObj<T>&other, bool isDeep)
    {
        if (isDeep) {
            /* Deep copy */
            const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
            context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
            context = new (context) ManagedCtx(1);
            object = new ((char*)context + offset)T(*other);
        } else {
            /* Normal copy constructor (inc ref) of existing object */
            context = other.context;
            object = other.object;
            IncRef();
        }
    }

    /** Allocate T() on the heap and set it's reference count to 1. */
    ManagedObj<T>()
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T();
    }

    /** Constructor to wrap an existing T in its managed object type */
    ManagedObj<T>(T * naked)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        object = naked;
        context = (ManagedCtx*)((char*)object - offset);
        IncRef();
    }

    /**
     * Allocate T(arg1) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     */
    template <typename A1> ManagedObj<T>(A1 & arg1)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1);
    }

    /**
     * Allocate T(arg1, arg2) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     */
    template <typename A1, typename A2> ManagedObj<T>(A1 & arg1, A2 & arg2)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2);
    }

    /**
     * Allocate T(arg1, arg2, arg3) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     */
    template <typename A1, typename A2, typename A3> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3);
    }

    /**
     * Allocate T(arg1, arg2, arg3, arg4) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     * @param arg4   Fourth arg to T constructor.
     */
    template <typename A1, typename A2, typename A3, typename A4> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3, A4 & arg4)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3, arg4);
    }

    /**
     * Allocate T(arg1, arg2, arg3, arg4, arg5) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     * @param arg4   Fourth arg to T constructor.
     * @param arg5   Fifth arg to T constructor.
     */
    template <typename A1, typename A2, typename A3, typename A4, typename A5> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3, A4 & arg4, A5 & arg5)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3, arg4, arg5);
    }

    /**
     * Allocate T(arg1, arg2, arg3, arg4, arg5, arg6) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     * @param arg4   Fourth arg to T constructor.
     * @param arg5   Fifth arg to T constructor.
     * @param arg6   Sixth arg to T constructor.
     */
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3, A4 & arg4, A5 & arg5, A6 & arg6)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3, arg4, arg5, arg6);
    }

    /**
     * Allocate T(arg1, arg2, arg3, arg4, arg5, arg6, arg7) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     * @param arg4   Fourth arg to T constructor.
     * @param arg5   Fifth arg to T constructor.
     * @param arg6   Sixth arg to T constructor.
     * @param arg7   Seventh arg to T constructor.
     */
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3, A4 & arg4, A5 & arg5, A6 & arg6, A7 & arg7)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    }

    /**
     * Allocate T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     * @param arg4   Fourth arg to T constructor.
     * @param arg5   Fifth arg to T constructor.
     * @param arg6   Sixth arg to T constructor.
     * @param arg7   Seventh arg to T constructor.
     * @param arg8   Eight arg to T constructor.
     */
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3, A4 & arg4, A5 & arg5, A6 & arg6, A7 & arg7, A8 & arg8)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    }

    /**
     * Allocate T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     * @param arg4   Fourth arg to T constructor.
     * @param arg5   Fifth arg to T constructor.
     * @param arg6   Sixth arg to T constructor.
     * @param arg7   Seventh arg to T constructor.
     * @param arg8   Eight arg to T constructor.
     * @param arg9   Ninth arg to T constructor.
     */
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3, A4 & arg4, A5 & arg5, A6 & arg6, A7 & arg7, A8 & arg8, A9 & arg9)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    }

    /**
     * Allocate T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10) on the heap and set it's reference count to 1.
     * @param arg1   First arg to T constructor.
     * @param arg2   Second arg to T constructor.
     * @param arg3   Third arg to T constructor.
     * @param arg4   Fourth arg to T constructor.
     * @param arg5   Fifth arg to T constructor.
     * @param arg6   Sixth arg to T constructor.
     * @param arg7   Seventh arg to T constructor.
     * @param arg8   Eight arg to T constructor.
     * @param arg9   Ninth arg to T constructor.
     * @param arg10  Tenth arg to T constructor.
     */
    template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10> ManagedObj<T>(A1 & arg1, A2 & arg2, A3 & arg3, A4 & arg4, A5 & arg5, A6 & arg6, A7 & arg7, A8 & arg8, A9 & arg9, A10 & arg10)
    {
        const size_t offset = (sizeof(ManagedCtx) + 7) & ~0x07;
        context = reinterpret_cast<ManagedCtx*>(malloc(offset + sizeof(T)));
        context = new (context) ManagedCtx(1);
        object = new ((char*)context + offset)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    }

    /**
     * ManagedObj destructor.
     * Decrement T's reference count and deallocate if zero.
     */
    ~ManagedObj<T>()
    {
        DecRef();
    }

    /**
     * Assign a ManagedObj<T> to an existing ManagedObj<T>
     * @param assignFromMe   ManagedObj<T> to copy from.
     * @return reference to this MangedObj<T>.
     */
    ManagedObj<T>& operator=(const ManagedObj<T>& assignFromMe)
    {
        /* Decrement ref of current context */
        DecRef();

        /* Reassign this Managed Obj */
        context = assignFromMe.context;
        object = assignFromMe.object;

        /* Increment the ref */
        IncRef();

        return *this;
    }

    /**
     * Equality for managed objects is whatever equality means for @<T@>
     * @param other  The other managed object to compare.
     * @return  true if the managed objects are equal.
     */
    bool operator==(const ManagedObj<T>& other) const { return (object == other.object) || (*object == *other.object); }


    /**
     * Returns true if the two managed objects managed the same object
     * @param other  The other managed object to compare.
     * @return  true if the managed objects refer to the same underlying object.
     */
    bool iden(const ManagedObj<T>& other) const { return (object == other.object); }

    /**
     * Inequality for managed objects is whatever inequality means for @<T@>
     * @param other  The other managed object to compare.
     * @return  true if the managed objects are equal.
     */
    bool operator!=(const ManagedObj<T>& other) const { return !(*this == other); }

    /**
     * Less-than for managed objects is whatever less-than means for @<T@>
     * @param other  The other managed object to compare.
     * @return  true if the managed objects are equal.
     */
    bool operator<(const ManagedObj<T>& other) const { return (object != other.object) && (*object < *other.object); }

    /**
     * Get a reference to T.
     * @return A reference to the managed object T.
     */
    T& operator*() { return *object; }

    /**
     * Get a pointer to T.
     * @return A reference to the managed object T.
     */
    T* operator->() { return object; }

    /**
     * Get a const reference to T.
     * @return A const reference to the managed object T.
     */
    const T& operator*() const { return *object; }

    /**
     * Get a const pointer to T.
     * @return A const pointer to the managed object T.
     */
    const T* operator->() const { return object; }

    /** Increment the ref count */
    void IncRef()
    {
        IncrementAndFetch(&context->refCount);
    }

    /** Decrement the ref count and deallocate if necessary. */
    void DecRef()
    {
        uint32_t refs = DecrementAndFetch(&context->refCount);
        if (0 == refs) {
            object->T::~T();
            context->ManagedCtx::~ManagedCtx();
            free(context);
        }
    }

    /**
     * Get the reference count
     */
    int32_t GetRefCount() const { return context->refCount; }
};


}

#endif
