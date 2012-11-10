#ifndef __stl_config__android_h
#define __stl_config__android_h

/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define _STLP_PLATFORM "Android"

// The Android C library is mostly glibc-like
#define _STLP_USE_GLIBC 1

// ...and Unix-like.
#define _STLP_UNIX 1

// Have pthreads support.
#define _PTHREADS

// Don't have native <cplusplus> headers
#define _STLP_HAS_NO_NEW_C_HEADERS 1

// Don't use wchar.h etc
//#define _STLP_NO_WCHAR_T 1

// Don't have (working) native wide character support.
#define _STLP_NO_NATIVE_WIDE_FUNCTIONS 1

// Don't use mbstate_t, define our own.
//#define _STLP_NO_NATIVE_MBSTATE_T 1

// No (proper) wide stream support in Android
#define _STLP_NO_NATIVE_WIDE_STREAMS 1

// C library is in the global namespace.
#define _STLP_VENDOR_GLOBAL_CSTD 1

// Don't have underlying local support.
#undef _STLP_REAL_LOCALE_IMPLEMENTED

// No pthread_spinlock_t in Android
#define _STLP_DONT_USE_PTHREAD_SPINLOCK 1

// Little endian platform.
#define _STLP_LITTLE_ENDIAN 1

// No <exception> headers
#define _STLP_NO_EXCEPTION_HEADER 1

// No need to define our own namespace
#define _STLP_NO_OWN_NAMESPACE 1

// Need this to define STLport's own bad_alloc class (which won't be
// thrown in any case)
#define _STLP_NEW_DONT_THROW_BAD_ALLOC 1

// Use __new_alloc instead of __node_alloc, so we don't need static functions.
#define _STLP_USE_SIMPLE_NODE_ALLOC 1

// Don't use extern versions of range errors, so we don't need to
// compile as a library.
#define _STLP_USE_NO_EXTERN_RANGE_ERRORS 1

// The system math library doesn't have long double variants, e.g
// sinl, cosl, etc
#define _STLP_NO_VENDOR_MATH_L 1

// Define how to include our native headers.
#define _STLP_NATIVE_HEADER(header) <libstdc++/include/header>
#define _STLP_NATIVE_C_HEADER(header) <../include/header>
#define _STLP_NATIVE_CPP_C_HEADER(header) <libstdc++/include/header>
#define _STLP_NATIVE_OLD_STREAMS_HEADER(header) <libstdc++/include/header>
#define _STLP_NATIVE_CPP_RUNTIME_HEADER(header) <libstdc++/include/header>

#endif /* __stl_config__android_h */
