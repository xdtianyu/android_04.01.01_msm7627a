/**
 * @file
 *
 * Implemenation of StreamPump.
 */

/******************************************************************************
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

#include <qcc/platform.h>

#include <vector>
#include <qcc/StreamPump.h>
#include <qcc/Event.h>
#include <qcc/ManagedObj.h>

#include <Status.h>

#define QCC_MODULE "STREAM"

using namespace std;
using namespace qcc;

StreamPump::StreamPump(Stream* streamA, Stream* streamB, size_t chunkSize, const char* name, bool isManaged) :
    Thread(name), streamA(streamA), streamB(streamB), chunkSize(chunkSize), isManaged(isManaged)
{
    /* Keep the object alive until Run exits */
    if (isManaged) {
        ManagedObj<StreamPump> pump(this);
        pump.IncRef();
    }
}

QStatus StreamPump::Start()
{
    QStatus status = Thread::Start();
    if ((status != ER_OK) && isManaged) {
        ManagedObj<StreamPump> pump(this);
        pump.DecRef();
    }
    return status;
}

ThreadReturn STDCALL StreamPump::Run(void* args)
{
    // TODO: Make sure streams are non-blocking

    Event& streamASrcEv = streamA->GetSourceEvent();
    Event& streamBSrcEv = streamB->GetSourceEvent();
    Event& streamASinkEv = streamA->GetSinkEvent();
    Event& streamBSinkEv = streamB->GetSinkEvent();
    size_t bToAOffset = 0;
    size_t aToBOffset = 0;
    size_t bToALen = 0;
    size_t aToBLen = 0;
    uint8_t* aToBBuf = new uint8_t[chunkSize];
    uint8_t* bToABuf = new uint8_t[chunkSize];

    QStatus status = ER_OK;
    while ((status == ER_OK) && !IsStopping()) {
        vector<Event*> checkEvents;
        vector<Event*> sigEvents;
        checkEvents.push_back((aToBOffset == aToBLen) ? &streamASrcEv : &streamBSinkEv);
        checkEvents.push_back((bToAOffset == aToBLen) ? &streamBSrcEv : &streamASinkEv);
        status = Event::Wait(checkEvents, sigEvents);
        if (status == ER_OK) {
            for (size_t i = 0; i < sigEvents.size(); ++i) {
                if (sigEvents[i] == &streamASrcEv) {
                    status = streamA->PullBytes(aToBBuf, chunkSize, aToBLen, 0);
                    if (status == ER_OK) {
                        status = streamB->PushBytes(aToBBuf, aToBLen, aToBOffset);
                        if (status != ER_OK) {
                            QCC_LogError(status, ("Stream::PushBytes failed"));
                        }
                    } else if (status == ER_NONE) {
                        status = ER_OK;
                    } else {
                        QCC_LogError(status, ("Stream::PullBytes failed"));
                    }
                } else if (sigEvents[i] == &streamBSinkEv) {
                    size_t r;
                    status = streamB->PushBytes(aToBBuf + aToBOffset, aToBLen - aToBOffset, r);
                    if (status == ER_OK) {
                        aToBOffset += r;
                    } else {
                        QCC_LogError(status, ("Stream::PushBytes failed"));
                    }
                } else if (sigEvents[i] == &streamBSrcEv) {
                    status = streamB->PullBytes(bToABuf, chunkSize, bToALen, 0);
                    if (status == ER_OK) {
                        status = streamA->PushBytes(bToABuf, bToALen, bToAOffset);
                        if (status != ER_OK) {
                            QCC_LogError(status, ("Stream::PushBytes failed"));
                        }
                    } else if (status == ER_NONE) {
                        status = ER_OK;
                    } else {
                        QCC_LogError(status, ("Stream::PullBytes failed"));
                    }
                } else if (sigEvents[i] == &streamASinkEv) {
                    size_t r;
                    status = streamA->PushBytes(bToABuf + bToAOffset, bToALen - bToAOffset, r);
                    if (status == ER_OK) {
                        bToAOffset += r;
                    } else {
                        QCC_LogError(status, ("Stream::PushBytes failed"));
                    }
                }
                if (aToBOffset == aToBLen) {
                    aToBOffset = aToBLen = 0;
                }
                if (bToAOffset == bToALen) {
                    bToAOffset = bToALen = 0;
                }
            }
        }
    }
    delete[] aToBBuf;
    delete[] bToABuf;
    if (isManaged) {
        ManagedObj<StreamPump> p(this);
        p.DecRef();
    }
    return (ThreadReturn) ER_OK;
}
