/**
 * @file
 * The simple name service protocol implementation
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
#include <qcc/Debug.h>
#include <qcc/SocketTypes.h>

#include "NsProtocol.h"

#define QCC_MODULE "NS"

//
// Strangely, Android doesn't define the IPV4 presentation format string length
// even though it does define the IPv6 version.
//
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

using namespace std;
using namespace qcc;

namespace ajn {

StringData::StringData()
    : m_size(0)
{
}

StringData::~StringData()
{
}

void StringData::Set(qcc::String string)
{
    m_size = string.size();
    m_string = string;
}

qcc::String StringData::Get(void) const
{
    return m_string;
}

size_t StringData::GetSerializedSize(void) const
{
    return 1 + m_size;
}

size_t StringData::Serialize(uint8_t* buffer) const
{
    QCC_DbgPrintf(("StringData::Serialize(): %s to buffer 0x%x", m_string.c_str(), buffer));
    assert(m_size == m_string.size());
    buffer[0] = static_cast<uint8_t>(m_size);
    memcpy(reinterpret_cast<void*>(&buffer[1]), const_cast<void*>(reinterpret_cast<const void*>(m_string.c_str())), m_size);

    return 1 + m_size;
}

size_t StringData::Deserialize(uint8_t const* buffer, uint32_t bufsize)
{
    QCC_DbgPrintf(("StringData::Deserialize()"));

    //
    // If there's not enough data in the buffer to even get the string size out
    // then bail.
    //
    if (bufsize < 1) {
        QCC_DbgPrintf(("StringData::Deserialize(): Insufficient bufsize %d", bufsize));
        return 0;
    }

    m_size = buffer[0];
    --bufsize;

    //
    // If there's not enough data in the buffer then bail.
    //
    if (bufsize < m_size) {
        QCC_DbgPrintf(("StringData::Deserialize(): Insufficient bufsize %d", bufsize));
        m_size = 0;
        return 0;
    }
    if (m_size > 0) {
        m_string.assign(reinterpret_cast<const char*>(buffer + 1), m_size);
    } else {
        m_string.clear();
    }
    QCC_DbgPrintf(("StringData::Deserialize(): %s from buffer", m_string.c_str()));
    return 1 + m_size;
}

IsAt::IsAt()
    : m_flagG(false), m_flagC(false), m_flagT(false), m_flagU(false), m_flagS(false), m_flagF(false), m_port(0)
{
}

IsAt::~IsAt()
{
}

void IsAt::SetGuid(const qcc::String& guid)
{
    m_guid = guid;
    m_flagG = true;
}

qcc::String IsAt::GetGuid(void) const
{
    return m_guid;
}

void IsAt::SetPort(uint16_t port)
{
    assert(port);
    m_port = port;
}

uint16_t IsAt::GetPort(void) const
{
    return m_port;
}

void IsAt::ClearIPv4(void)
{
    m_ipv4.clear();
    m_flagF = false;
}

void IsAt::SetIPv4(qcc::String ipv4)
{
    m_ipv4 = ipv4;
    m_flagF = true;
}

qcc::String IsAt::GetIPv4(void) const
{
    return m_ipv4;
}

void IsAt::ClearIPv6(void)
{
    m_ipv6.clear();
    m_flagS = false;
}

void IsAt::SetIPv6(qcc::String ipv6)
{
    m_ipv6 = ipv6;
    m_flagS = true;
}

qcc::String IsAt::GetIPv6(void) const
{
    return m_ipv6;
}

void IsAt::AddName(qcc::String name)
{
    m_names.push_back(name);
}

uint32_t IsAt::GetNumberNames(void) const
{
    return m_names.size();
}

qcc::String IsAt::GetName(uint32_t index) const
{
    assert(index < m_names.size());
    return m_names[index];
}

size_t IsAt::GetSerializedSize(void) const
{
    //
    // We have one octet for type and flags, one octet for count and
    // two octets for port.  Four octets to start.
    //
    size_t size = 4;

    //
    // If the F bit is set, we are going to include an IPv4 address
    // which is 32 bits long;
    //
    if (m_flagF) {
        size += 32 / 8;
    }

    //
    // If the S bit is set, we are going to include an IPv6 address
    // which is 128 bits long;
    //
    if (m_flagS) {
        size += 128 / 8;
    }

    //
    // Let the string data decide for themselves how long the rest of the
    // message will be.  The rest of the message will be a possible GUID
    // string and the names.
    //
    if (m_flagG) {
        StringData s;
        s.Set(m_guid);
        size += s.GetSerializedSize();
    }

    for (uint32_t i = 0; i < m_names.size(); ++i) {
        StringData s;
        s.Set(m_names[i]);
        size += s.GetSerializedSize();
    }

    return size;
}

size_t IsAt::Serialize(uint8_t* buffer) const
{
    QCC_DbgPrintf(("IsAt::Serialize(): to buffer 0x%x", buffer));
    //
    // We keep track of the size so testers can check coherence between
    // GetSerializedSize() and Serialize() and Deserialize().
    //
    size_t size = 0;

    //
    // The first octet is type (M = 1) and flags.
    //
    uint8_t typeAndFlags = 1 << 6;

    if (m_flagG) {
        QCC_DbgPrintf(("IsAt::Serialize(): G flag"));
        typeAndFlags |= 0x20;
    }
    if (m_flagC) {
        QCC_DbgPrintf(("IsAt::Serialize(): C flag"));
        typeAndFlags |= 0x10;
    }
    if (m_flagT) {
        QCC_DbgPrintf(("IsAt::Serialize(): T flag"));
        typeAndFlags |= 0x8;
    }
    if (m_flagU) {
        QCC_DbgPrintf(("IsAt::Serialize(): U flag"));
        typeAndFlags |= 0x4;
    }
    if (m_flagS) {
        QCC_DbgPrintf(("IsAt::Serialize(): S flag"));
        typeAndFlags |= 0x2;
    }
    if (m_flagF) {
        QCC_DbgPrintf(("IsAt::Serialize(): F flag"));
        typeAndFlags |= 0x1;
    }

    buffer[0] = typeAndFlags;
    size += 1;

    //
    // The second octet is the count of bus names.
    //
    assert(m_names.size() < 256);
    buffer[1] = static_cast<uint8_t>(m_names.size());
    QCC_DbgPrintf(("IsAt::Serialize(): Count %d", m_names.size()));
    size += 1;

    //
    // The following two octets are the port number in network byte
    // order (big endian, or most significant byte first).
    //
    buffer[2] = static_cast<uint8_t>(m_port >> 8);
    buffer[3] = static_cast<uint8_t>(m_port);
    QCC_DbgPrintf(("IsAt::Serialize(): Port %d", m_port));
    size += 2;

    //
    // From this point on, things are not at fixed addresses
    //
    uint8_t* p = &buffer[4];

    //
    // If the F bit is set, we need to include the IPv4 address.
    //
    if (m_flagF) {
        INET_PTON(AF_INET, m_ipv4.c_str(), p);
        QCC_DbgPrintf(("IsAt::Serialize(): IPv4: %s", m_ipv4.c_str()));
        p += 4;
        size += 4;
    }

    //
    // If the S bit is set, we need to include the IPv6 address.
    //
    if (m_flagS) {
        INET_PTON(AF_INET6, m_ipv6.c_str(), p);
        QCC_DbgPrintf(("IsAt::Serialize(): IPv6: %s", m_ipv6.c_str()));
        p += 16;
        size += 16;
    }

    //
    // Let the string data decide for themselves how long the rest of the
    // message will be.  If the G bit is set, we need to include the GUID
    // string.
    //
    if (m_flagG) {
        StringData stringData;
        stringData.Set(m_guid);
        QCC_DbgPrintf(("IsAt::Serialize(): GUID %s", m_guid.c_str()));
        size_t stringSize = stringData.Serialize(p);
        size += stringSize;
        p += stringSize;
    }


    for (uint32_t i = 0; i < m_names.size(); ++i) {
        StringData stringData;
        stringData.Set(m_names[i]);
        QCC_DbgPrintf(("IsAt::Serialize(): name %s", m_names[i].c_str()));
        size_t stringSize = stringData.Serialize(p);
        size += stringSize;
        p += stringSize;
    }

    return size;
}

size_t IsAt::Deserialize(uint8_t const* buffer, uint32_t bufsize)
{
    QCC_DbgPrintf(("IsAt::Deserialize()"));

    //
    // If there's not enough room in the buffer to get the fixed part out then
    // bail (one byte of type and flags, one byte of name count and two bytes
    // of port).
    //
    if (bufsize < 4) {
        QCC_DbgPrintf(("IsAt::Deserialize(): Insufficient bufsize %d", bufsize));
        return 0;
    }

    //
    // We keep track of the size (the size of the buffer we read) so testers
    // can check coherence between GetSerializedSize() and Serialize() and
    // Deserialize().
    //
    size_t size = 0;

    //
    // The first octet is type (1) and flags.
    //
    uint8_t typeAndFlags = buffer[0];
    size += 1;

    //
    // This had better be an IsAt message we're working on
    //
    if ((typeAndFlags & 0xc0) != 1 << 6) {
        QCC_DbgPrintf(("IsAt::Deserialize(): Incorrect type %d", typeAndFlags & 0xc0));
        return 0;
    }

    m_flagG = (typeAndFlags & 0x20) != 0;
    QCC_DbgPrintf(("IsAt::Deserialize(): G flag %d", m_flagG));

    m_flagC = (typeAndFlags & 0x10) != 0;
    QCC_DbgPrintf(("IsAt::Deserialize(): C flag %d", m_flagC));

    m_flagT = (typeAndFlags & 0x8) != 0;
    QCC_DbgPrintf(("IsAt::Deserialize(): T flag %d", m_flagT));

    m_flagU = (typeAndFlags & 0x4) != 0;
    QCC_DbgPrintf(("IsAt::Deserialize(): U flag %d", m_flagU));

    m_flagS = (typeAndFlags & 0x2) != 0;
    QCC_DbgPrintf(("IsAt::Deserialize(): S flag %d", m_flagS));

    m_flagF = (typeAndFlags & 0x1) != 0;
    QCC_DbgPrintf(("IsAt::Deserialize(): F flag %d", m_flagF));

    //
    // The second octet is the count of bus names.
    //
    uint8_t numberNames = buffer[1];
    QCC_DbgPrintf(("IsAt::Deserialize(): Count %d", numberNames));
    size += 1;

    //
    // The following two octets are the port number in network byte
    // order (big endian, or most significant byte first).
    //
    m_port = (static_cast<uint16_t>(buffer[2]) << 8) | (static_cast<uint16_t>(buffer[3]) & 0xff);
    QCC_DbgPrintf(("IsAt::Deserialize(): Port %d", m_port));
    size += 2;

    //
    // From this point on, things are not at fixed addresses
    //
    uint8_t const* p = &buffer[4];
    bufsize -= 4;

    //
    // If the F bit is set, we need to read off an IPv4 address; and we'd better
    // have enough buffer to read it out of.
    //
    if (m_flagF) {
        if (bufsize < 4) {
            QCC_DbgPrintf(("IsAt::Deserialize(): Insufficient bufsize %d", bufsize));
            return 0;
        }
        char strbuf[INET_ADDRSTRLEN];
        INET_NTOP(AF_INET, (void*)p, strbuf, INET_ADDRSTRLEN);
        m_ipv4 = qcc::String(strbuf);
        QCC_DbgPrintf(("IsAt::Deserialize(): IPv4: %s", m_ipv4.c_str()));
        p += 4;
        size += 4;
        bufsize -= 4;
    }

    //
    // If the S bit is set, we need to read off an IPv6 address; and we'd better
    // have enough buffer to read it out of.
    //
    if (m_flagS) {
        if (bufsize < 16) {
            QCC_DbgPrintf(("IsAt::Deserialize(): Insufficient bufsize %d", bufsize));
            return 0;
        }
        char strbuf[INET6_ADDRSTRLEN];
        INET_NTOP(AF_INET6, (void*)p, strbuf, INET6_ADDRSTRLEN);
        m_ipv6 = qcc::String(strbuf);
        QCC_DbgPrintf(("IsAt::Deserialize(): IPv6: %s", m_ipv6.c_str()));
        p += 16;
        size += 16;
        bufsize -= 16;
    }

    //
    // If the G bit is set, we need to read off a GUID string.
    //
    if (m_flagG) {
        QCC_DbgPrintf(("IsAt::Deserialize(): StringData::Deserialize() GUID"));
        StringData stringData;

        //
        // Tell the string to read itself out.  If there's not enough buffer
        // it will complain by returning 0.  We pass the complaint on up.
        //
        size_t stringSize = stringData.Deserialize(p, bufsize);
        if (stringSize == 0) {
            QCC_DbgPrintf(("IsAt::Deserialize(): StringData::Deserialize():  Error"));
            return 0;
        }
        SetGuid(stringData.Get());
        size += stringSize;
        p += stringSize;
        bufsize -= stringSize;
    }

    //
    // Now we need to read out <numberNames> names that the packet has told us
    // will be there.
    //
    for (uint32_t i = 0; i < numberNames; ++i) {
        QCC_DbgPrintf(("IsAt::Deserialize(): StringData::Deserialize() name %d", i));
        StringData stringData;

        //
        // Tell the string to read itself out.  If there's not enough buffer
        // it will complain by returning 0.  We pass the complaint on up.
        //
        size_t stringSize = stringData.Deserialize(p, bufsize);
        if (stringSize == 0) {
            QCC_DbgPrintf(("IsAt::Deserialize(): StringData::Deserialize():  Error"));
            return 0;
        }
        AddName(stringData.Get());
        size += stringSize;
        p += stringSize;
        bufsize -= stringSize;
    }

    return size;
}

WhoHas::WhoHas()
    : m_flagT(false), m_flagU(false), m_flagS(false), m_flagF(false)
{
}

WhoHas::~WhoHas()
{
}

void WhoHas::AddName(qcc::String name)
{
    m_names.push_back(name);
}

uint32_t WhoHas::GetNumberNames(void) const
{
    return m_names.size();
}

qcc::String WhoHas::GetName(uint32_t index) const
{
    assert(index < m_names.size());
    return m_names[index];
}

size_t WhoHas::GetSerializedSize(void) const
{
    //
    // We have one octet for type and flags and one octet for count.
    // Two octets to start.
    //
    size_t size = 2;

    //
    // Let the string data decide for themselves how long the rest
    // of the message will be.
    //
    for (uint32_t i = 0; i < m_names.size(); ++i) {
        StringData s;
        s.Set(m_names[i]);
        size += s.GetSerializedSize();
    }

    return size;
}

size_t WhoHas::Serialize(uint8_t* buffer) const
{
    QCC_DbgPrintf(("WhoHas::Serialize(): to buffer 0x%x", buffer));
    //
    // We keep track of the size so testers can check coherence between
    // GetSerializedSize() and Serialize() and Deserialize().
    //
    size_t size = 0;

    //
    // The first octet is type (M = 2) and flags.
    //
    uint8_t typeAndFlags = 2 << 6;

    if (m_flagT) {
        QCC_DbgPrintf(("WhoHas::Serialize(): T flag"));
        typeAndFlags |= 0x8;
    }
    if (m_flagU) {
        QCC_DbgPrintf(("WhoHas::Serialize(): U flag"));
        typeAndFlags |= 0x4;
    }
    if (m_flagS) {
        QCC_DbgPrintf(("WhoHas::Serialize(): S flag"));
        typeAndFlags |= 0x2;
    }
    if (m_flagF) {
        QCC_DbgPrintf(("WhoHas::Serialize(): F flag"));
        typeAndFlags |= 0x1;
    }

    buffer[0] = typeAndFlags;
    size += 1;

    //
    // The second octet is the count of bus names.
    //
    assert(m_names.size() < 256);
    buffer[1] = static_cast<uint8_t>(m_names.size());
    QCC_DbgPrintf(("WhoHas::Serialize(): Count %d", m_names.size()));
    size += 1;

    //
    // From this point on, things are not at fixed addresses
    //
    uint8_t* p = &buffer[2];

    //
    // Let the string data decide for themselves how long the rest
    // of the message will be.
    //
    for (uint32_t i = 0; i < m_names.size(); ++i) {
        StringData stringData;
        stringData.Set(m_names[i]);
        QCC_DbgPrintf(("Whohas::Serialize(): name %s", m_names[i].c_str()));
        size_t stringSize = stringData.Serialize(p);
        size += stringSize;
        p += stringSize;
    }

    return size;
}

size_t WhoHas::Deserialize(uint8_t const* buffer, uint32_t bufsize)
{
    QCC_DbgPrintf(("WhoHas::Deserialize()"));

    //
    // If there's not enough room in the buffer to get the fixed part out then
    // bail (one byte of type and flags, one byte of name count).
    //
    if (bufsize < 2) {
        QCC_DbgPrintf(("WhoHas::Deserialize(): Insufficient bufsize %d", bufsize));
        return 0;
    }

    //
    // We keep track of the size so testers can check coherence between
    // GetSerializedSize() and Serialize() and Deserialize().
    //
    size_t size = 0;

    //
    // The first octet is type (1) and flags.
    //
    uint8_t typeAndFlags = buffer[0];
    size += 1;

    //
    // This had better be an WhoHas message we're working on
    //
    if ((typeAndFlags & 0xc0) != 2 << 6) {
        QCC_DbgPrintf(("WhoHas::Deserialize(): Incorrect type %d", typeAndFlags & 0xc0));
        return 0;
    }

    m_flagT = (typeAndFlags & 0x8) != 0;
    QCC_DbgPrintf(("WhoHas::Deserialize(): T flag %d", m_flagT));

    m_flagU = (typeAndFlags & 0x4) != 0;
    QCC_DbgPrintf(("WhoHas::Deserialize(): U flag %d", m_flagU));

    m_flagS = (typeAndFlags & 0x2) != 0;
    QCC_DbgPrintf(("WhoHas::Deserialize(): S flag %d", m_flagS));

    m_flagF = (typeAndFlags & 0x1) != 0;
    QCC_DbgPrintf(("WhoHas::Deserialize(): F flag %d", m_flagF));

    //
    // The second octet is the count of bus names.
    //
    uint8_t numberNames = buffer[1];
    QCC_DbgPrintf(("WhoHas::Deserialize(): Count %d", numberNames));
    size += 1;

    //
    // From this point on, things are not at fixed addresses
    //
    uint8_t const* p = &buffer[2];
    bufsize -= 2;

    //
    // Now we need to read out <numberNames> names that the packet has told us
    // will be there.
    //
    for (uint32_t i = 0; i < numberNames; ++i) {
        QCC_DbgPrintf(("WhoHas::Deserialize(): StringData::Deserialize() name %d", i));
        StringData stringData;

        //
        // Tell the string to read itself out.  If there's not enough buffer
        // it will complain by returning 0.  We pass the complaint on up.
        //
        size_t stringSize = stringData.Deserialize(p, bufsize);
        if (stringSize == 0) {
            QCC_DbgPrintf(("WhoHas::Deserialize(): StringData::Deserialize():  Error"));
            return 0;
        }

        AddName(stringData.Get());
        size += stringSize;
        p += stringSize;
        bufsize -= stringSize;
    }

    return size;
}

Header::Header()
    : m_version(0), m_timer(0), m_retries(0), m_tick(0)
{
}

Header::~Header()
{
}

void Header::SetVersion(uint8_t version)
{
    m_version = version;
}

uint8_t Header::GetVersion(void) const
{
    return m_version;
}

void Header::SetTimer(uint8_t timer)
{
    m_timer = timer;
}

uint8_t Header::GetTimer(void) const
{
    return m_timer;
}

void Header::AddQuestion(WhoHas question)
{
    m_questions.push_back(question);
}

uint32_t Header::GetNumberQuestions(void) const
{
    return m_questions.size();
}

WhoHas Header::GetQuestion(uint32_t index) const
{
    assert(index < m_questions.size());
    return m_questions[index];
}

void Header::GetQuestion(uint32_t index, WhoHas** question)
{
    assert(index < m_questions.size());
    *question = &m_questions[index];
}

void Header::AddAnswer(IsAt answer)
{
    m_answers.push_back(answer);
}

uint32_t Header::GetNumberAnswers(void) const
{
    return m_answers.size();
}

IsAt Header::GetAnswer(uint32_t index) const
{
    assert(index < m_answers.size());
    return m_answers[index];
}

void Header::GetAnswer(uint32_t index, IsAt** answer)
{
    assert(index < m_answers.size());
    *answer = &m_answers[index];
}

size_t Header::GetSerializedSize(void) const
{
    //
    // We have one octet for version, one four question count, one for answer
    // count and one for timer.  Four octets to start.
    //
    size_t size = 4;

    //
    // Let the questions data decide for themselves how long the question part
    // of the message will be.
    //
    for (uint32_t i = 0; i < m_questions.size(); ++i) {
        WhoHas whoHas = m_questions[i];
        size += whoHas.GetSerializedSize();
    }

    //
    // Let the answers decide for themselves how long the answer part
    // of the message will be.
    //
    for (uint32_t i = 0; i < m_answers.size(); ++i) {
        IsAt isAt = m_answers[i];
        size += isAt.GetSerializedSize();
    }

    return size;
}

size_t Header::Serialize(uint8_t* buffer) const
{
    QCC_DbgPrintf(("Header::Serialize(): to buffer 0x%x", buffer));
    //
    // We keep track of the size so testers can check coherence between
    // GetSerializedSize() and Serialize() and Deserialize().
    //
    size_t size = 0;

    //
    // The first octet is version
    //
    buffer[0] = m_version;;
    QCC_DbgPrintf(("Header::Serialize(): version = %d", m_version));
    size += 1;

    //
    // The second octet is the count of questions.
    //
    buffer[1] = static_cast<uint8_t>(m_questions.size());
    QCC_DbgPrintf(("Header::Serialize(): QCount = %d", m_questions.size()));
    size += 1;

    //
    // The third octet is the count of answers.
    //
    buffer[2] = static_cast<uint8_t>(m_answers.size());
    QCC_DbgPrintf(("Header::Serialize(): ACount = %d", m_answers.size()));
    size += 1;

    //
    // The fourth octet is the timer for the answers.
    //
    buffer[3] = m_timer;
    QCC_DbgPrintf(("Header::Serialize(): timer = %d", m_timer));
    size += 1;

    //
    // From this point on, things are not at fixed addresses
    //
    uint8_t* p = &buffer[4];

    //
    // Let the questions push themselves out.
    //
    for (uint32_t i = 0; i < m_questions.size(); ++i) {
        QCC_DbgPrintf(("Header::Serialize(): WhoHas::Serialize() question %d", i));
        WhoHas whoHas = m_questions[i];
        size_t questionSize = whoHas.Serialize(p);
        size += questionSize;
        p += questionSize;
    }

    //
    // Let the answers push themselves out.
    //
    for (uint32_t i = 0; i < m_answers.size(); ++i) {
        QCC_DbgPrintf(("Header::Serialize(): IsAt::Serialize() answer %d", i));
        IsAt isAt = m_answers[i];
        size_t answerSize = isAt.Serialize(p);
        size += answerSize;
        p += answerSize;
    }

    return size;
}

size_t Header::Deserialize(uint8_t const* buffer, uint32_t bufsize)
{
    //
    // If there's not enough room in the buffer to get the fixed part out then
    // bail (one byte of version, one byte of question count, one byte of answer
    // count and one byte of timer).
    //
    if (bufsize < 4) {
        QCC_DbgPrintf(("Header::Deserialize(): Insufficient bufsize %d", bufsize));
        return 0;
    }

    //
    // We keep track of the size so testers can check coherence between
    // GetSerializedSize() and Serialize() and Deserialize().
    //
    size_t size = 0;

    //
    // The first octet is version
    //
    m_version = buffer[0];
    size += 1;

    //
    // The second octet is the count of questions.
    //
    uint8_t qCount = buffer[1];
    size += 1;

    //
    // The third octet is the count of answers.
    //
    uint8_t aCount = buffer[2];
    size += 1;

    //
    // The fourth octet is the timer for the answers.
    //
    m_timer = buffer[3];
    size += 1;

    //
    // From this point on, things are not at fixed addresses
    //
    uint8_t const* p = &buffer[4];
    bufsize -= 4;

    //
    // Now we need to read out <qCount> questions that the packet has told us
    // will be there.
    //
    for (uint8_t i = 0; i < qCount; ++i) {
        QCC_DbgPrintf(("Header::Deserialize(): WhoHas::Deserialize() question %d", i));
        WhoHas whoHas;

        //
        // Tell the question to read itself out.  If there's not enough buffer
        // it will complain by returning 0.  We pass the complaint on up.
        //
        size_t qSize = whoHas.Deserialize(p, bufsize);
        if (qSize == 0) {
            QCC_DbgPrintf(("Header::Deserialize(): WhoHas::Deserialize():  Error"));
            return 0;
        }
        m_questions.push_back(whoHas);
        size += qSize;
        p += qSize;
        bufsize -= qSize;
    }

    //
    // Now we need to read out <aCount> answers that the packet has told us
    // will be there.
    //
    for (uint8_t i = 0; i < aCount; ++i) {
        QCC_DbgPrintf(("Header::Deserialize(): IsAt::Deserialize() answer %d", i));
        IsAt isAt;

        //
        // Tell the answer to read itself out.  If there's not enough buffer
        // it will complain by returning 0.  We pass the complaint on up.
        //
        size_t aSize = isAt.Deserialize(p, bufsize);
        if (aSize == 0) {
            QCC_DbgPrintf(("Header::Deserialize(): IsAt::Deserialize():  Error"));
            return 0;
        }
        m_answers.push_back(isAt);
        size += aSize;
        p += aSize;
        bufsize -= aSize;
    }

    return size;
}

} // namespace ajn
