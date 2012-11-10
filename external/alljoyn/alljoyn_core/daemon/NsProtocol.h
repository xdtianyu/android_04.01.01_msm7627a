/**
 * @file
 * @internal
 * Data structures used for a lightweight service protocol.
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

#ifndef _NS_PROTOCOL_H
#define _NS_PROTOCOL_H

#ifndef __cplusplus
#error Only include NsProtocol.h in C++ code.
#endif

#include <vector>
#include <qcc/String.h>
#include <Status.h>

namespace ajn {

/**
 * @defgroup name_service_protocol Name Service Protocol
 * @{
 * <b>Introduction</b>
 *
 * One goal of AllJoyn is to allow clients of the bus to make Remote Procedure Calls
 * (RPC) or receive Signals from physically remote obejcts connected to the
 * bus as if they were local.  Collections of RCP and Signal signatures,
 * typically called interfaces.  Bus attachments are collections of interface
 * implementations and are described by so-called well-known or bus names.
 * Groups of one or more bus attachments are coordinated by AllJoyn daemon
 * processes that run on each host.  Physically or logically distributed AllJoyn
 * daemons may be merged into a single virtual bus.
 *
 * One of the fundamental issues in distributing processes across different
 * hosts is the discovering the address and port of a given service.  In the
 * case of AllJoyn, the communication endpoints of the various daemon processes
 * must be located so communication paths may be established.  This lightweight
 * name service protocol provides a definition of a protocol for such a
 * process.
 *
 * <b>Transport</b>
 *
 * Name service protocol messages are expected to be transported
 * over UDP, typically over a well-known multicast group and port.  A
 * UDP datagram carrying a name service message would appear like,
 *
 * @verbatim
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |        Source Port            |      Destination Port         |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |           Length              |           Checksum            |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |                      Name Service Packet                      |
 *     ~                                                               ~
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * @endverbatim
 *
 * <b>Strings</b>
 *
 * Since well-known names are strings, one of the fundamental objects in
 * the protocol is the StringData object.  Strings are encoded an octet
 * giving the length of the string, followed by some number UTF-8 characters.
 * (no terminating zero is required)  For example, the string "STRING"
 * would be encoded as follows.  The single octet length means that the
 * longest string possible is 255 characters.  This should not prove to
 * be a problem since it is the same maximum length as a domain name, on
 * which bus names are modeled.
 *
 * @verbatim
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |       6       |       S       |       T       |      R        |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                                               |
 *     |       I       |       N       |       G       |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * @endverbatim
 *
 * <b>IS-AT Message</b>
 *
 * The IS-AT message is an answer message used to advertise the existence
 * of a number of bus names on a given AllJoyn daemon.  IS-AT messages can
 * be sent as part of a response to a question, or they can be sent
 * gratuitously when an AllJoyn daemon decides to export the fact that it
 * supports some number of bus names.
 *
 * @verbatim
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |F S U T C G| M |     Count     |              Port             |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |            IPv4Address present if 'F' bit is set              |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |                                                               |
 *     |            IPv6Address present if 'S' bit is set              |
 *     |                                                               |
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |                                                               |
 *     ~       Daemon GUID StringData present if 'G' bit is set        ~
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |                                                               |
 *     ~            Variable Number of StringData Records              ~
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * @endverbatim
 *
 * @li @c M The message type of the IS-AT message.  Defined to be '01' (1).
 * @li @c G If '1' indicates that a variable length daemon GUID string is present.
 * @li @c C If '1' indicates that the list of StringData records is a complete
 *     list of all well-known names exported by the responding daemon.
 * @li @c T If '1' indicates that the responding daemon is listening on TCP.
 * @li @c U If '1' indicates that the responding daemon is listening on UDP.
 * @li @c S If '1' indicates that the responding daemon is listening on an IPv6
 *     address and that an IPv6 address is present in the message.  If '0'
 *     indicates is no IPv6 address present.
 * @li @c F If '1' indicates that the responding daemon is listening on an IPv4
 *     address and that an IPv4 address is present in the message.  If '0'
 *     indicates is no IPv4 address present.
 * @li @c Count The number of StringData items that follow.  Each StringData item
 *     describes one well-known bus name supported by the responding daemon.
 * @li @c Port The port on which the responding daemon is listening.
 * @li @c IPv4Address The IPv4 address on which the responding daemon is listening.
 *     Present if the 'F' bit is set to '1'.
 * @li @c IPv6Address The IPv6 address on which the responding daemon is listening.
 *     Present if the 'S' bit is set to '1'.
 *
 * <b>WHO-HAS Message</b>
 *
 * The WHO-HAS message is a question message used to ask AllJoyn daemons if they
 * support one or more bus names.
 *
 * @verbatim
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |F S U T R R| M |     Count     |                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 *     |                                                               |
 *     ~              Variable Number of StringData Records            ~
 *     |                                                               |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * @endverbatim
 *
 * @li @c M The message type of the WHO-HAS message.  Defined to be '10' (2)
 * @li @c R Reserved bit.
 * @li @c T If '1' indicates that the requesting daemon wants to connect using
 *     TCP.
 * @li @c U If '1' indicates that the requesting daemon wants to connect using
 *     UDP.
 * @li @c S If '1' indicates that the responding daemon is interested in receiving
 *     information about services accessible via IPv6 addressing.
 * @li @c F If '1' indicates that the responding daemon is interested in receiving
 *     information about services accessible via IPv4 addressing.
 * @li @c Count The number of StringData items that follow.  Each StringData item
 *     describes one well-known bus name that the querying daemon is interested in.
 *
 * <b>Messages<b>
 *
 * A name service message consists of a header, followed by a variable
 * number of question (Q) messages (for example, WHO-HAS) followed by a variable
 * number of answer(A) messages (for example, IS-AT).  All messages are packed
 * to octet boundaries.
 *
 * <b>Name Service Header</b>
 *
 * @verbatim
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |    Version    |    QCount     |    ACount     |     Timer     |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * @endverbatim
 *
 * @li @c Version The version of the protocol.
 * @li @c QCount The number of question messages that follow the header.
 * @li @c ACount The number of question messages that follow the question
 *     messages.
 * @li @c Timer A count of seconds for which any answers should be considered
 *     valid.  A zero in this field means that the sending daemon is
 *     withdrawing the advertisements.  A value of 255 in this field means
 *     "forever," or at least until withdrawn
 *
 * <b>Example</b>
 *
 * It is expected that when an AllJoyn daemon comes up, it will want to advertise
 * the fact that it is up and supports some number of bus names.  It may also
 * be the case that the AllJoyn daemon already knows that it will need to find
 * some remote bus names when it comes up.  In this case, it can send an initial
 * message that both announces its bus names, and asks for any other AllJoyn
 * daemons that support what it wants.
 *
 * Consider a daemon that is capable of dealing with IPv4 addresses, and is
 * listening on Port 9955 of IPv4 address "192.168.10.10" for incoming
 * connections.  If the daemon is interested in locating another daemons that
 * supports the bus name "org.yadda.foo" and wants to export the fact that it
 * supports the bus name, "org.yadda.bar", and will support than name forever,
 * it might send a packet that combines WHO-HAS and IS-AT messages and that
 * looks something like
 *
 * @verbatim
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Version = 0   |  Q Count = 1  |  A Count = 1  | Timer = 255   |   (A)
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |   WHO-HAS     |   Count = 1   |   Count = 13  |     'o'       |   (B) (C)
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
 *     |     'r'             'g'             '.'             'y'       |
 *     |                                                               |
 *     |     'a'             'd'             'd'             'a'       |
 *     |                                                               |
 *     |     '.'             'f'             'o'             'o'       |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |     IS-AT     |  Count = 1    |  Port = 9955  |      192      |  (D)
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |      168             10              10       |  Count = 13   |  (E)
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |      'o'            'r'             'g'             '.'       |
 *     |                                                               |
 *     |      'y'            'a'             'd'             'd'       |
 *     |                                                               |
 *     |      'a'            '.'             'b'             'a'       |
 *     |               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |      'r'      |
 *     +-+-+-+-+-+-+-+-+
 * @endverbatim
 *
 * The notation (A) indicates the name service header.  This header tells
 * us that the version is zero, there is one question and one answer
 * message following, and that the timeout value of any answer messages
 * present in this message is set to be 255 (infinite).
 *
 * The (B) section shows one question, which is a WHO-HAS message.  The
 * flags present in the WHO-HAS message are not shown here. What follows
 * is the count of bus names present in this message (1).  The bus name
 * shown here is "org.yadda.foo" (C).  This name is contained in a
 * serialized SDATA (string data) message.  The length of the string is
 * thirteen bytes, and the characters of the string follow.  This ends
 * the question section of the messge since there was only one question
 * present as indicated by the header.
 *
 * Next, the (D) notation shows the single answer message described in
 * the header.  Answer messages are called IS-AT messages.  There is a
 * count of one bus name in the IS-AT messsage which, the message is
 * telling us, can be found at port 9955 of IPv4 address 192.168.10.10
 * which come next in the serialized message.  The single SDATA record
 * (E) with a count of 13 indicates that the sending daemon supports the
 * bus name "org.yadda.bar" at that address.
 * @} End of "defgroup name_service_protocol"
 */

