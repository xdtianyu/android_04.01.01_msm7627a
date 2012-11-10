/**
 * @file
 *
 * This file tests the IP Address class.
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

#include <stdio.h>
#include <string>

#include <qcc/IPAddress.h>

using namespace qcc;

int main(void)
{
    IPAddress invalid;

    uint8_t ipv4_init1[] = { 0, 0, 0, 0 };
    IPAddress ipv4_1(ipv4_init1, sizeof(ipv4_init1));

    uint8_t ipv4_init2[] = { 127, 0, 0, 1 };
    IPAddress ipv4_2(ipv4_init2, sizeof(ipv4_init2));

    uint8_t ipv4_init3[] = { 10, 10, 32, 32 };
    IPAddress ipv4_3(ipv4_init3, sizeof(ipv4_init3));


    uint8_t ipv6_init1[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    IPAddress ipv6_1(ipv6_init1, sizeof(ipv6_init1));

    uint8_t ipv6_init2[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    IPAddress ipv6_2(ipv6_init2, sizeof(ipv6_init2));

    uint8_t ipv6_init3[] = { 0xde, 0xad, 0, 0, 0xbe, 0xef, 0, 0, 0xca, 0x11, 0, 0, 0, 0, 0xd, 0xad };
    IPAddress ipv6_3(ipv6_init3, sizeof(ipv6_init3));

    uint8_t ipv6_init4[] = { 0, 0, 0, 0x12, 0, 0, 0x34, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    IPAddress ipv6_4(ipv6_init4, sizeof(ipv6_init4));

    uint8_t ipv6_init5[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0xab, 0xcd, 0xef, 0, 0, 0, 0, 0 };
    IPAddress ipv6_5(ipv6_init5, sizeof(ipv6_init5));

    IPAddress ipv64_1(ipv4_3.GetIPv6Reference(), IPAddress::IPv6_SIZE);
    IPAddress ipv64_2(ipv4_3);
    ipv64_2.ConvertToIPv6();

    printf("Invalid IP Address: %s\n", invalid.ToString().c_str());

    printf("IPv4 Address 1: %s\n", ipv4_1.ToString().c_str());
    printf("IPv4 Address 2: %s\n", ipv4_2.ToString().c_str());
    printf("IPv4 Address 3: %s\n", ipv4_3.ToString().c_str());

    printf("IPv6 Address 1: %s\n", ipv6_1.ToString().c_str());
    printf("IPv6 Address 2: %s\n", ipv6_2.ToString().c_str());
    printf("IPv6 Address 3: %s\n", ipv6_3.ToString().c_str());
    printf("IPv6 Address 4: %s\n", ipv6_4.ToString().c_str());
    printf("IPv6 Address 5: %s\n", ipv6_5.ToString().c_str());

    printf("IPv4 address in IPv6 space: %s\n", ipv64_1.ToString().c_str());
    printf("IPv4 address in IPv6 space: %s\n", ipv64_2.ToString().c_str());

    printf("Parse \"16.32.48.64\": %s\n", IPAddress("16.32.48.64").ToString().c_str());

    printf("Parse \"0123:4567:89AB:CDEF:fedc:ba98:7654:3210\":  %s\n", IPAddress("0123:4567:89AB:CDEF:fedc:ba98:7654:3210").ToString().c_str());
    printf("Parse \"::\":    %s\n", IPAddress("::").ToString().c_str());
    printf("Parse \"::1\":   %s\n", IPAddress("::1").ToString().c_str());
    printf("Parse \"::1:2\": %s\n", IPAddress("::1:2").ToString().c_str());
    printf("Parse \"1::\":   %s\n", IPAddress("1::").ToString().c_str());
    printf("Parse \"1:2::\": %s\n", IPAddress("1:2::").ToString().c_str());
    printf("Parse \"1::2\":  %s\n", IPAddress("1::2").ToString().c_str());

    return 0;
}
