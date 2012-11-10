/**
 * @file XmlElement.h
 *
 * Extremely simple XML Parser/Generator.
 *
 */

/******************************************************************************
 * Copyright 2010 - 2011, Qualcomm Innovation Center, Inc.
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
 *
 ******************************************************************************/

#ifndef _STLCONTAINER_H
#define _STLCONTAINER_H





#if defined(QCC_OS_ANDROID)

#include <ext/hash_map>
#include <ext/hash_set>
/*
 * For android use hash_map and hash_set since the STL list does not compile with gcc 4.6
 * with the gnu++0x flag which is required for unordered_map and unordered_set.
 */
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_multimap;
using __gnu_cxx::hash_set;
using __gnu_cxx::hash;

#define unordered_map hash_map
#define unordered_multimap hash_multimap
#define unordered_set hash_set
#define STL_NAMESPACE_PREFIX __gnu_cxx

#else
#include <unordered_map>
#include <unordered_set>
#define STL_NAMESPACE_PREFIX std

#if defined _MSC_VER && _MSC_VER == 1500

/*
 * For MSVC2008 unordered_map, unordered_multimap, unordered_set, and hash
 * are found in the tr1 libraries while in new version of MSVC and in GNU
 * the libraries are all found in std namespace.
 */
using std::tr1::unordered_map;
using std::tr1::unordered_multimap;
using std::tr1::unordered_set;
using std::tr1::hash;

#else

using std::unordered_map;
using std::unordered_multimap;
using std::unordered_set;
using std::hash;

#endif

#endif

#endif