/**
 * @internal
 * @brief An abstract data type defining the operations that each element
 * of a name service protocol must implement.
 *
 * Every instance of a piece of the name service protocol must
 * have the capability of being serialized into a datagram and deserialized
 * from a datagram.  It is also useful to be able to query an existing
 * object for how much buffer space it and its children will need in order
 * to be successfully serialized.
 *
 * This ADT class "enforces" the signatures of these functions.
 *
 * @ingroup name_service_protocol
 */
class ProtocolElement {
  public:
    /**
     * Virtual destructor for derivable class.
     */
    virtual ~ProtocolElement() { }

    /**
     * @internal
     * @brief Get the size of a buffer that will allow the object and all of
     * its children to be successfully serialized
     *
     * @return The size of the buffer required to serialize the object
     */
    virtual size_t GetSerializedSize(void) const = 0;

    /**
     * @internal
     * @brief Serialize this object and all of its children to the provided
     * buffer.
     *
     * @warning The buffer should be at least as large as the size returned
     * by GetSerializedSize().
     *
     * @return The number of octets written to the buffer.
     */
    virtual size_t Serialize(uint8_t* buffer) const = 0;

    /**
     * @internal
     * @brief Deserialize this object and all of its children from the provided
     * buffer.
     *
     * An object implementing this method will attempt to read its wire
     * representation and construct an object representation.  If there are
     * insuffucient bits in the buffer to do so, it will return a count of
     * zero bytes read.  There is very little redundancy in a wire representation
     * to provide information for error checking, so if the UDP checksum of
     * the containing datagram indicates a successfully received message, a
     * protocol error will typically be reported through a zero return count
     * here.
     *
     * @param buffer The buffer to read the bytes from.
     * @param bufsize The number of bytes available in the buffer.
     *
     * @return The number of octets read from the buffer, or zero if an error
     * occurred.
     */
    virtual size_t Deserialize(uint8_t const* buffer, uint32_t bufsize) = 0;
};

