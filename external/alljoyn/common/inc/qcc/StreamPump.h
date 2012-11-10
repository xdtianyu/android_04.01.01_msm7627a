/**
 * @file
 *
 * StreamPump moves data bidirectionally between two Streams.
 */

/******************************************************************************
 *
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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

#ifndef _QCC_STREAMPUMP_H
#define _QCC_STREAMPUMP_H

#include <qcc/platform.h>
#include <qcc/Stream.h>
#include <qcc/Thread.h>
#include <Status.h>

namespace qcc {

class StreamPump : public qcc::Thread {
  public:

    /** Construct a bi-directional stream pump */
    StreamPump(Stream* streamA, Stream* streamB, size_t chunkSize, const char* name = "pump", bool isManaged = false);

    /** Destructor */
    virtual ~StreamPump()
    {
        delete streamA;
        delete streamB;
    }

    /**
     * Start the data pump.
     *
     * @return ER_OK if successful.
     */
    QStatus Start();

  protected:

    /**
     *  Worker thread used to move data from streamA to streamB
     *
     * @param arg  unused.
     */
    ThreadReturn STDCALL Run(void* arg);

  private:

    StreamPump(const StreamPump& other) : chunkSize(0), isManaged(false) { }
    StreamPump& operator=(const StreamPump& other) { return *this; }

    Stream* streamA;
    Stream* streamB;
    const size_t chunkSize;
    const bool isManaged;
};

}  /* namespace */

#endif
