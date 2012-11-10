/**
 * @file
 *
 * Define a scatter-gather list class.
 */

/******************************************************************************
 *
 *
 * Copyright 2009-2012, Qualcomm Innovation Center, Inc.
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
#ifndef _QCC_SCATTERGATHERLIST_H
#define _QCC_SCATTERGATHERLIST_H

#include <qcc/platform.h>

#include <algorithm>
#include <assert.h>
#include <list>
#include <string.h>

#include <qcc/Debug.h>
#include <qcc/SocketTypes.h>

#define QCC_MODULE  "NETWORK"

namespace qcc {

/**
 * Classes that need to add an internal buffer to a scatter-gather list should
 * inherit this class and implement the interface functions.
 */
class ScatterGatherList {
  public:
    /// Define the type of dereferenced iterators.
    typedef IOVec type_value;

    /// iterator typedef.
    typedef std::list<IOVec>::iterator iterator;

    /// const_iterator typedef.
    typedef std::list<IOVec>::const_iterator const_iterator;

  private:
    std::list<IOVec> sg;     ///< Collection of buffers and associated buffer lengths.

    size_t maxDataSize;         ///< Maximum data that can be held in the SG buffers.
    size_t dataSize;            ///< Amount of data currently held in the SG buffers.

  public:
    ScatterGatherList(void) : maxDataSize(0), dataSize(0) { }

    /**
     * Copy constructor.  Note: this only copies the pointers to the buffers
     * and buffer lengths; it does not copy the data in the buffers.
     *
     * @param other The ScatterGatherList to intialize from.
     */
    ScatterGatherList(const ScatterGatherList& other) :
        sg(other.sg), maxDataSize(other.maxDataSize), dataSize(other.dataSize)
    { }

    /**
     * Assignment operator.  Note: this only copies the pointers to the buffers
     * and buffer lengths; it does not copy the data in the buffers.
     *
     * @param other The ScatterGatherList to intialize from.
     *
     * @return  Reference to *this.
     */
    ScatterGatherList& operator=(const ScatterGatherList& other)
    {
        QCC_DbgTrace(("ScatterGatherList::operator=(other) [maxDataSize = %u  dataSize = %u]",
                      other.maxDataSize, other.dataSize));
        sg = other.sg;
        maxDataSize = other.maxDataSize;
        dataSize = other.dataSize;
        return *this;
    }

    /**
     * Add a buffer to the scatter-gather list.
     *
     * @param buffer    Buffer to be added to the list.
     * @param length    Length of the buffer.
     */
    void AddBuffer(void* buffer, size_t length);

    /**
     * @overload
     * The underlying C structure is used for both const and non-const SG
     * lists so this version casts away the const-ness.  Be careful to not mix
     * up sending and receiving.
     *
     * @param buffer    Buffer to be added to the list.
     * @param length    Length of the buffer.
     */
    void AddBuffer(const void* buffer, size_t length)
    {
        AddBuffer(const_cast<void*>(buffer), length);
    }

    /**
     * Add the entries from one list to this list.
     *
     * @param other The SG list to get entries from.
     */
    void AddSG(const ScatterGatherList& other) { AddSG(other.Begin(), other.End()); }

    /**
     * @overload
     * This version takes const iterators an so can be used to add just a
     * subset of another SG list.
     *
     * @param begin Beginning of the SG list to get buffers from.
     * @param end   End of the SG list to get buffers from.
     */
    void AddSG(const_iterator begin, const_iterator end)
    {
        const_iterator iter;
        for (iter = begin; iter != end; ++iter) {
            AddBuffer(iter->buf, iter->len);
        }
    }

    /**
     * Get an interator referencing the beginning of the scatter-gather list.
     *
     * @return  An interator referencing the beginning of the scatter-gather list.
     */
    iterator Begin(void) { return sg.begin(); }

    /**
     * Get a const interator referencing the beginning of the scatter-gather
     * list.
     *
     * @return  A const interator referencing the beginning of the
     *          scatter-gather list.
     */
    const_iterator Begin(void) const { return sg.begin(); }

    /**
     * Get an interator referencing the end of the scatter-gather list.
     *
     * @return  An interator referencing the end of the scatter-gather list.
     */
    iterator End(void) { return sg.end(); }

    /**
     * Get a const interator referencing the end of the scatter-gather list.
     *
     * @return  A const interator referencing the end of the scatter-gather list.
     */
    const_iterator End(void) const { return sg.end(); }

    /**
     * This removes the specifed entry from the scatter-gather list.
     *
     * @param i     Iterator referencing the element to be removed.
     */
    void Erase(iterator& i)
    {
        maxDataSize -= i->len;
        dataSize -= (std::min)(dataSize, (size_t)(i->len));
        sg.erase(i);
    }

    /**
     * Clear out the list of SG entries.
     */
    void Clear(void) {
        QCC_DbgTrace(("ScatterGatherList::Clear()"));
        sg.clear();
        maxDataSize = 0;
        dataSize = 0;
    }

    /**
     * Return the number of entries in the scatter-gather list.
     *
     * @return  Number of elements in the scatter-gather list.
     */
    size_t Size(void) const { return sg.size(); }

    /**
     * Get the amount of space used among the buffers in the SG list.  This
     * value is only valid if SetDataSize was used or the data was copied from
     * a source with a known data size.
     *
     * @return  Number of octets of actual data in the SG list.
     */
    size_t DataSize(void) const { return dataSize; }

    /**
     * Get the amount of space available among the buffers in the SG list.
     *
     * @return  Number of octets of available space in the SG list.
     */
    size_t MaxDataSize(void) const { return maxDataSize; }

