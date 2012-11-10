/**
 * @file
 * org.bluez interface table definitions
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
#ifndef _ALLJOYN_BLUEZIFC_H
#define _ALLJOYN_BLUEZIFC_H

#include <qcc/platform.h>

#include <alljoyn/Message.h>


namespace ajn {
namespace bluez {

struct InterfaceDesc {
    AllJoynMessageType type;
    const char* name;
    const char* inputSig;
    const char* outSig;
    const char* argNames;
    uint8_t annotation;
};

struct InterfaceTable {
    const char* ifcName;
    const InterfaceDesc* desc;
    size_t tableSize;
};

extern const InterfaceDesc bzManagerIfcTbl[];
extern const InterfaceDesc bzAdapterIfcTbl[];
extern const InterfaceDesc bzServiceIfcTbl[];
extern const InterfaceDesc bzDeviceIfcTbl[];
extern const InterfaceTable ifcTables[];
extern const size_t ifcTableSize;

extern const char* bzBusName;
extern const char* bzMgrObjPath;
extern const char* bzManagerIfc;
extern const char* bzServiceIfc;
extern const char* bzAdapterIfc;
extern const char* bzDeviceIfc;


} // namespace bluez
} // namespace ajn

#endif
