/**
 * @file AdapterUtil.cc
 *
 *
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

#include <qcc/platform.h>

#include <winsock2.h>
#include <string>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <list>

#include <qcc/AdapterUtil.h>
#include <qcc/NetInfo.h>

using namespace std;

/** @internal */
#define QCC_MODULE "ADAPTERUTIL"

namespace qcc {

AdapterUtil* AdapterUtil::singleton = NULL;

AdapterUtil::~AdapterUtil(void) {
}

QStatus AdapterUtil::ForceUpdate()
{
    uint32_t dwSize = 0;
    uint32_t dwRetVal = 0;
    QStatus status = ER_OK;
    char szAddress[NI_MAXHOST];

    int i = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
    IP_ADAPTER_DNS_SERVER_ADDRESS* pDnServer = NULL;
    IP_ADAPTER_PREFIX* pPrefix = NULL;

    status = lock.Lock();
    if (ER_OK != status) {
        goto exit;
    }

    interfaces.clear();
    isMultihomed = false;


    outBufLen = sizeof (IP_ADAPTER_ADDRESSES);
    pAddresses = (IP_ADAPTER_ADDRESSES*) malloc(outBufLen);
    if (pAddresses == NULL) {
        // printf("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
        status = ER_FAIL;
        goto exit;
    }

    // Make an initial call to GetAdaptersAddresses to get the
    // size needed into the outBufLen variable
    if (GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen)
        == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES*) malloc(outBufLen);
    }

    if (pAddresses == NULL) {
        // printf("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
        status = ER_FAIL;
        goto exit;
    }
    // Make a second call to GetAdapters Addresses to get the
    // actual data we want
    dwRetVal =
        GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

    if (dwRetVal == NO_ERROR) {
        // If successful, output some information from the data we received
        string savedPhysicalAddress;
        uint8_t adapterCount = 1;
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            qcc::String adapterName(pCurrAddresses->AdapterName);
#if 0
            // This code creates a circular dependency between unicode and common projects
            qcc::String description;
            ConvertUTF(pCurrAddresses->Description, description);
#else
            qcc::String description = "";
#endif
            QCC_DbgPrintf(("name %s", adapterName.c_str()));
            string physicalAddress((const char*)(pCurrAddresses->PhysicalAddress), pCurrAddresses->PhysicalAddressLength);

            // ignore the loopback interface and adapters that may be 'non-operational'
            if ((pCurrAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK) &&
                (description.find("Loopback") == description.npos) &&
                (IfOperStatusUp == pCurrAddresses->OperStatus ||
                 IfOperStatusTesting == pCurrAddresses->OperStatus ||
                 IfOperStatusDormant == pCurrAddresses->OperStatus)) {

                if (adapterCount > 1 &&
                    physicalAddress != savedPhysicalAddress) {
                    isMultihomed = true;
                }

                pUnicast = pCurrAddresses->FirstUnicastAddress;
                if (pUnicast != NULL) {
                    for (i = 0; pUnicast != NULL; i++) {

                        /* Convert the address stored in network format (binary) */
                        /* into a human-readable character string (presentation format). */
                        if (getnameinfo(pUnicast->Address.lpSockaddr,
                                        pUnicast->Address.iSockaddrLength,
                                        szAddress, sizeof(szAddress), NULL, 0,
                                        NI_NUMERICHOST)) {
                            QCC_LogError(ER_NONE, ("can't convert network format to presentation format"));

                            status = ER_FAIL;
                            goto exit;
                        }

                        NetInfo netInfo;
                        netInfo.name = adapterName;
                        netInfo.addr = IPAddress(szAddress);
                        netInfo.mtu = pCurrAddresses->Mtu;
                        netInfo.isVPN = (pCurrAddresses->IfType == IF_TYPE_TUNNEL);

                        interfaces.push_back(netInfo);

                        pUnicast = pUnicast->Next;
                    }
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
            adapterCount++;
            savedPhysicalAddress = physicalAddress;
        }
    } else {
        if (dwRetVal == ERROR_NO_DATA) {
            //    printf("\tNo addresses were found for the requested parameters\n");
        } else {
            QCC_LogError(ER_NONE, ("Invalid message type: 0x%0x", dwRetVal));

            status = ER_FAIL;
            goto exit;
        }
    }
exit:
    lock.Unlock();
    if (pAddresses != NULL) {
        free(pAddresses);
    }
    return status;
}


bool AdapterUtil::IsVPN(IPAddress addr)
{
    bool isVPN = false;
    AdapterUtil::const_iterator networkInterfaceIter;

    lock.Lock();

    // find record corresponding to 'addr'
    for (networkInterfaceIter = interfaces.begin(); networkInterfaceIter != interfaces.end(); ++networkInterfaceIter) {
        if (networkInterfaceIter->addr != addr) {
            continue;
        }
        isVPN = networkInterfaceIter->isVPN;
        break;
    }

    lock.Unlock();

    return isVPN;
}

}   /* namespace */
