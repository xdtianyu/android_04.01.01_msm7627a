#ifndef _STUNATTRIBUTEDATA_H
#define _STUNATTRIBUTEDATA_H
/**
 * @file
 *
 * This file defines the DATA STUN message attribute.
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
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
#error Only include StunAttributeData.h in C++ code.
#endif

#include <assert.h>
#include <qcc/platform.h>
#include <qcc/ScatterGatherList.h>
#include <StunAttributeBase.h>
#include <types.h>
#include "Status.h"

using namespace qcc;

/** @internal */
#define QCC_MODULE "STUN_ATTRIBUTE"


/**
 * Data STUN attribute class.
 */
class StunAttributeData : public StunAttribute {
  private:
    ScatterGatherList data;     ///< Application data.

  public:
    /**
     * This constructor just sets the attribute type to STUN_ATTR_DATA.
     */
    StunAttributeData(void) : StunAttribute(STUN_ATTR_DATA, "DATA") { }

    /**
     * This constructor sets the attribute type to STUN_ATTR_DATA and
     * intializes the data pointer and data size.
     *
     * @param dataPtr   Pointer to the data to be sent.
     * @param dataSize  Size of the data to be sent.
     */
    StunAttributeData(const void* dataPtr, size_t dataSize) :
        StunAttribute(STUN_ATTR_DATA, "DATA")
    {
        data.AddBuffer(dataPtr, dataSize);
        data.SetDataSize(dataSize);
    }

    /**
     * This constructor sets the attribute type to STUN_ATTR_DATA and
     * intializes the data pointer and data size.
     *
     * @param sg    SG list to be sent.
     */
    StunAttributeData(const ScatterGatherList& sg) :
        StunAttribute(STUN_ATTR_DATA, "DATA")
    {
        data.AddSG(sg);
        data.IncDataSize(sg.DataSize());
    }

    QStatus Parse(const uint8_t*& buf, size_t& bufSize);
    QStatus RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const;
    size_t RenderSize(void) const
    {
        return StunAttribute::RenderSize() + (static_cast<size_t>(-AttrSize()) & 0x3);
    }

    uint16_t AttrSize(void) const { return data.DataSize(); }

    /**
     * This function returns a pointer to the data in this message.  For
     * incoming messagees this pointer refers to the block of memory
     * containing the receive buffer.
     *
     * @return  Constant reference to a SG list.
     */
    const ScatterGatherList& GetData(void) const { return data; }

    /**
     * This adds a buffer to the data that will be encapsulated in a STUN
     * attribute for transfer via a TURN server.
     *
     * @param dataPtr    Pointer to the data.
     * @param dataSize   Size of the data.
     */
    void AddBuffer(const uint8_t* dataPtr, uint16_t dataSize)
    {
        assert(dataPtr == NULL);
        data.AddBuffer(dataPtr, dataSize);
        data.IncDataSize(dataSize);
    }
};


#undef QCC_MODULE
#endif
