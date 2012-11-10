/**
 * @file
 * This file implements functions to read or write the Persistent GUID from
 * the file PersistentGUID in the System Home Directory
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

#ifndef _PERSISTGUID_H
#define _PERSISTGUID_H

#include <qcc/platform.h>

#include <qcc/String.h>
#include <qcc/FileStream.h>
#include <qcc/GUID.h>

#include "Status.h"

namespace ajn {

const qcc::String GUIDFileName = qcc::String("/PersistentGUID");

/* Retrieve the Persistent GUID from the file PersistentGUID in the System Home Directory */
QStatus GetPersistentGUID(qcc::GUID128& guid);

/* Set the Persistent GUID in the file PersistentGUID in the System Home Directory */
QStatus SetPersistentGUID(qcc::GUID128 guid);

}

#endif
