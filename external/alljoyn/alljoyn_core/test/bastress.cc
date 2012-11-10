/**
 * @file
 * Bundled daemon bus attachment stress test
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
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <vector>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/Environ.h>
#include <qcc/Util.h>
#include <qcc/Thread.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/version.h>

#include <Status.h>

#define QCC_MODULE "ALLJOYN"
#define THREAD_COUNT 5

using namespace std;
using namespace qcc;
using namespace ajn;

static int count = 0;

class ThreadClass : public Thread {

  public:
    ThreadClass(char*name) : Thread(name), name(name) { }

  protected:
    qcc::ThreadReturn STDCALL Run(void* arg) {

        BusAttachment*b1 = new BusAttachment(name.c_str(), true);
        QStatus status =  b1->Start();
        /* Get env vars */
        Environ* env = Environ::GetAppEnviron();

        /* Force bundled daemon */
        qcc::String connectArgs = env->Find("BUS_ADDRESS", "null:");
        status = b1->Connect(connectArgs.c_str());

        char buf[256];
        sprintf(buf, "Thread.i%d", 100 * qcc::Rand8());
        status = b1->RequestName(name.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE);
        if (status != ER_OK) {
            QCC_LogError(status, ("RequestName(%s) failed.", name.c_str()));
        }
        /* Begin Advertising the well-known name */
        status = b1->AdvertiseName(name.c_str(), TRANSPORT_ANY);
        if (ER_OK != status) {
            QCC_LogError(status, ("Could not advertise (%s)", name.c_str()));
        }

        BusObject bo(*b1, "/org/cool");
        b1->RegisterBusObject(bo);
        b1->UnregisterBusObject(bo);


        delete b1;
        b1 = NULL;

        return this;
    }

  private:
    String name;

};


/** Main entry point */
int main(int argc, char**argv)
{
    QStatus status = ER_OK;
    static int count = 0;

    while (1) {

        ThreadClass*t[THREAD_COUNT];;

        QCC_SyncPrintf("Starting threads... \n");
        for (int i = 0; i < THREAD_COUNT; i++) {
            char buf[256];
            sprintf(buf, "Thread.n%d", count);
            count++;
            t[i] = new ThreadClass((char*)buf);
            t[i]->Start();
        }


        QCC_SyncPrintf("deleting threads... \n");
        for (int i = 0; i < THREAD_COUNT; i++) {
            t[i]->Join();
            delete t[i];
        }

    }

    return (int) status;
}