/**
 * @internal
 * @brief A class representing a name service StringData object.
 *
 * Since well-known names are strings, one of the fundamental objects in
 * the protocol is the StringData object.  Strings are encoded an octet
 * giving the length of the string, followed by some number UTF-8 characters.
 * (no terminating zero is required)  For example, the string "STRING"
 * would be encoded as follows.  The single octet length means that the
 * longest string possible is 255 characters.  This should not prove to
 * be a problem since it is the same maximum length as a domain name, on
 * which bus names are modeled.
 *
 * @ingroup name_service_protocol
 */
class StringData : public ProtocolElement {
  public:
    /**
     * @internal
     * @brief Construct a StringData object.  The size of the string represented
     * by the object is initialized to 0 -- the null string.
     */
    StringData();

    /**
     * @internal
     * @brief Destroy a StringData object.
     */
    ~StringData();

    /**
     * @internal
     * @brief Set the string represented by this StringData object to the
     * provided string.
     *
     * @param string The well-known or bus name to be represented by this object.
     */
    void Set(qcc::String string);

    /**
     * @internal
     * @brief Get the string represented by this StringData object.
     *
     * @return The well-known or bus name to represented by this object.
     */
    qcc::String Get(void) const;

    /**
     * @internal
     * @brief Get the size of a buffer that will allow the object to be
     * successfully serialized
     *
     * @return The size of the buffer required to serialize the object
     */
    size_t GetSerializedSize(void) const;

    /**
     * @internal
     * @brief Serialize this string to the provided buffer.
     *
     * @warning The buffer should be at least as large as the size returned
     * by GetSerializedSize().
     *
     * @return The number of octets written to the buffer.
     */
    size_t Serialize(uint8_t* buffer) const;

