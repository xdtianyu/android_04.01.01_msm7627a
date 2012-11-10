/**
 * @file
 * Prototype code for accessing avahi Dbus API directly.
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

#include <assert.h>

#include <string.h>
#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/IfConfig.h>
#include <qcc/GUID.h>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <qcc/Thread.h>  // For qcc::Sleep()

#include <Status.h>

#include <NameService.h>

#define QCC_MODULE "ALLJOYN"

using namespace ajn;

char const* g_names[] = {
    "org.randomteststring.A",
    "org.randomteststring.B",
    "org.randomteststring.C",
    "org.randomteststring.D",
    "org.randomteststring.E",
    "org.randomteststring.F",
    "org.randomteststring.G",
    "org.randomteststring.H",
    "org.randomteststring.I",
    "org.randomteststring.J",
    "org.randomteststring.K",
    "org.randomteststring.L",
    "org.randomteststring.M",
    "org.randomteststring.N",
    "org.randomteststring.O",
    "org.randomteststring.P",
    "org.randomteststring.Q",
    "org.randomteststring.R",
    "org.randomteststring.S",
    "org.randomteststring.T",
    "org.randomteststring.U",
    "org.randomteststring.V",
    "org.randomteststring.W",
    "org.randomteststring.X",
    "org.randomteststring.Y",
    "org.randomteststring.Z",
};

const uint32_t g_numberNames = sizeof(g_names) / sizeof(char const*);

class Finder {
  public:

    void Callback(const qcc::String& busAddr, const qcc::String& guid, std::vector<qcc::String>& wkn, uint8_t timer)
    {
        printf("Callback %s with guid %s and timer %d: ", busAddr.c_str(), guid.c_str(), timer);
        for (uint32_t i = 0; i < wkn.size(); ++i) {
            printf("%s ", wkn[i].c_str());
        }
        printf("\n");

        m_called = true;
        m_guid = guid;
        m_wkn = wkn;
        m_timer = timer;
    }

    void Reset(void)
    {
        m_called = false;
        m_guid.clear();
        m_wkn.clear();
    }

    bool GetCalled() { return m_called; }
    qcc::String GetGuid() { return m_guid; }
    std::vector<qcc::String> GetWkn() { return m_wkn; }
    uint8_t GetTimer() { return m_timer; }

  private:
    bool m_called;
    qcc::String m_guid;
    std::vector<qcc::String> m_wkn;
    uint8_t m_timer;
};

static void PrintFlags(uint32_t flags)
{
    printf("(");
    if (flags & qcc::IfConfigEntry::UP) {
        printf("UP ");
    }
    if (flags & qcc::IfConfigEntry::BROADCAST) {
        printf("BROADCAST ");
    }
    if (flags & qcc::IfConfigEntry::DEBUG) {
        printf("DEBUG ");
    }
    if (flags & qcc::IfConfigEntry::LOOPBACK) {
        printf("LOOPBACK ");
    }
    if (flags & qcc::IfConfigEntry::POINTOPOINT) {
        printf("POINTOPOINT ");
    }
    if (flags & qcc::IfConfigEntry::RUNNING) {
        printf("RUNNING ");
    }
    if (flags & qcc::IfConfigEntry::NOARP) {
        printf("NOARP ");
    }
    if (flags & qcc::IfConfigEntry::PROMISC) {
        printf("PROMISC ");
    }
    if (flags & qcc::IfConfigEntry::NOTRAILERS) {
        printf("NOTRAILERS ");
    }
    if (flags & qcc::IfConfigEntry::ALLMULTI) {
        printf("ALLMULTI ");
    }
    if (flags & qcc::IfConfigEntry::MASTER) {
        printf("MASTER ");
    }
    if (flags & qcc::IfConfigEntry::SLAVE) {
        printf("SLAVE ");
    }
    if (flags & qcc::IfConfigEntry::MULTICAST) {
        printf("MULTICAST ");
    }
    if (flags & qcc::IfConfigEntry::PORTSEL) {
        printf("PORTSEL ");
    }
    if (flags & qcc::IfConfigEntry::AUTOMEDIA) {
        printf("AUTOMEDIA ");
    }
    if (flags & qcc::IfConfigEntry::DYNAMIC) {
        printf("DYNAMIC ");
    }
    if (flags) printf("\b");
    printf(")");
}

#define ERROR_EXIT exit(1)

int main(int argc, char** argv)
{
    QStatus status;
    uint16_t port = 0;

    bool advertise = false;
    bool useEth0 = false;
    bool runtests = false;
    bool wildcard = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp("-a", argv[i]) == 0) {
            advertise = true;
        } else if (strcmp("-e", argv[i]) == 0) {
            useEth0 = true;
        } else if (strcmp("-t", argv[i]) == 0) {
            runtests = true;
        } else if (strcmp("-w", argv[i]) == 0) {
            wildcard = true;
        } else {
            printf("Unknown option %s\n", argv[i]);
            ERROR_EXIT;;
        }
    }

    if (runtests) {
        exit(0);
    }

    NameService ns;

    //
    // Initialize to a random quid, and talk to ourselves.
    //
    bool enableIPv4, enableIPv6, loopback;
    enableIPv4 = enableIPv6 = loopback = true;
    status = ns.Init(qcc::GUID128().ToString(), enableIPv4, enableIPv6, false, loopback);
    if (status != ER_OK) {
        QCC_LogError(status, ("Init failed"));
        ERROR_EXIT;
    }

    //
    // Figure out which interfaces we want to enable discovery on.
    //
    std::vector<qcc::IfConfigEntry> entries;
    status = qcc::IfConfig(entries);
    if (status != ER_OK) {
        QCC_LogError(status, ("IfConfig failed"));
        ERROR_EXIT;
    }

    printf("Checking out interfaces ...\n");
    qcc::String overrideInterface;
    for (uint32_t i = 0; i < entries.size(); ++i) {
        if (!useEth0) {
            if (entries[i].m_name == "eth0") {
                printf("******** Ignoring eth0, use \"-e\" to enable \n");
                continue;
            }
        }
        printf("    %s: ", entries[i].m_name.c_str());
        printf("0x%x = ", entries[i].m_flags);
        PrintFlags(entries[i].m_flags);
        if (entries[i].m_flags & qcc::IfConfigEntry::UP) {
            printf(", MTU = %d, address = %s", entries[i].m_mtu, entries[i].m_addr.c_str());
            if ((entries[i].m_flags & qcc::IfConfigEntry::LOOPBACK) == 0) {
                printf(" <--- Let's use this one");
                overrideInterface = entries[i].m_name;
                //
                // Tell the name service to talk and listen over the interface we chose
                // above.
                //
                status = ns.OpenInterface(entries[i].m_name);
                if (status != ER_OK) {
                    QCC_LogError(status, ("OpenInterface failed"));
                    ERROR_EXIT;
                }
            }
        }
        printf("\n");
    }

    srand(time(0));

    //
    // Pick a random port to advertise.  This is what would normally be the
    // daemon TCP well-known endpoint (9955) but we just make one up.  N.B. this
    // is not the name service multicast port.
    //
    port = rand();
    printf("Picked random port %d\n", port);
    status = ns.SetEndpoints("", "", port);

    if (status != ER_OK) {
        QCC_LogError(status, ("SetEndpoints failed"));
        ERROR_EXIT;
    }

    //
    // Enable the name service to communicate with the outside world.
    //
    ns.Enable();

    Finder finder;

    ns.SetCallback(new CallbackImpl<Finder, void, const qcc::String&, const qcc::String&,
                                    std::vector<qcc::String>&, uint8_t>(&finder, &Finder::Callback));

    if (wildcard) {
        //
        // Enable discovery on all of the test names in one go.
        //
        printf("locate org.randomteststring.*\n");
        status = ns.Locate("org.randomteststring.*");
        if (status != ER_OK) {
            QCC_LogError(status, ("Locate failed"));
            ERROR_EXIT;
        }
    } else {
        //
        // Enable discovery on all of the test names individually
        //
        for (uint32_t i = 0; !wildcard && i < g_numberNames; ++i) {
            printf("Locate %s\n", g_names[i]);

            status = ns.Locate(g_names[i]);
            if (status != ER_OK) {
                QCC_LogError(status, ("Locate failed"));
                ERROR_EXIT;
            }
        }
    }

    //
    // Hang around and mess with advertisements for a while.
    //
    for (uint32_t i = 0; i < 200; ++i) {
        //
        // Sleep for a while -- long enough for the name service to respond and
        // humans to observe what is happening.
        //
        printf("Zzzzz %d\n", i);

        qcc::Sleep(1000);

        if (advertise) {
            uint32_t nameIndex = rand() % g_numberNames;
            char const* wkn = g_names[nameIndex];

            status = ns.Advertise(wkn);
            printf("Advertised %s\n", wkn);
            if (status != ER_OK) {
                QCC_LogError(status, ("Advertise failed"));
                ERROR_EXIT;
            }

            nameIndex = rand() % g_numberNames;
            wkn = g_names[nameIndex];

            status = ns.Cancel(wkn);
            printf("Cancelled %s\n", wkn);
            if (status != ER_OK) {
                QCC_LogError(status, ("Cancel failed"));
                ERROR_EXIT;
            }
        }
    }

    return 0;
}
