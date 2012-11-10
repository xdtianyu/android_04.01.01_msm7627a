/**
 * @file
 *
 * This file implements methods from IPAddress.h.
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

#ifdef QCC_OS_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <algorithm>
#include <ctype.h>
#include <string.h>

#include <qcc/Debug.h>
#include <qcc/IPAddress.h>
#include <qcc/SocketTypes.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>

#include <Status.h>

#define QCC_MODULE "NETWORK"

using namespace std;
using namespace qcc;

IPAddress::IPAddress(const uint8_t* addrBuf, size_t addrBufSize)
{
    assert(addrBuf != NULL);
    assert(addrBufSize == IPv4_SIZE || addrBufSize == IPv6_SIZE);
    addrSize = (uint16_t)addrBufSize;
    if (addrSize == IPv4_SIZE) {
        // Encode the IPv4 address in the IPv6 address space for easy
        // conversion.
        memset(addr, 0, sizeof(addr) - 6);
        memset(&addr[IPv6_SIZE - IPv4_SIZE - sizeof(uint16_t)], 0xff, sizeof(uint16_t));
    }
    memcpy(&addr[IPv6_SIZE - addrSize], addrBuf, addrSize);
}

IPAddress::IPAddress(uint32_t ipv4Addr)
{
    addrSize = IPv4_SIZE;
    memset(addr, 0, sizeof(addr) - 6);
    addr[IPv6_SIZE - IPv4_SIZE - sizeof(uint16_t) + 0] = 0xff;
    addr[IPv6_SIZE - IPv4_SIZE - sizeof(uint16_t) + 1] = 0xff;
    addr[IPv6_SIZE - IPv4_SIZE + 0] = (uint8_t)((ipv4Addr >> 24) & 0xff);
    addr[IPv6_SIZE - IPv4_SIZE + 1] = (uint8_t)((ipv4Addr >> 16) & 0xff);
    addr[IPv6_SIZE - IPv4_SIZE + 2] = (uint8_t)((ipv4Addr >> 8) & 0xff);
    addr[IPv6_SIZE - IPv4_SIZE + 3] = (uint8_t)(ipv4Addr & 0xff);
}

IPAddress::IPAddress(const qcc::String& addrString)
{
    QStatus status = SetAddress(addrString, false);
    if (ER_OK != status) {
        QCC_LogError(status, ("Could not resolve \"%s\". Defaulting to INADDR_ANY", addrString.c_str()));
        SetAddress("");
    }
}

qcc::String IPAddress::IPv4ToString(const uint8_t addr[])
{
    qcc::String oss;
    size_t pos;

    pos = 0;
    oss.append(U32ToString(static_cast<uint32_t>(addr[pos]), 10));;
    for (++pos; pos < IPv4_SIZE; ++pos) {
        oss.push_back('.');
        oss.append(U32ToString(static_cast<uint32_t>(addr[pos]), 10));
    }

    return oss;
}

qcc::String IPAddress::IPv6ToString(const uint8_t addr[])
{
    qcc::String oss;
    size_t i;
    size_t j;
    int zerocnt;
    int maxzerocnt = 0;

    for (i = 0; i < IPv6_SIZE; i += 2) {
        if (addr[i] == 0 && addr[i + 1] == 0) {
            zerocnt = 0;
            for (j = IPv6_SIZE - 2; j > i; j -= 2) {
                if (addr[j] == 0 && addr[j + 1] == 0) {
                    ++zerocnt;
                    maxzerocnt = max(maxzerocnt, zerocnt);
                } else {
                    zerocnt = 0;
                }
            }
            // Count the zero we are pointing to.
            ++zerocnt;
            maxzerocnt = max(maxzerocnt, zerocnt);

            if (zerocnt == maxzerocnt) {
                oss.push_back(':');
                if (i == 0) {
                    oss.push_back(':');
                }
                i += (zerocnt - 1) * 2;
                continue;
            }
        }
        oss.append(U32ToString((uint32_t)(addr[i] << 8 | addr[i + 1]), 16));
        if (i + 2 < IPv6_SIZE) {
            oss.push_back(':');
        }
    }
    return oss;
}

inline void SetBits(uint64_t set[], int32_t offset, uint64_t bits)
{
    int32_t index = offset >> 6;
    if (0 == index) {
        set[index] |= (bits << offset);
    } else {
        set[index] |= (bits << (offset - 64));
    }
}

inline int64_t AccumulateDigits(char digits[], int32_t startIndex, int32_t lastIndex, int32_t radix)
{
    if (radix == 16) {
        uint64_t temp = 0;
        for (int32_t index = startIndex; index < lastIndex; index++) {
            temp <<= 4;
            temp |= CharToU8(digits[index]);
        }
        return temp;
    } else if (radix == 10) {
        uint64_t temp = 0;
        for (int32_t index = startIndex; index < lastIndex; index++) {
            temp *= 10;
            char c = digits[index];
            if (!IsDecimalDigit(c)) {
                return -1;
            }
            temp += CharToU8(c);
        }
        return temp;
    } else if (radix == 8) {
        uint64_t temp = 0;
        for (int32_t index = startIndex; index < lastIndex; index++) {
            temp <<= 3;
            char c = digits[index];
            if (!IsOctalDigit(c)) {
                return -1;
            }
            temp |= CharToU8(c);
        }
        return temp;
    }
    return -1;
}

QStatus IPAddress::StringToIPv6(qcc::String address, uint8_t addrBuf[], size_t addrBufSize)
{
    QStatus result = ER_OK;

    while (true) {
        if (NULL == addrBuf) {
            result = ER_BAD_ARG_2;
            break;
        }

        if (IPv6_SIZE != addrBufSize) {
            result = ER_BAD_ARG_3;
            break;
        }

        uint64_t leftBits[2] = { 0 };
        uint64_t rightBits[2] = { 0 };
        int32_t leftBitCount = 0;
        int32_t rightBitCount = 0;
        int32_t groupExpansionCount = 0;
        bool groupProcessed = false;
        int32_t octetCount = 0;
        bool leftCounter = false;
        size_t digitCount = 0;
        char digits[4];
        bool parseModeAny = true;

        for (int32_t i = address.size() - 1; i >= 0; --i) {
            if (parseModeAny) {
                if (address[i] == ':' && (i - 1 >= 0) && address[i - 1] == ':') {
                    // ::
                    if (++groupExpansionCount > 1) {
                        // invalid data
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    if (digitCount > 0) {
                        // 16 bit group, convert nibbles to hex
                        int64_t temp = AccumulateDigits(digits, ArraySize(digits) - digitCount, ArraySize(digits), 16);
                        if (temp < 0) {
                            // invalid data (should never happen)
                            result = ER_PARSE_ERROR;
                            break;
                        }

                        // Store in upper or lower 64 bits
                        if (leftCounter) {
                            SetBits(leftBits, leftBitCount, temp);
                            leftBitCount += 16;
                        } else {
                            SetBits(rightBits, rightBitCount, temp);
                            rightBitCount += 16;
                        }
                        // reset digitCount
                        digitCount = 0;
                    }

                    leftCounter = true;

                    // adjust for token
                    --i;
                } else if (address[i] == ':') {
                    // :
                    if (digitCount == 0) {
                        // invalid data
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    // 16 bit group, convert nibbles to hex
                    int64_t temp = AccumulateDigits(digits, ArraySize(digits) - digitCount, ArraySize(digits), 16);
                    if (temp < 0) {
                        // invalid data (should never happen)
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    // Store in upper or lower 64 bits
                    if (leftCounter) {
                        SetBits(leftBits, leftBitCount, temp);
                        leftBitCount += 16;
                    } else {
                        SetBits(rightBits, rightBitCount, temp);
                        rightBitCount += 16;
                    }
                    // reset digitCount
                    digitCount = 0;
                    groupProcessed = true;
                } else if (IsHexDigit(address[i])) {
                    // 0-F
                    if (++digitCount > ArraySize(digits)) {
                        // too much data
                        result = ER_PARSE_ERROR;
                        break;
                    }
                    digits[ArraySize(digits) - digitCount] = address[i];
                } else if (address[i] == '.') {
                    // . (mode transition)
                    if (++octetCount > 4) {
                        // too many octets
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    if (groupProcessed || groupExpansionCount > 0) {
                        // octets have to be the first group
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    if (digitCount == 0) {
                        // invalid data
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    int64_t temp = AccumulateDigits(digits, ArraySize(digits) - digitCount, ArraySize(digits), 10);
                    if (temp < 0 || temp > 0xff) {
                        // invalid decimal digits or range
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    // Store in upper or lower 64 bits
                    if (leftCounter) {
                        SetBits(leftBits, leftBitCount, temp);
                        leftBitCount += 8;
                    } else {
                        SetBits(rightBits, rightBitCount, temp);
                        rightBitCount += 8;
                    }
                    // reset digitCount
                    digitCount = 0;
                    parseModeAny = false;
                } else {
                    // invalid data
                    result = ER_PARSE_ERROR;
                    break;
                }
            } else {
                // Parse octets
                if (IsDecimalDigit(address[i])) {
                    // 0-9
                    if (++digitCount > ArraySize(digits)) {
                        // too much data
                        result = ER_PARSE_ERROR;
                        break;
                    }
                    digits[ArraySize(digits) - digitCount] = address[i];
                } else if (address[i] == '.' || address[i] == ':') {
                    ++octetCount;

                    if (address[i] == ':') {
                        // :
                        if (octetCount != 4) {
                            // too few octets
                            result = ER_PARSE_ERROR;
                            break;
                        }
                        parseModeAny = true;
                    }

                    // .
                    if (octetCount > 4) {
                        // too many octets
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    if (digitCount == 0) {
                        // invalid data
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    int64_t temp = AccumulateDigits(digits, ArraySize(digits) - digitCount, ArraySize(digits), 10);
                    if (temp < 0 || temp > 0xff) {
                        // invalid decimal digits or range
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    // Store in upper or lower 64 bits
                    if (leftCounter) {
                        SetBits(leftBits, leftBitCount, temp);
                        leftBitCount += 8;
                    } else {
                        SetBits(rightBits, rightBitCount, temp);
                        rightBitCount += 8;
                    }
                    // reset digitCount
                    digitCount = 0;
                } else {
                    // invalid data
                    result = ER_PARSE_ERROR;
                    break;
                }
            }
        }

        if (ER_OK != result) {
            break;
        }

        // Handle tail cases
        if (!parseModeAny) {
            // impartial ipv4 address in string termination
            result = ER_PARSE_ERROR;
            break;
        }

        if (digitCount > 0) {
            // 16 bit group, convert nibbles to hex
            int64_t temp = AccumulateDigits(digits, ArraySize(digits) - digitCount, ArraySize(digits), 16);
            if (temp < 0) {
                // invalid data (should never happen)
                result = ER_PARSE_ERROR;
                break;
            }

            // Store in upper or lower 64 bits
            if (leftCounter) {
                SetBits(leftBits, leftBitCount, temp);
                leftBitCount += 16;
            } else {
                SetBits(rightBits, rightBitCount, temp);
                rightBitCount += 16;
            }
        }

        // default initialization
        for (size_t i = 0; i < IPv6_SIZE; i++) {
            addrBuf[i] = 0;
        }

        // Pack results
        if (rightBitCount > 0 && leftBitCount > 0) {
            // Expansion in the middle
            int32_t expansionBits = 128 - rightBitCount - leftBitCount;
            int32_t index = IPv6_SIZE - 1;
            int32_t bitCount = 0;

            while (bitCount < rightBitCount) {
                if (bitCount < 64) {
                    addrBuf[index] = (uint8_t)(rightBits[0] & 0xff);
                    rightBits[0] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(rightBits[0] & 0xff);
                    rightBits[0] >>= 8;
                    --index;
                } else {
                    addrBuf[index] = (uint8_t)(rightBits[1] & 0xff);
                    rightBits[1] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(rightBits[1] & 0xff);
                    rightBits[1] >>= 8;
                    --index;
                }

                bitCount += 16;
            }

            // skip the expansion
            index -= (expansionBits >> 3);
            bitCount = 0;

            while (bitCount < leftBitCount) {
                if (bitCount < 64) {
                    addrBuf[index] = (uint8_t)(leftBits[0] & 0xff);
                    leftBits[0] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(leftBits[0] & 0xff);
                    leftBits[0] >>= 8;
                    --index;
                } else {
                    addrBuf[index] = (uint8_t)(leftBits[1] & 0xff);
                    leftBits[1] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(leftBits[1] & 0xff);
                    leftBits[1] >>= 8;
                    --index;
                }

                bitCount += 16;
            }

            result = ER_OK;
            break;
        } else if (rightBitCount == 0 && leftBitCount == 0) {
            if (groupExpansionCount > 0) {
                result = ER_OK;
                break;
            }
        } else if (leftBitCount > 0) {
            int32_t index = IPv6_SIZE - 1;
            int32_t bitCount = 0;

            if (groupExpansionCount > 0) {
                int32_t expansionBits = 128 - leftBitCount;
                // skip the expansion (already initialized)
                index -= (expansionBits >> 3);
            } else if (leftBitCount != 128) {
                // not enough data
                result = ER_PARSE_ERROR;
                break;
            }

            while (bitCount < leftBitCount) {
                if (bitCount < 64) {
                    addrBuf[index] = (uint8_t)(leftBits[0] & 0xff);
                    leftBits[0] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(leftBits[0] & 0xff);
                    leftBits[0] >>= 8;
                    --index;
                } else {
                    addrBuf[index] = (uint8_t)(leftBits[1] & 0xff);
                    leftBits[1] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(leftBits[1] & 0xff);
                    leftBits[1] >>= 8;
                    --index;
                }

                bitCount += 16;
            }

            result = ER_OK;
            break;
        } else if (rightBitCount > 0) {
            int32_t index = IPv6_SIZE - 1;
            int32_t bitCount = 0;

            // check for expansion
            if (groupExpansionCount == 0 && rightBitCount != 128) {
                // not enough data
                result = ER_PARSE_ERROR;
                break;
            }

            while (bitCount < rightBitCount) {
                if (bitCount < 64) {
                    addrBuf[index] = (uint8_t)(rightBits[0] & 0xff);
                    rightBits[0] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(rightBits[0] & 0xff);
                    rightBits[0] >>= 8;
                    --index;
                } else {
                    addrBuf[index] = (uint8_t)(rightBits[1] & 0xff);
                    rightBits[1] >>= 8;
                    --index;
                    addrBuf[index] = (uint8_t)(rightBits[1] & 0xff);
                    rightBits[1] >>= 8;
                    --index;
                }

                bitCount += 16;
            }

            result = ER_OK;
            break;
        }

        // insufficient data
        result = ER_PARSE_ERROR;
        break;
    }

    return result;
}

// 01.01.01.01, octal
// 0x01.0x01.0x01.0x1, hexadecimal
// 1.1.1.1, decimal, four octets
// 1.1.1, two octets, last is 16 bit,
// 1.1, one octet, second is 24 bits
// 1, value is 32 bits
QStatus IPAddress::StringToIPv4(qcc::String address, uint8_t addrBuf[], size_t addrBufSize)
{
    QStatus result = ER_OK;

    while (true) {
        if (NULL == addrBuf) {
            result = ER_BAD_ARG_2;
            break;
        }

        if (IPv4_SIZE != addrBufSize) {
            result = ER_BAD_ARG_3;
            break;
        }

        size_t digitCount = 0;
        // 32-bit integer in octal takes 11 digits
        char digits[11];
        //0 - decimal, 1 - hexadecimal, 2 - octal
        int32_t digitsMode = 0;
        uint32_t parts[4];
        size_t partCount = 0;

        for (size_t i = 0; i < address.size(); i++) {
            if (address[i] == '.') {
                // .
                if (digitCount == 0) {
                    // no data specified
                    result = ER_PARSE_ERROR;
                    break;
                }

                if (partCount >= ArraySize(parts)) {
                    // too many parts
                    result = ER_PARSE_ERROR;
                    break;
                }

                int64_t temp;

                if (digitsMode == 0) {
                    // decimal
                    temp = AccumulateDigits(digits, 0, digitCount, 10);
                } else if (digitsMode == 1) {
                    // hex
                    temp = AccumulateDigits(digits, 0, digitCount, 16);
                } else if (digitsMode == 2) {
                    // octal
                    temp = AccumulateDigits(digits, 0, digitCount, 8);
                } else {
                    // invalid mode
                    result = ER_PARSE_ERROR;
                    break;
                }

                if (temp < 0) {
                    // conversion was invalid
                    result = ER_PARSE_ERROR;
                    break;
                }

                if (temp > 0xFFFFFFFF) {
                    // part overflowed
                    result = ER_PARSE_ERROR;
                    break;
                }

                parts[partCount++] = (uint32_t)temp;
                digitCount = 0;
                // back to decimal default
                digitsMode = 0;
            } else if (address[i] == '0') {
                if (i + 1 < address.size() && address[i + 1] == 'x') {
                    if (digitCount != 0) {
                        // mode change only allowed at the beginning of the value
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    // hexadecimal
                    digitsMode = 1;

                    // adjust for token
                    ++i;
                } else if (digitCount == 0 &&
                           i + 1 < address.size() && IsOctalDigit(address[i + 1])) {
                    // octal
                    digitsMode = 2;
                } else {
                    if (digitCount >= ArraySize(digits)) {
                        // too much data
                        result = ER_PARSE_ERROR;
                        break;
                    }

                    // 0 is valid in all 3 supported bases
                    digits[digitCount++] = address[i];
                }
            } else if (IsHexDigit(address[i])) {
                // accumulate digits

                // validate mode data
                if (digitsMode == 0 && !IsDecimalDigit(address[i])) {
                    // invalid decimal digit
                    result = ER_PARSE_ERROR;
                    break;
                } else if (digitsMode == 1 && !IsHexDigit(address[i])) {
                    // invalid hex digit
                    result = ER_PARSE_ERROR;
                    break;
                } else if (digitsMode == 2 && !IsOctalDigit(address[i])) {
                    // invalid octal digit
                    result = ER_PARSE_ERROR;
                    break;
                } else if (digitsMode < 0 || digitsMode > 2) {
                    // invalid mode
                    result = ER_PARSE_ERROR;
                    break;
                }

                if (digitCount >= ArraySize(digits)) {
                    // too much data
                    result = ER_PARSE_ERROR;
                    break;
                }

                digits[digitCount++] = address[i];
            } else {
                // invalid data
                result = ER_PARSE_ERROR;
                break;
            }
        }

        if (ER_OK != result) {
            break;
        }

        // handle tail case
        if (digitCount > 0) {
            if (partCount >= ArraySize(parts)) {
                // too many parts
                result = ER_PARSE_ERROR;
                break;
            }

            int64_t temp;

            if (digitsMode == 0) {
                // decimal
                temp = AccumulateDigits(digits, 0, digitCount, 10);
            } else if (digitsMode == 1) {
                // hex
                temp = AccumulateDigits(digits, 0, digitCount, 16);
            } else if (digitsMode == 2) {
                // octal
                temp = AccumulateDigits(digits, 0, digitCount, 8);
            } else {
                // invalid mode
                result = ER_PARSE_ERROR;
                break;
            }

            if (temp < 0) {
                // conversion was invalid
                result = ER_PARSE_ERROR;
                break;
            }

            if (temp > 0xFFFFFFFF) {
                // part overflowed
                result = ER_PARSE_ERROR;
                break;
            }

            parts[partCount++] = (uint32_t)temp;
        }

        // check ranges against parts & pack
        if (partCount == 1) {
#if (QCC_TARGET_ENDIAN == QCC_LITTLE_ENDIAN)
            addrBuf[3] = (uint8_t)(parts[0] & 0xFF);
            addrBuf[2] = (uint8_t)((parts[0] >> 8) & 0xFF);
            addrBuf[1] = (uint8_t)((parts[0] >> 16) & 0xFF);
            addrBuf[0] = (uint8_t)((parts[0] >> 24) & 0xFF);
#else
            addrBuf[3] = (uint8_t)((parts[0] >> 24) & 0xFF);
            addrBuf[2] = (uint8_t)((parts[0] >> 16) & 0xFF);
            addrBuf[1] = (uint8_t)((parts[0] >> 8)  & 0xFF);
            addrBuf[0] = (uint8_t)(parts[0] & 0xFF);
#endif
            result = ER_OK;
            break;
        } else if (partCount == 2) {
            if (parts[0] > 0xFF || parts[1] > 0xFFFFFF) {
                // range invalid
                result = ER_PARSE_ERROR;
                break;
            }
#if (QCC_TARGET_ENDIAN == QCC_LITTLE_ENDIAN)
            addrBuf[3] = (uint8_t)(parts[0] & 0xFF);
            addrBuf[2] = (uint8_t)((parts[0] >> 8) & 0xFF);
            addrBuf[1] = (uint8_t)((parts[0] >> 16) & 0xFF);
#else
            addrBuf[3] = (uint8_t)((parts[0] >> 24) & 0xFF);
            addrBuf[2] = (uint8_t)((parts[0] >> 16) & 0xFF);
            addrBuf[1] = (uint8_t)((parts[0] >> 8)  & 0xFF);
#endif
            addrBuf[0] = (uint8_t)parts[0];

            result = ER_OK;
            break;
        } else if (partCount == 3) {
            if (parts[0] > 0xFF || parts[1] > 0xFF ||
                parts[2] > 0xFFFF) {
                // range invalid
                result = ER_PARSE_ERROR;
                break;
            }
#if (QCC_TARGET_ENDIAN == QCC_LITTLE_ENDIAN)
            addrBuf[3] = (uint8_t)(parts[0] & 0xFF);
            addrBuf[2] = (uint8_t)((parts[0] >> 8) & 0xFF);
#else
            addrBuf[3] = (uint8_t)((parts[0] >> 24) & 0xFF);
            addrBuf[2] = (uint8_t)((parts[0] >> 16) & 0xFF);
#endif
            addrBuf[1] = (uint8_t)parts[1];
            addrBuf[0] = (uint8_t)parts[0];

            result = ER_OK;
            break;
        } else if (partCount == 4) {
            if (parts[0] > 0xFF || parts[1] > 0xFF ||
                parts[2] > 0xFF || parts[3] > 0xFF) {
                // range invalid
                result = ER_PARSE_ERROR;
                break;
            }

            addrBuf[3] = (uint8_t)parts[3];
            addrBuf[2] = (uint8_t)parts[2];
            addrBuf[1] = (uint8_t)parts[1];
            addrBuf[0] = (uint8_t)parts[0];

            result = ER_OK;
            break;
        }

        // not enough data specified
        result = ER_PARSE_ERROR;
        break;
    }

    return result;
}

QStatus IPAddress::SetAddress(const qcc::String& addrString, bool allowHostNames, uint32_t timeoutMs)
{
    QStatus status = ER_PARSE_ERROR;

    addrSize = 0;
    memset(addr, 0xFF, sizeof(addr));

    if (addrString.empty()) {
        // INADDR_ANY
        addrSize = IPv6_SIZE;
        status = StringToIPv6("::", addr, addrSize);
    } else if (addrString.find_first_of(':') != addrString.npos) {
        // IPV6
        addrSize = IPv6_SIZE;
        status = StringToIPv6(addrString, addr, addrSize);
    } else {
        // Try IPV4
        addrSize = IPv4_SIZE;
        status = StringToIPv4(addrString, &addr[IPv6_SIZE - IPv4_SIZE], addrSize);
        if (ER_OK != status && allowHostNames) {
            size_t addrLen;
            status = ResolveHostName(addrString, addr, IPv6_SIZE, addrLen, timeoutMs);
            if (ER_OK == status) {
                if (addrLen == IPv6_SIZE) {
                    addrSize = IPv6_SIZE;
                } else {
                    addrSize = IPv4_SIZE;
                }
            }
        }
    }

    return status;
}

QStatus IPAddress::RenderIPv4Binary(uint8_t addrBuf[], size_t addrBufSize) const
{
    QStatus status = ER_OK;
    assert(addrSize == IPv4_SIZE);
    if (addrBufSize < IPv4_SIZE) {
        status = ER_BUFFER_TOO_SMALL;
        QCC_LogError(status, ("Copying IPv4 address to buffer"));
        goto exit;
    }
    memcpy(addrBuf, &addr[IPv6_SIZE - IPv4_SIZE], IPv4_SIZE);

exit:
    return status;
}
QStatus IPAddress::RenderIPv6Binary(uint8_t addrBuf[], size_t addrBufSize) const
{
    QStatus status = ER_OK;
    assert(addrSize == IPv6_SIZE);
    if (addrBufSize < IPv6_SIZE) {
        status = ER_BUFFER_TOO_SMALL;
        QCC_LogError(status, ("Copying IPv6 address to buffer"));
        goto exit;
    }
    memcpy(addrBuf, addr, IPv6_SIZE);

exit:
    return status;
}

QStatus IPAddress::RenderIPBinary(uint8_t addrBuf[], size_t addrBufSize) const
{
    QStatus status = ER_OK;
    if (addrBufSize < addrSize) {
        status = ER_BUFFER_TOO_SMALL;
        QCC_LogError(status, ("Copying IP address to buffer"));
        goto exit;
    }
    memcpy(addrBuf, &addr[IPv6_SIZE - addrSize], addrSize);

exit:
    return status;
}


uint32_t IPAddress::GetIPv4AddressCPUOrder(void) const
{
    return ((static_cast<uint32_t>(addr[IPv6_SIZE - IPv4_SIZE + 0]) << 24) |
            (static_cast<uint32_t>(addr[IPv6_SIZE - IPv4_SIZE + 1]) << 16) |
            (static_cast<uint32_t>(addr[IPv6_SIZE - IPv4_SIZE + 2]) << 8) |
            static_cast<uint32_t>(addr[IPv6_SIZE - IPv4_SIZE + 3]));
}

uint32_t IPAddress::GetIPv4AddressNetOrder(void) const
{
    uint32_t addr4;
    memcpy(&addr4, &addr[IPv6_SIZE - IPv4_SIZE], IPv4_SIZE);
    return addr4;
}