    /**
     * Sets the amount of space used by data in the SG List.  The copy
     * operations depend on this value being accurate.
     *
     * @param newSize   Number of octets used.
     */
    void SetDataSize(size_t newSize) {
        QCC_DbgTrace(("ScatterGatherList::SetDataSize(newSize = %u)", newSize));
        dataSize = newSize;
    }

    /**
     * Increments the amount of space used by data in the SG List.  The copy
     * operations depend on this value being accurate.
     *
     * @param newSize   Number of octets used.
     */
    void IncDataSize(size_t increment) {
        QCC_DbgTrace(("ScatterGatherList::IncDataSize(increment = %u)", increment));
        dataSize += increment;
    }

    /**
     * Copies the data from another SG list into this list.  The underlying
     * implementation uses memmove to avoid problems with overlapping memory.
     * Only the lesser of this->DataSize() or other.DataSize() bytes will be
     * copied.
     *
     * @param other The SG list to copy data from;
     * @param limit [optional] Limits the number octets to copy (default is all).
     *
     * @return  Number of octets copied.
     */
    size_t CopyDataFrom(const ScatterGatherList& other, size_t limit = ~0)
    {
        return CopyDataFrom(other.Begin(), other.End(), (std::min)(limit, other.DataSize()));
    }

    /**
     * @overload
     * This version uses iterators instead of a SG list.
     *
     * @param begin Beginig of the SG list to copy.
     * @param end   End of the SG list to copy.
     * @param limit This limits the amount of data to be copied (defaults to all data).
     *
     * @return  Number of octets copied.
     */
    size_t CopyDataFrom(const_iterator begin, const_iterator end, size_t limit = ~0);

    /**
     * Copies data from the SG list to a buffer.  It will copy at most bufSize
     * or this->DataSize() octets.
     *
     * @param buf       Buffer where data will be copied to.
     * @param bufSize   Size of the buffer.
     *
     * @return  Number of octets copied.
     */
    size_t CopyToBuffer(void* buf, size_t bufSize) const
    {
        const_iterator src(sg.begin());
        uint8_t* pos = reinterpret_cast<uint8_t*>(buf);
        size_t copyCnt = (std::min)(bufSize, dataSize);

        QCC_DbgTrace(("ScatterGatherList::CopyToBuffer(*buf, bufSize = %u)", bufSize));
        QCC_DbgPrintf(("bufSize = %u  dataSize = %u  copyCnt = %u", bufSize, dataSize, copyCnt));

        assert(buf != NULL);

        while (copyCnt > 0 && src != sg.end()) {
            size_t copyLen = (std::min)(copyCnt, (size_t)(src->len));
            memmove(pos, src->buf, copyLen);
            if (copyLen == src->len) {
                ++src;
            }
            copyCnt -= copyLen;
            QCC_DbgPrintf(("Copied %u bytes (%u left)", copyLen, copyCnt));
            QCC_DbgLocalData(pos, copyLen);
            pos += copyLen;
        }
        return (std::min)(bufSize, dataSize);
    }


    /**
     * Copies data to the SG list from a buffer.  It will copy at most bufSize
     * or this->DataSize() octets.
     *
     * @param buf       Buffer where data will be copied from.
     * @param bufSize   Size of the buffer.
     *
     * @return  Number of octets copied.
     */
    size_t CopyFromBuffer(const void* buf, size_t bufSize)
    {
        iterator dest(sg.begin());
        const uint8_t* pos = reinterpret_cast<const uint8_t*>(buf);
        size_t copyCnt = (std::min)(bufSize, maxDataSize);

        QCC_DbgTrace(("ScatterGatherList::CopyFromBuffer(*buf, bufSize = %u)", bufSize));
        QCC_DbgPrintf(("bufSize = %u  maxDataSize = %u  copyCnt = %u", bufSize, maxDataSize, copyCnt));

        assert(buf != NULL);

        while (copyCnt > 0 && dest != sg.end()) {
            size_t copyLen = (std::min)(copyCnt, static_cast<size_t>(dest->len));
            memmove(dest->buf, pos, copyLen);
            if (copyLen == dest->len) {
                ++dest;
            }
            copyCnt -= copyLen;
            QCC_DbgPrintf(("Copied %u bytes (%u left)", copyLen, copyCnt));
            pos += copyLen;
        }
        dataSize = static_cast<size_t>(pos - reinterpret_cast<const uint8_t*>(buf));
        return dataSize;
    }


    /**
     * This alters the SG list by removing buffers and/or adjusting the
     * pointer to the resulting first buffer such that the MaxDataSize will be
     * reduced by the specifed amount.
     *
     * @param trim  Number of octets to remove from the begining of the SG list.
     *
     * @return  Number of octets that could not be trimmed.
     */
    size_t TrimFromBegining(size_t trim)
    {
        size_t orig = trim;
        maxDataSize -= (std::min)(maxDataSize, trim);
        dataSize -= (std::min)(dataSize, trim);

        QCC_DbgTrace(("ScatterGatherList::TrimFromBegining(trim = %u) [maxDataSize = %u  dataSize = %u]",
                      trim, maxDataSize, dataSize));

        while (trim > 0 && sg.begin() != sg.end()) {
            if (trim >= sg.begin()->len) {
                trim -= sg.begin()->len;
                sg.erase(sg.begin());
            } else {
                sg.begin()->buf = reinterpret_cast<char*>(sg.begin()->buf) + trim;
                sg.begin()->len -= (uint32_t)trim;
                trim = 0;
            }
        }
        return (orig - trim);
    }

};

}

#if defined(QCC_OS_GROUP_POSIX)
#include <qcc/posix/ScatterGatherList.h>
#elif defined(QCC_OS_GROUP_WINDOWS)
#include <qcc/windows/ScatterGatherList.h>
#else
#error No OS GROUP defined.
#endif

#undef QCC_MODULE
#endif
