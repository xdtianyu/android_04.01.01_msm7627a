/**
 * @file
 *
 * %BufferedSource is an Source wrapper that reads the underlying (wrapped)
 * Source in chunks. It is typically used by Source consumers that want
 * to read one byte at a time effieciently.
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

#include <qcc/platform.h>

#include <assert.h>
#include <string.h>

#include <qcc/BufferedSource.h>
#include <qcc/Thread.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "STREAM"

BufferedSource::BufferedSource(Source& source, size_t bufSize, bool usePushBack)
    : source(&source),
    event(source.GetSourceEvent(), Event::IO_READ, true),
    bufSize(bufSize),
    usePushBack(usePushBack)
{
    buf = new uint8_t[usePushBack ? 2 * bufSize : bufSize];
    rdPtr = buf;
    endPtr = buf;
}

void BufferedSource::Reset(Source& source)
{
    this->source = &source;
    rdPtr = buf;
    endPtr = buf;
}


BufferedSource::~BufferedSource()
{
    delete [] buf;
}

QStatus BufferedSource::PullBytes(void* outBuf, size_t reqBytes, size_t& actualBytes, uint32_t timeout)
{
    QStatus status = ER_OK;
    char* outPtr = (char*) outBuf;
    bool bufEmpty = rdPtr == endPtr;

    while (0 < reqBytes) {
        /* Copy buffered bytes first */
        if (endPtr > rdPtr) {
            size_t b = min(reqBytes, (size_t) (endPtr - rdPtr));
            memcpy(outPtr, rdPtr, b);
            rdPtr += b;
            reqBytes -= b;
            outPtr += b;
        }

        /* Get more bytes from source if needed */
        if (0 < reqBytes) {
            if (reqBytes > bufSize) {
                /* Since caller wants more bytes than fit in our buffer, just
                   copy directly into his buffer */
                size_t rb;
                status = source->PullBytes(outPtr, reqBytes, rb, timeout);
                if (ER_OK == status) {
                    outPtr += rb;
                }
                if (ER_OK != status) {
                    status = (outPtr == outBuf) ? status : ER_OK;
                }
                break;
            } else {
                /* Get another chunk from source */
                size_t rb = 0;
                status = source->PullBytes(buf, bufSize, rb, timeout);
                if (ER_OK != status) {
                    status = (outBuf == outPtr) ? status : ER_OK;
                    break;
                } else {
                    rdPtr = (uint8_t*) buf;
                    endPtr = (uint8_t*) buf + rb;
                }
            }
        }
    }

    /* Keep event in sync with buffered data */
    if (bufEmpty && (rdPtr != endPtr)) {
        event.SetEvent();
    } else if (!bufEmpty && (rdPtr == endPtr)) {
        event.ResetEvent();
    }

    actualBytes = (outPtr - (char*)outBuf);

    return status;
}

QStatus BufferedSource::PushBack(const void* inBuf, size_t numPush)
{
    bool bufEmpty = rdPtr == endPtr;

    if (0 == numPush) {
        return ER_OK;
    } else if (numPush > bufSize) {
        return ER_FAIL;
    } else if (!usePushBack) {
        return ER_FAIL;
    }

    /* Check to see if we need to copy the bytes */
    if ((size_t)(rdPtr - buf) >= numPush) {
        rdPtr -= numPush;
    } else {
        /* Make room in buffer for push back bytes */
        if (endPtr > rdPtr) {
            memmove(buf + numPush, rdPtr, endPtr - rdPtr);
        }
        /* Copy pushback bytes and adjust */
        memcpy(buf, inBuf, numPush);
        rdPtr = buf;
        endPtr += numPush;
    }

    if (bufEmpty) {
        event.SetEvent();
    }
    return ER_OK;
}