    /**
     * @internal
     * @brief Deserialize this string from the provided buffer.
     *
     * @see ProtocolElement::Deserialize()
     *
     * @param buffer The buffer to read the bytes from.
     * @param bufsize The number of bytes available in the buffer.
     *
     * @return The number of octets read from the buffer, or zero if an error
     * occurred.
     */
    size_t Deserialize(uint8_t const* buffer, uint32_t bufsize);

  private:
    /**
     * @internal
     * @brief The in-memory representation of the StringData object.
     */
    qcc::String m_string;

    /**
     * @internal
     * @brief The size of the represented StringData object.
     */
    size_t m_size;
};

/**
 * @internal
 * @brief A class representing an authoritative answer in the name service
 * protocol.
 *
 * The IS-AT message is an answer message used to advertise the existence
 * of a number of bus names on a given AllJoyn daemon.  IS-AT messages can
 * be sent as part of a response to a direct question, or they can be sent
 * gratuitously when an AllJoyn daemon decides to export the fact that it
 * supports some number of bus names.
 *
 * @ingroup name_service_protocol
 */
class IsAt : public ProtocolElement {
  public:
    /**
     * @internal
     * @brief Construct an in-memory object representation of an on-the-wire
     * name service protocol answer.
     */
    IsAt();

    /**
     * @internal
     * @brief Destroy a name service protocol answer object.
     */
    ~IsAt();

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this answer is providing its entire well-known name list.
     *
     * @param flag True if the daemon is providing the entire well-known name
     * list.
     */
    void SetCompleteFlag(bool flag) { m_flagC = flag; }

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this answer is providing its entire well-known name list.
     *
     * @param flag True if the daemon is providing the entire well-known name
     * list.
     */
    bool GetCompleteFlag(void) const { return m_flagC; }

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this answer is listening on a TCP socket.
     *
     * @param flag True if the daemon is listening on TCP.
     */
    void SetTcpFlag(bool flag) { m_flagT = flag; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this answer is listening on a TCP socket.
     *
     * @return True if the daemon is listening on TCP.
     */
    bool GetTcpFlag(void) const { return m_flagT; }

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this answer is listening on a UDP socket.
     *
     * @param flag True if the daemon is listening on UDP.
     */
    void SetUdpFlag(bool flag) { m_flagU = flag; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this answer is listening on a UDP socket.
     *
     * @return True if the daemon is listening on UDP.
     */
    bool GetUdpFlag(void) const { return m_flagU; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this answer has provided a GUID string.
     *
     * This flag is set to true by the SetGuid() call (or deserializing
     * a message for which the flag was set) and indicates that a daemon
     * GUID is provided in the message.
     *
     * @return True if the daemon has provided a GUID.
     */
    bool GetGuidFlag(void) const { return m_flagG; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this answer is listening on an IPv6 address.
     *
     * This flag is set to true by the SetIPv6() call (or deserializing
     * a message for which the flag was set) and indicates that an IPv6
     * address is provided in the message.
     *
     * @return True if the daemon is listening on IPv6.
     */
    bool GetIPv6Flag(void) const { return m_flagS; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this answer is listening on an IPv4 address.
     *
     * This flag is set to true by the SetIPv4() call (or deserializing
     * a message for which the flag was set) and indicates that an IPv4
     * address is provided in the message.
     *
     * @return True if the daemon is listening on IPv4.
     */
    bool GetIPv4Flag(void) const { return m_flagF; }

    /**
     * @internal
     * @brief Set the GUID string for the responding name service.
     *
     * This method takes a string with the name service guid in "presentation" format
     * and  arranges for it to be written out in the protocol message.  Although
     * this is typically a string representation of a GUID, the string is not
     * interpreted by the protocol and can be used to carry any global (not
     * related to an individual well-known name) information related to the
     * generating daemon.
     *
     * @param guid The name service GUID string of the responding name service.
     */
    void SetGuid(const qcc::String& guid);

    /**
     * @internal
     * @brief Get the name service GUID string for the responding daemon.
     *
     * This method returns a string with the guid in "presentation" format.
     * Although this is typically a string representation of a GUID, the string
     * is not interpreted by the protocol and can be used to carry any global
     * (not related to an infividual well-known name) information related to the
     * generating daemon.
     *
     * @return The GUID string of the responding name service.
     */
    qcc::String GetGuid(void) const;

    /**
     * @internal
     * @brief Set the port on which the daemon generating this answer is
     * listening.
     *
     * @param port The port on which the daemon is listening.
     */
    void SetPort(uint16_t port);

    /**
     * @internal
     * @brief Get the port on which the daemon generating this answer is
     * listening.
     *
     * @return The port on which the daemon is listening.
     */
    uint16_t GetPort(void) const;

    /**
     * @internal
     * @brief Clear the IPv4 address.
     */
    void ClearIPv4(void);

    /**
     * @internal
     * @brief Set the IPv4 address on which the daemon generating this answer
     * is listening.
     *
     * This method takes an IPv4 address string in presentation format and
     * arranges for it to be written out in the protocol message in network
     * format (32-bits, big endian).  It also has the side-effect of setting
     * the IPv4 flag.
     *
     * @param ipv4Addr The IPv4 address on which the daemon is listening.
     */
    void SetIPv4(qcc::String ipv4Addr);

    /**
     * @internal
     * @brief Get the IPv4 address on which the daemon generating this answer
     * is listening.
     *
     * This method returns an IPv4 address string in presentation format.
     * If the IPv4 flag is not set, the results are undefined.
     *
     * @return The IPv4 address on which the daemon is listening.
     */
    qcc::String GetIPv4(void) const;

    /**
     * @internal
     * @brief Clear the IPv6 address.
     */
    void ClearIPv6(void);

    /**
     * @internal
     * @brief Set the IPv6 address on which the daemon generating this answer
     * is listening.
     *
     * This method takes an IPv6 address string in presentation format and
     * arranges for it to be written out in the protocol message in network
     * format (128-bits, big endian).  It also has the side-effect of setting
     * the IPv6 flag.
     *
     * @param ipv6Addr The IPv6 address on which the daemon is listening.
     */
    void SetIPv6(qcc::String ipv6Addr);

    /**
     * @internal
     * @brief Get the IPv6 address on which the daemon generating this answer
     * is listening.
     *
     * This method returns an IPv6 address string in presentation format.
     * If the IPv6 flag is not set, the results are undefined.
     *
     * @return The IPv6 address on which the daemon is listening.
     */
    qcc::String GetIPv6(void) const;

    /**
     * @internal
     * @brief Add a string representing a well-known or bus name to the answer.
     *
     * This method adds a well-known or bus name and adds it to an internal
     * list of authoritative answers regarding the names supported by the
     * calling daemon.  These names will be serialized to an answer message
     *  as StringData objects.
     *
     * @param name A well-known or bus name which the daemon supports.
     */
    void AddName(qcc::String name);

    /**
     * @internal
     * @brief Get the number of well-known or bus names represented by this
     * object.
     *
     * This method returns the number of well-known bus names from the internal
     * list of authoritative answers regarding the names supported by the
     * responding daemon.  These names are typically deserialized from an
     * answer received over the network.
     *
     * @see GetName()
     *
     * @return The number of well-known names represented by this answer
     * object.
     */
    uint32_t GetNumberNames(void) const;

    /**
     * @internal
     * @brief Get a string representing a well-known or bus name.
     *
     * This method returns one of the well-known or bus names from the internal
     * list of authoritative answers regarding the names supported by the
     * responding daemon.  These names are typically deserialized from an answer
     *  received over the network.
     *
     * The number of entries in the list (used to determine legal values for the
     * index) is found by calling GetNumberNames()
     *
     * @see GetNumberNames()
     *
     * @param name The index of the name to retrieve.
     *
     * @return The well-known or bus name at the provided index.
     */
    qcc::String GetName(uint32_t index) const;

    /**
     * @internal
     * @brief Get the size of a buffer that will allow the answer object and
     * all of its children StringData objects to be successfully serialized.
     *
     * @return The size of the buffer required to serialize the object
     */
    size_t GetSerializedSize(void) const;

    /**
     * @internal
     * @brief Serialize this answer and all of its children StringData objects
     * to the provided buffer.
     *
     * @warning The buffer should be at least as large as the size returned
     * by GetSerializedSize().
     *
     * @return The number of octets written to the buffer.
     */
    size_t Serialize(uint8_t* buffer) const;

    /**
     * @internal
     * @brief Deserialize this answer from the provided buffer.
     *
     * @see ProtocolElement::Deserialize()
     *
     * @param buffer The buffer to read the bytes from.
     * @param bufsize The number of bytes available in the buffer.
     *
     * @return The number of octets read from the buffer, or zero if an error
     * occurred.
     */
    size_t Deserialize(uint8_t const* buffer, uint32_t bufsize);

  private:
    bool m_flagG;
    bool m_flagC;
    bool m_flagT;
    bool m_flagU;
    bool m_flagS;
    bool m_flagF;
    uint16_t m_port;
    qcc::String m_guid;
    qcc::String m_ipv4;
    qcc::String m_ipv6;
    std::vector<qcc::String> m_names;
};

/**
 * @internal
 * @brief A class representing a question in the name service protocol.
 *
 * The WHO-HAS message is a question message used to ask AllJoyn daemons if they
 * support one or more bus names.
 *
 * @ingroup name_service_protocol
 */
class WhoHas : public ProtocolElement {
  public:

    /**
     * @internal
     * @brief Construct an in-memory object representation of an on-the-wire
     * name service protocol question.
     */
    WhoHas();

    /**
     * @internal
     * @brief Destroy a name service protocol answer object.
     */
    ~WhoHas();

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this question is interested in hearing about daemons listening on a TCP
     * socket.
     *
     * @param flag True if the daemon is interested in TCP.
     */
    void SetTcpFlag(bool flag) { m_flagT = flag; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this question is interested in hearing about daemons listening on a TCP
     * socket.
     *
     * @return True if the daemon ways it is interested in TCP.
     */
    bool GetTcpFlag(void) const { return m_flagT; }

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this question is interested in hearing about daemons listening on a UDP
     * socket.
     *
     * @param flag True if the daemon is interested in UDP.
     */
    void SetUdpFlag(bool flag) { m_flagU = flag; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this question is interested in hearing about daemons listening on a UDP
     * socket.
     *
     * @return True if the daemon ways it is interested in UDP.
     */
    bool GetUdpFlag(void) const { return m_flagU; }

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this question is interested in IPv6 addresses.
     *
     * @param flag True if the daemon is interested on IPv6.
     */
    void SetIPv6Flag(bool flag) { m_flagS = flag; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this question is interested in IPv6 addresses.
     *
     * @return True if the daemon is interested on IPv6.
     */
    bool GetIPv6Flag(void) const { return m_flagS; }

    /**
     * @internal
     * @brief Set the protocol flag indicating that the daemon generating
     * this question is interested in IPv4 addresses.
     *
     * @param flag True if the daemon is interested on IPv4.
     */
    void SetIPv4Flag(bool flag) { m_flagF = flag; }

    /**
     * @internal
     * @brief Get the protocol flag indicating that the daemon generating
     * this question is interested in IPv4 addresses.
     *
     * @return True if the daemon is interested on IPv4.
     */
    bool GetIPv4Flag(void) const { return m_flagF; }

    /**
     * @internal
     * @brief Add a string representing a well-known or bus name to the
     * question.
     *
     * This method adds a well-known or bus name to an internal list of names
     * in which the calling daemon is interested.  These names will be
     * serialized to a question message as StringData objects.
     *
     * @param name The well-known or bus name which the daemon is interested.
     */
    void AddName(qcc::String name);

    /**
     * @internal
     * @brief Get the number of well-known or bus names represented by this
     * object.
     *
     * This method returns the number of well-known or bus names from the
     * internal list names that the questioning daemon is interested in.
     * These names are typically deserialized from a question received over the
     * network.
     *
     * @see GetName()
     *
     * @return The number of well-known interfaces represented by this question
     * object.
     */
    uint32_t GetNumberNames(void) const;

    /**
     * @internal
     * @brief Get a string representing a well-known or bus name.
     *
     * This method returns one of the well-known or bus names from the internal
     * list of names in which questioning daemon is interested.  These names
     * are typically deserialized from a question received over the network.
     *
     * The number of entries in the list (used to determine legal values for the
     * index) is found by calling GetNumberNames()
     *
     * @see GetNumberNames()
     *
     * @param name The index of the name to retrieve.
     *
     * @return The well-known or bus name at the provided index.
     */
    qcc::String GetName(uint32_t index) const;

    /**
     * @internal
     * @brief Get the size of a buffer that will allow the question object and
     * all of its children StringData objects to be successfully serialized.
     *
     * @return The size of the buffer required to serialize the object
     */
    size_t GetSerializedSize(void) const;

    /**
     * @internal
     * @brief Serialize this question and all of its children StringData objects
     * to the provided buffer.
     *
     * @warning The buffer should be at least as large as the size returned
     * by GetSerializedSize().
     *
     * @return The number of octets written to the buffer.
     */
    size_t Serialize(uint8_t* buffer) const;

    /**
     * @internal
     * @brief Deserialize a question wire-representation from the provided
     * buffer.
     *
     * @see ProtocolElement::Deserialize()
     *
     * @param buffer The buffer to read the bytes from.
     * @param bufsize The number of bytes available in the buffer.
     *
     * @return The number of octets read from the buffer, or zero if an error
     * occurred.
     */
    size_t Deserialize(uint8_t const* buffer, uint32_t bufsize);

  private:
    bool m_flagT;
    bool m_flagU;
    bool m_flagS;
    bool m_flagF;
    std::vector<qcc::String> m_names;
};

/**
 * @internal
 * @brief A class representing a message in the name service protocol.
 *
 * A name service message consists of a header, followed by a variable
 * number of question (Q) messages (for example, WHO-HAS) followed by a variable
 * number of answer(A) messages (for example, IS-AT).  All messages are packed
 * to octet boundaries.
 *
 * @ingroup name_service_protocol
 */
class Header : public ProtocolElement {
  public:

    /**
     * @internal
     * @brief Construct an in-memory object representation of an on-the-wire
     * name service protocol header.
     */
    Header();

    /**
     * @internal
     * @brief Destroy a name service protocol header object.
     */
    ~Header();

    /**
     * @internal
     * @brief Set the number of times this header has been sent on the wire.
     * This is not a perfect place for this information, but it is a very
     * convenient place.  This information is not part of the wire protocol.
     *
     * @param retries The number of times the header has been sent on the wire.
     */
    void SetRetries(uint32_t retries) { m_retries = retries; }

    /**
     * @internal
     * @brief Set the number of times this header has been sent on the wire.
     * This is not a perfect place for this information, but it is a very
     * convenient place.  This information is not part of the wire protocol.
     *
     * @return The number of times the header has been sent on the wire.
     */
    uint32_t GetRetries(void) { return m_retries; }

    /**
     * @internal
     * @brief Get the tick value representing the last time this header was sent
     * on the wire.  This is not a perfect place for this information, but it
     * is a very convenient place.  This information is not part of the wire
     * protocol.
     *
     * @param tick The last time the header was sent on the wire.
     */
    void SetRetryTick(uint32_t tick) { m_tick = tick; }

    /**
     * @internal
     * @brief Set the tick value representing the last time this header was sent
     * on the wire.  This is not a perfect place for this information, but it
     * is a very convenient place.  This information is not part of the wire
     * protocol.
     *
     * @return The last time the header was been sent on the wire.
     */
    uint32_t GetRetryTick(void) { return m_tick; }

    /**
     * @internal
     * @brief Set the version of the protocol message.
     *
     * @param version The version  (0 .. 255) of the protocol.
     */
    void SetVersion(uint8_t version);

    /**
     * @internal
     * @brief get the version of the protocol message.
     *
     * @return The version  (0 .. 255) of the protocol.
     */
    uint8_t GetVersion(void) const;

    /**
     * @internal
     * @brief Set the timer value for all answers present in the protocol
     * message.
     *
     * The timer value is typcally used to encode whether or not included
     * answer (IS-AT) messages indicate the establishment or withdrawal
     * of service advertisements.  A timer value of zero indicates that
     * the included answers are valid for zero seconds.  This implies
     * that the advertisements are no longer valid and should be withdrawn.
     *
     * A timer value of 255 indicates that the advertisements included in
     * the following IS-AT messages should be considered valid until they
     * are explicitly withdrawn.
     *
     * Other timer values indicate that the advertisements included are
     * are ephemeral and should not be considered valid for longer than
     * the number of seconds after which the message datagram containing
     * the header is received.
     *
     * @param timer The timer value (0 .. 255) for included answers.
     */
    void SetTimer(uint8_t timer);

    /**
     * @internal
     * @brief Get the timer value for all answers present in the protocol
     * message.
     *
     * @see SetTimer()
     *
     * @return  The timer value (0 .. 255) for included answers.
     */
    uint8_t GetTimer(void) const;

    /**
     * @internal
     * @brief Add a question object to the list of questions represented by
     * this header.
     *
     * This method adds a question (WhoHas) object to an internal list of
     * questions the calling daemon is asking.  These questions will be
     * serialized to on-the-wire question objects when the header is
     * serialized.
     *
     * @see class WhoHas
     *
     * @param whoHas The question object to add to the protocol message.
     */
    void AddQuestion(WhoHas whoHas);

    /**
     * @internal
     * @brief Get the number of question objects represented by this object.
     *
     * @see GetQuestion()
     *
     * @return The number of question objects represented by this header object.
     */
    uint32_t GetNumberQuestions(void) const;

    /**
     * @internal
     * @brief Get a Question object represented by this header object.
     *
     * This method returns one of the question objects from the internal list
     * of questions asked by a questioning daemon.  These questions are
     * typically automatically deserialized from a header received over the
     * network.
     *
     * The number of entries in the list (used to determine legal values for the
     * index) is found by calling GetNumberQuestions()
     *
     * @see GetNumberQuestions()
     *
     * @param name The index of the question to retrieve.
     *
     * @return The question object at the provided index.
     */
    WhoHas GetQuestion(uint32_t index) const;

    /**
     * @internal
     * @brief Get a pointer to an answer object represented by this header object.
     *
     * This method returns a pointer.  This is typically used if one needs to
     * rewrite the contents of a question object.  We can't just return the pointer
     * since C++ doesn't consider return types in overload resolution, so we have
     * to return it indirectly.
     *
     * @see GetQuestion()
     *
     * @param name The index of the question to retrieve.
     * @param answer A pointer to the returned pointer to the question object at
     *     the provided index.
     */
    void GetQuestion(uint32_t index, WhoHas** question);

    /**
     * @internal
     * @brief Add an answer object to the list of answers represented by
     * this header.
     *
     * This method adds an answer (IsAt) object to an internal list of
     * answers the calling daemon is providing.  These answers will be
     * serialized to on-the-wire answer objects when the header is
     * serialized.
     *
     * @see class IsAt
     *
     * @param isAt The answer object to add to the protocol message.
     */
    void AddAnswer(IsAt isAt);

    /**
     * @internal
     * @brief Get the number of answer objects represented by this object.
     *
     * @see GetAnswer()
     *
     * @return The number of answer objects represented by this header object.
     */
    uint32_t GetNumberAnswers(void) const;

    /**
     * @internal
     * @brief Get an answer object represented by this header object.
     *
     * This method returns one of the answer objects from the internal list
     * of answers provided by a responding daemon.  These answers are typically
     * automatically deserialized from a header received over the network.
     *
     * The number of entries in the list (used to determine legal values for the
     * index) is found by calling GetNumberAnswers()
     *
     * @see GetNumberAnswers()
     *
     * @param name The index of the answer to retrieve.
     *
     * @return The answer object at the provided index.
     */
    IsAt GetAnswer(uint32_t index) const;

    /**
     * @internal
     * @brief Get a pointer to an answer object represented by this header object.
     *
     * This method returns a pointer.  This is typically used if one needs to
     * rewrite the contents of an answer object.  We can't just return the pointer
     * since C++ doesn't consider return types in overload resolution, so we have
     * to return it indirectly.
     *
     * @see GetAnswer()
     *
     * @param name The index of the answer to retrieve.
     * @param answer A pointer to the returned pointer to the answer object at
     *     the provided index.
     */
    void GetAnswer(uint32_t index, IsAt** answer);

    /**
     * @internal
     * @brief Get the size of a buffer that will allow the header object and
     * all of its children questions and answer objects to be successfully
     * serialized.
     *
     * @return The size of the buffer required to serialize the object
     */
    size_t GetSerializedSize(void) const;

    /**
     * @internal
     * @brief Serialize this header and all of its children question and
     * answer objects to the provided buffer.
     *
     * @warning The buffer should be at least as large as the size returned
     * by GetSerializedSize().
     *
     * @return The number of octets written to the buffer.
     */
    size_t Serialize(uint8_t* buffer) const;

    /**
     * @internal
     * @brief Deserialize a header wire-representation and all of its children
     * questinos and answers from the provided buffer.
     *
     * @see ProtocolElement::Deserialize()
     *
     * @param buffer The buffer to read the bytes from.
     * @param bufsize The number of bytes available in the buffer.
     *
     * @return The number of octets read from the buffer, or zero if an error
     * occurred.
     */
    size_t Deserialize(uint8_t const* buffer, uint32_t bufsize);

  private:
    uint8_t m_version;
    uint8_t m_timer;
    uint32_t m_retries;
    uint32_t m_tick;
    std::vector<WhoHas> m_questions;
    std::vector<IsAt> m_answers;
};

} // namespace ajn

#endif // _NS_PROTOCOL_H
