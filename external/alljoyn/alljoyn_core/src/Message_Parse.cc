/**
 * @file
 *
 * This file implements the parsing side of _Message class
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

#include <algorithm>

#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Socket.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/Message.h>

#include "Router.h"
#include "KeyStore.h"
#include "LocalTransport.h"
#include "PeerState.h"
#include "CompressionRules.h"
#include "BusUtil.h"
#include "AllJoynCrypto.h"
#include "AllJoynPeerObj.h"
#include "SignatureUtils.h"
#include "BusInternal.h"

#define QCC_MODULE "ALLJOYN"

using namespace qcc;
using namespace std;

namespace ajn {

/*
 * A header size larger than anything we could reasonably expect
 */
#define MAX_HEADER_LEN  1024 * 64

/* Sized to avoid dynamic allocation for typical message calls */
#define DEFAULT_BUFFER_SIZE  1024

#define MIN_BUF_ADD   (DEFAULT_BUFFER_SIZE / 2)

#define VALID_HEADER_FIELD(f) (((f) > ALLJOYN_HDR_FIELD_INVALID) && ((f) < ALLJOYN_HDR_FIELD_UNKNOWN))



QStatus _Message::ParseArray(MsgArg* arg,
                             const char*& sigPtr)
{
    QStatus status;
    uint32_t len;
    const char* sigStart = sigPtr;

    /*
     * First check that the array type signature is valid
     */
    arg->typeId = ALLJOYN_ARRAY;
    status = SignatureUtils::ParseContainerSignature(*arg, sigPtr);
    if (status != ER_OK) {
        arg->typeId = ALLJOYN_INVALID;
        return status;
    }
    /*
     * Length is aligned on a 4 byte boundary
     */
    bufPos = AlignPtr(bufPos, 4);
    if (endianSwap) {
        len = EndianSwap32(*((uint32_t*)bufPos));
    } else {
        len = *((uint32_t*)bufPos);
    }
    /*
     * Check array length is valid and in bounds.
     */
    bufPos += 4;
    if ((len > ALLJOYN_MAX_ARRAY_LEN) || ((len + bufPos) > bufEOD)) {
        status = ER_BUS_BAD_LENGTH;
        QCC_LogError(status, ("Array length %ld at pos:%ld is too big", len, bufPos - bodyPtr - 4));
        arg->typeId = ALLJOYN_INVALID;
        return status;
    }
    QCC_DbgPrintf(("ParseArray len %ld at pos:%ld", len, bufPos - bodyPtr));
    /*
     * Note: at this point alignment is on a 4 bytes boundary so we only need to align values that
     * need 8 byte alignment.
     */
    switch (char elemTypeId = *sigStart) {
    case ALLJOYN_BYTE:
        arg->typeId = (AllJoynTypeId)((elemTypeId << 8) | ALLJOYN_ARRAY);
        arg->v_scalarArray.numElements = (size_t)len;
        arg->v_scalarArray.v_byte = bufPos;
        bufPos += len;
        break;

    case ALLJOYN_INT16:
    case ALLJOYN_UINT16:
        if ((len & 1) == 0) {
            arg->typeId = (AllJoynTypeId)((elemTypeId << 8) | ALLJOYN_ARRAY);
            arg->v_scalarArray.numElements = (size_t)(len / 2);
            if (endianSwap) {
                arg->v_scalarArray.v_uint16 = new uint16_t[arg->v_scalarArray.numElements];
                uint16_t* p = (uint16_t*)arg->v_scalarArray.v_uint16;
                uint16_t* n = (uint16_t*)bufPos;
                for (size_t i = 0; i < arg->v_scalarArray.numElements; i++) {
                    *p++ = EndianSwap16(*n++);
                }
                arg->flags = MsgArg::OwnsData;
            } else {
                arg->v_scalarArray.v_uint16 = (uint16_t*)bufPos;
            }
            bufPos += len;
        } else {
            status = ER_BUS_BAD_LENGTH;
        }
        break;

    case ALLJOYN_BOOLEAN:
        if ((len & 3) == 0) {
            size_t num = (size_t)(len / 4);
            bool* bools = new bool[num];
            for (size_t i = 0; i < num; i++) {
                uint32_t b = *bufPos;
                if (endianSwap) {
                    b = EndianSwap32(b);
                }
                if (b > 1) {
                    delete [] bools;
                    status = ER_BUS_BAD_VALUE;
                    break;
                }
                bools[i] = (b == 1);
                bufPos += 4;
            }
            /*
             * if status is set to ER_BUS_BAD_VALUE it means the for loop above
             * found that the value was not an ALLJOYN_BOOLEAN type and deleted
             * the array 'bools' then exited the for loop. Do not try and set
             * the 'v_scalarArray.v_bool' to the now invalid 'bools'.
             */
            if (status == ER_BUS_BAD_VALUE) {
                break;
            }
            arg->typeId = ALLJOYN_BOOLEAN_ARRAY;
            arg->v_scalarArray.numElements = num;
            arg->v_scalarArray.v_bool = bools;
            arg->flags = MsgArg::OwnsData;
        } else {
            status = ER_BUS_BAD_LENGTH;
        }
        break;

    case ALLJOYN_INT32:
    case ALLJOYN_UINT32:
        if ((len & 3) == 0) {
            arg->typeId = (AllJoynTypeId)((elemTypeId << 8) | ALLJOYN_ARRAY);
            arg->v_scalarArray.numElements = (size_t)(len / 4);
            if (endianSwap) {
                arg->v_scalarArray.v_uint32 = new uint32_t[arg->v_scalarArray.numElements];
                uint32_t* p = (uint32_t*)arg->v_scalarArray.v_uint32;
                uint32_t* n = (uint32_t*)bufPos;
                for (size_t i = 0; i < arg->v_scalarArray.numElements; i++) {
                    *p++ = EndianSwap32(*n++);
                }
                arg->flags = MsgArg::OwnsData;
            } else {
                arg->v_scalarArray.v_uint32 = (uint32_t*)bufPos;
            }
            bufPos += len;
        } else {
            status = ER_BUS_BAD_LENGTH;
        }
        break;

    case ALLJOYN_DOUBLE:
    case ALLJOYN_INT64:
    case ALLJOYN_UINT64:
        if ((len & 7) == 0) {
            arg->typeId = (AllJoynTypeId)((elemTypeId << 8) | ALLJOYN_ARRAY);
            arg->v_scalarArray.numElements = (size_t)(len / 8);
            bufPos = AlignPtr(bufPos, 8);
            arg->v_scalarArray.v_uint64 = (uint64_t*)bufPos;
            if (endianSwap) {
                arg->v_scalarArray.v_uint64 = new uint64_t[arg->v_scalarArray.numElements];
                uint64_t* p = (uint64_t*)arg->v_scalarArray.v_uint64;
                uint64_t* n = (uint64_t*)bufPos;
                for (size_t i = 0; i < arg->v_scalarArray.numElements; i++) {
                    *p++ = EndianSwap64(*n++);
                }
                arg->flags = MsgArg::OwnsData;
            } else {
                arg->v_scalarArray.v_uint64 = (uint64_t*)bufPos;
            }
            bufPos += len;
        } else {
            status = ER_BUS_BAD_LENGTH;
        }
        break;

    case ALLJOYN_STRUCT_OPEN:
    case ALLJOYN_DICT_ENTRY_OPEN:
        /*
         * The array length in bytes does not include the pad bytes between the length and the start
         * of the first element.
         */
        bufPos = AlignPtr(bufPos, 8);

    /* Falling through */
    default:
    {
        qcc::String elemSig(sigStart, sigPtr - sigStart);
        size_t numElements = 0;
        MsgArg* elements = NULL;
        if (len > 0) {
            /*
             * We know how many bytes there are in the array but not how many elements until we
             * unmarshal them.
             */
            uint8_t* endOfArray = bufPos + len;
            size_t capacity = 8;
            numElements = 0;
            elements = new MsgArg[capacity];
            /*
             * Loop until we have consumed all of the data bytes
             */
            while (bufPos < endOfArray) {
                if (numElements == capacity) {
                    capacity *= 2;
                    MsgArg* bigger = new MsgArg[capacity];
                    memcpy(bigger, elements, numElements * sizeof(MsgArg));
                    /*
                     * Clear the flags to prevent the destructor from freeing anything other
                     * than the MsgArgs.
                     */
                    for (size_t i = 0; i < numElements; i++) {
                        elements[i].flags = 0;
                    }
                    delete [] elements;
                    elements = bigger;
                }
                const char* esig = elemSig.c_str();
                status = ParseValue(&elements[numElements++], esig, true);
                if (status != ER_OK) {
                    break;
                }
            }
        }
        if (status == ER_OK) {
            arg->v_array.SetElements(elemSig.c_str(), numElements, elements);
            arg->flags |= MsgArg::OwnsArgs;
        } else {
            delete [] elements;
        }
    }
    break;
    }
    if (status != ER_OK) {
        arg->typeId = ALLJOYN_INVALID;
    }
    return status;
}


/*
 * Parse a STRUCT
 */
QStatus _Message::ParseStruct(MsgArg* arg, const char*& sigPtr)
{
    const char* memberSig = sigPtr;
    /*
     * First check that the struct type signature is valid
     */
    arg->typeId = ALLJOYN_STRUCT;
    QStatus status = SignatureUtils::ParseContainerSignature(*arg, sigPtr);
    if (status != ER_OK) {
        QCC_LogError(status, ("ParseStruct error in signature\n"));
        return status;
    }
    /*
     * Structs are aligned on an 8 byte boundary
     */
    bufPos = AlignPtr(bufPos, 8);

    QCC_DbgPrintf(("ParseStruct at pos:%d", bufPos - bodyPtr));

    arg->v_struct.members = new MsgArg[arg->v_struct.numMembers];
    arg->flags |= MsgArg::OwnsArgs;
    for (uint32_t i = 0; i < arg->v_struct.numMembers; ++i) {
        status = ParseValue(&arg->v_struct.members[i], memberSig);
        if (status != ER_OK) {
            arg->v_struct.numMembers = i;
            break;
        }
    }
    return status;
}


/*
 * Parse a DICT ENTRY
 */
QStatus _Message::ParseDictEntry(MsgArg* arg,
                                 const char*& sigPtr)
{
    const char* memberSig = sigPtr;
    /*
     * First check that the dict entry type signature is valid
     */
    arg->typeId = ALLJOYN_DICT_ENTRY;
    QStatus status = SignatureUtils::ParseContainerSignature(*arg, sigPtr);
    if (status != ER_OK) {
        arg->typeId = ALLJOYN_INVALID;
    } else {
        /*
         * Dict entries are aligned on an 8 byte boundary
         */
        bufPos = AlignPtr(bufPos, 8);

        QCC_DbgPrintf(("ParseDictEntry at pos:%d", bufPos - bodyPtr));

        arg->v_dictEntry.key = new MsgArg();
        arg->v_dictEntry.val = new MsgArg();
        arg->flags |= MsgArg::OwnsArgs;
        status = ParseValue(arg->v_dictEntry.key, memberSig);
        if (status == ER_OK) {
            status = ParseValue(arg->v_dictEntry.val, memberSig);
        }
    }
    return status;
}


QStatus _Message::ParseVariant(MsgArg* arg)
{
    QStatus status;

    arg->typeId = ALLJOYN_VARIANT;
    arg->v_variant.val = NULL;

    size_t len = (size_t)(*((uint8_t*)bufPos));
    const char* sigPtr = (char*)(++bufPos);

    bufPos += len;

    if (bufPos >= bufEOD) {
        status = ER_BUS_BAD_LENGTH;
    } else if (*bufPos++ != 0) {
        status = ER_BUS_BAD_SIGNATURE;
    } else {
        arg->v_variant.val = new MsgArg();
        arg->flags |= MsgArg::OwnsArgs;
        status = ParseValue(arg->v_variant.val, sigPtr);
        if ((status == ER_OK) && (*sigPtr != 0)) {
            status = ER_BUS_BAD_SIGNATURE;
        }
    }
    if (status != ER_OK) {
        delete arg->v_variant.val;
        arg->typeId = ALLJOYN_INVALID;
    }
    return status;
}


QStatus _Message::ParseSignature(MsgArg* arg)
{
    QStatus status = ER_OK;
    arg->v_signature.len = (size_t)(*((uint8_t*)bufPos));
    arg->v_signature.sig = (char*)(++bufPos);
    bufPos += arg->v_signature.len;
    if (bufPos >= bufEOD) {
        status = ER_BUS_BAD_LENGTH;
    } else if (*bufPos++ != 0) {
        status = ER_BUS_NOT_NUL_TERMINATED;
    } else {
        arg->typeId = ALLJOYN_SIGNATURE;
    }
    return status;
}


QStatus _Message::ParseValue(MsgArg* arg, const char*& sigPtr, bool arrayElem)
{
    QStatus status = ER_OK;

    arg->Clear();
    switch (AllJoynTypeId typeId = (AllJoynTypeId)(*sigPtr++)) {
    case ALLJOYN_BYTE:
        arg->v_byte = *bufPos++;
        arg->typeId = typeId;
        break;

    case ALLJOYN_INT16:
    case ALLJOYN_UINT16:
        bufPos = AlignPtr(bufPos, 2);
        if (endianSwap) {
            arg->v_uint16 = EndianSwap16(*((uint16_t*)bufPos));
        } else {
            arg->v_uint16 = *((uint16_t*)bufPos);
        }
        bufPos += 2;
        arg->typeId = typeId;
        break;

    case ALLJOYN_BOOLEAN:
    {
        bufPos = AlignPtr(bufPos, 4);
        uint32_t v = *((uint32_t*)bufPos);
        if (endianSwap) {
            v = EndianSwap32(v);
        }
        if (v > 1) {
            status = ER_BUS_BAD_VALUE;
        } else {
            arg->v_bool = (v == 1);
            bufPos += 4;
            arg->typeId = typeId;
        }
    }
    break;

    case ALLJOYN_INT32:
    case ALLJOYN_UINT32:
        bufPos = AlignPtr(bufPos, 4);
        if (endianSwap) {
            arg->v_uint32 = EndianSwap32(*((uint32_t*)bufPos));
        } else {
            arg->v_uint32 = *((uint32_t*)bufPos);
        }
        bufPos += 4;
        arg->typeId = typeId;
        break;

    case ALLJOYN_DOUBLE:
    case ALLJOYN_UINT64:
    case ALLJOYN_INT64:
        bufPos = AlignPtr(bufPos, 8);
        if (endianSwap) {
            arg->v_uint64 = EndianSwap64(*((uint64_t*)bufPos));
        } else {
            arg->v_uint64 = *((uint64_t*)bufPos);
        }
        bufPos += 8;
        arg->typeId = typeId;
        break;

    case ALLJOYN_OBJECT_PATH:
    case ALLJOYN_STRING:
        bufPos = AlignPtr(bufPos, 4);
        if (endianSwap) {
            arg->v_string.len = (size_t)EndianSwap32(*((uint32_t*)bufPos));
        } else {
            arg->v_string.len = (size_t)(*((uint32_t*)bufPos));
        }
        if (arg->v_string.len > ALLJOYN_MAX_PACKET_LEN) {
            QCC_LogError(status, ("String length %ld at pos:%ld is too big", arg->v_string.len, bufPos - bodyPtr));
            status = ER_BUS_BAD_LENGTH;
            break;
        }
        bufPos += 4;
        arg->v_string.str = (char*)bufPos;
        bufPos += arg->v_string.len;
        if (bufPos >= bufEOD) {
            status = ER_BUS_BAD_LENGTH;
        } else if (*bufPos++ != 0) {
            status = ER_BUS_NOT_NUL_TERMINATED;
        } else {
            arg->typeId = typeId;
        }
        break;

    case ALLJOYN_SIGNATURE:
        status = ParseSignature(arg);
        break;

    case ALLJOYN_ARRAY:
        status = ParseArray(arg, sigPtr);
        break;

    case ALLJOYN_DICT_ENTRY_OPEN:
        if (arrayElem) {
            status = ParseDictEntry(arg, sigPtr);
        } else {
            status = ER_BUS_BAD_SIGNATURE;
            QCC_LogError(status, ("Message arg parse error naked dicitionary element"));
        }
        break;

    case ALLJOYN_STRUCT_OPEN:
        status = ParseStruct(arg, sigPtr);
        break;

    case ALLJOYN_VARIANT:
        status = ParseVariant(arg);
        break;

    case ALLJOYN_HANDLE:
    {
        bufPos = AlignPtr(bufPos, 4);
        uint32_t index = *((uint32_t*)bufPos);
        if (endianSwap) {
            index = EndianSwap32(index);
        }
        uint32_t numHandles = (hdrFields.field[ALLJOYN_HDR_FIELD_HANDLES].typeId == ALLJOYN_INVALID) ? 0 : hdrFields.field[ALLJOYN_HDR_FIELD_HANDLES].v_uint32;
        if (index >=  numHandles) {
            status = ER_BUS_NO_SUCH_HANDLE;
        } else {
            arg->typeId = typeId;
            arg->v_handle.fd = handles[index];
            bufPos += 4;
        }
    }
    break;

    default:
        status = ER_BUS_BAD_VALUE_TYPE;
        break;
    }
    /*
     * Check we are not running of the end of the buffer
     */
    if ((status == ER_OK) && (bufPos > bufEOD)) {
        status = ER_BUS_BAD_SIGNATURE;
    }
    if (status != ER_OK) {
        QCC_LogError(status, ("Message arg parse error at or near %ld", bufPos - bodyPtr));
    } else {
        QCC_DbgPrintf(("Parse%s%s", SignatureUtils::IsBasicType(arg->typeId) ? " " : ":\n", arg->ToString().c_str()));
    }
    return status;
}

/*
 * The wildcard signature ("*") is used by test programs and for debugging.
 */
static const char* WildCardSignature = "*";

QStatus _Message::UnmarshalArgs(const qcc::String& expectedSignature, const char* expectedReplySignature)
{
    const char* sig = GetSignature();
    QStatus status = ER_OK;

    if (!bus->IsStarted()) {
        return ER_BUS_BUS_NOT_STARTED;
    }
    if (msgHeader.msgType == MESSAGE_INVALID) {
        return ER_FAIL;
    }
    if ((expectedSignature != sig) && (expectedSignature != WildCardSignature)) {
        status = ER_BUS_SIGNATURE_MISMATCH;
        QCC_LogError(status, ("Expected \"%s\" got \"%s\"", expectedSignature.c_str(), sig));
        return status;
    }
    if (msgHeader.bodyLen == 0) {
        if (*sig || (!expectedSignature.empty() && expectedSignature != WildCardSignature)) {
            status = ER_BUS_BAD_BODY_LEN;
            QCC_LogError(status, ("Expected a message body with signature %s", sig));
            return status;
        }
    }
    /*
     * Check we don't have two or more threads trying to unmarshall the message args at the same
     * time. Unlikely but definitely possible with broadcast signals and the bundled daemon.
     */
    if (IncrementAndFetch(&busy) > 1) {
        DecrementAndFetch(&busy);
        qcc::Sleep(1);
    }
    /*
     * Nothing to do if message args have already been unmarshalled.
     */
    if (msgArgs) {
        DecrementAndFetch(&busy);
        return ER_OK;
    }
    if (msgHeader.flags & ALLJOYN_FLAG_ENCRYPTED) {
        bool broadcast = (hdrFields.field[ALLJOYN_HDR_FIELD_DESTINATION].typeId == ALLJOYN_INVALID);
        size_t hdrLen = bodyPtr - (uint8_t*)msgBuf;
        PeerState peerState = bus->GetInternal().GetPeerStateTable()->GetPeerState(GetSender());
        KeyBlob key;
        status = peerState->GetKey(key, broadcast ? PEER_GROUP_KEY : PEER_SESSION_KEY);
        if (status != ER_OK) {
            QCC_LogError(status, ("Unable to decrypt message"));
            /*
             * This status triggers a call to the security failure handler.
             */
            status = ER_BUS_MESSAGE_DECRYPTION_FAILED;
            goto ExitUnmarshalArgs;
        }
        /*
         * Check remote peer is authorized to deliver us messagees of this message type.
         */
        if (!peerState->IsAuthorized((AllJoynMessageType)msgHeader.msgType, _PeerState::ALLOW_SECURE_RX)) {
            status = ER_BUS_NOT_AUTHORIZED;
            goto ExitUnmarshalArgs;
        }
        QCC_DbgHLPrintf(("Decrypting messge from %s", GetSender()));
        /*
         * Decryption will typically make the body length slightly smaller because the encryption
         * algorithm adds appends a MAC block to the end of the encrypted data.
         */
        size_t bodyLen = msgHeader.bodyLen;
        status = ajn::Crypto::Decrypt(*this, key, (uint8_t*)msgBuf, hdrLen, bodyLen);
        if (status != ER_OK) {
            goto ExitUnmarshalArgs;
        }
        msgHeader.bodyLen = static_cast<uint32_t>(bodyLen);
        authMechanism = key.GetTag();
    }
    /*
     * Calculate how many arguments there are
     */
    numMsgArgs = SignatureUtils::CountCompleteTypes(sig);
    msgArgs = new MsgArg[numMsgArgs];
    /*
     * Unmarshal the body values
     */
    bufPos = bodyPtr;
    for (uint8_t i = 0; i < numMsgArgs; i++) {
        status = ParseValue(&msgArgs[i], sig);
        if (status != ER_OK) {
            numMsgArgs = i;
            goto ExitUnmarshalArgs;
        }
    }
    if ((bufPos - bodyPtr) != static_cast<ptrdiff_t>(msgHeader.bodyLen)) {
        QCC_DbgHLPrintf(("UnmarshalArgs expected argLen %d got %d", msgHeader.bodyLen, (bufPos - bodyPtr)));
        status = ER_BUS_BAD_SIGNATURE;
    }

ExitUnmarshalArgs:

    if (status == ER_OK) {
        QCC_DbgPrintf(("Unmarshaled\n%s", ToString().c_str()));
        /*
         * If the message arguments are ever unmarshalled we convert the entire message to the native
         * endianess.
         */
        if (endianSwap) {
            QCC_DbgPrintf(("UnmarshalArgs converting to native endianess"));
            endianSwap = false;
            msgHeader.endian = myEndian;
        }
        /*
         * Save the reply signature so we can check it when we marshall the reply.
         */
        if (expectedReplySignature) {
            replySignature = expectedReplySignature;
        }
    } else {
        QCC_LogError(status, ("UnmarshalArgs failed"));
    }
    DecrementAndFetch(&busy);
    return status;
}



static QStatus PedanticCheck(const MsgArg* field, uint32_t fieldId)
{
    /*
     * Only checking strings
     */
    if (field->typeId != ALLJOYN_STRING) {
        return ER_OK;
    }
    switch (fieldId) {
    case ALLJOYN_HDR_FIELD_PATH:
        if (field->v_string.len > ALLJOYN_MAX_NAME_LEN) {
            return ER_BUS_NAME_TOO_LONG;
        }
        if (!IsLegalObjectPath(field->v_string.str)) {
            QCC_DbgPrintf(("Bad object path \"%s\"", field->v_string.str));
            return ER_BUS_BAD_OBJ_PATH;
        }
        break;

    case ALLJOYN_HDR_FIELD_INTERFACE:
        if (field->v_string.len > ALLJOYN_MAX_NAME_LEN) {
            return ER_BUS_NAME_TOO_LONG;
        }
        if (!IsLegalInterfaceName(field->v_string.str)) {
            QCC_DbgPrintf(("Bad interface name \"%s\"", field->v_string.str));
            return ER_BUS_BAD_INTERFACE_NAME;
        }
        break;

    case ALLJOYN_HDR_FIELD_MEMBER:
        if (field->v_string.len > ALLJOYN_MAX_NAME_LEN) {
            return ER_BUS_NAME_TOO_LONG;
        }
        if (!IsLegalMemberName(field->v_string.str)) {
            QCC_DbgPrintf(("Bad member name \"%s\"", field->v_string.str));
            return ER_BUS_BAD_MEMBER_NAME;
        }
        break;

    case ALLJOYN_HDR_FIELD_ERROR_NAME:
        if (field->v_string.len > ALLJOYN_MAX_NAME_LEN) {
            return ER_BUS_NAME_TOO_LONG;
        }
        if (!IsLegalInterfaceName(field->v_string.str)) {
            QCC_DbgPrintf(("Bad error name \"%s\"", field->v_string.str));
            return ER_BUS_BAD_ERROR_NAME;
        }
        break;

    case ALLJOYN_HDR_FIELD_SENDER:
    case ALLJOYN_HDR_FIELD_DESTINATION:
        if (field->v_string.len > ALLJOYN_MAX_NAME_LEN) {
            return ER_BUS_NAME_TOO_LONG;
        }
        if (!IsLegalBusName(field->v_string.str)) {
            QCC_DbgPrintf(("Bad bus name \"%s\"", field->v_string.str));
            return ER_BUS_BAD_BUS_NAME;
        }
        break;

    default:
        break;
    }
    return ER_OK;
}

/*
 * Maximuim number of bytes to pull in one go.
 */
static const size_t MAX_PULL = (128 * 1024);

/*
 * Timeout is scaled by the amount of data being read but is very conservative to allow for
 * congested Bluetooth links.
 */
#define PULL_TIMEOUT(num)  (20000 + num / 2)

/*
 * Pull exactly the number or bytes requested from the source
 */
static QStatus PullExact(Source& source,
                         void* buffer,
                         size_t numBytes,
                         qcc::SocketFd* fdList,
                         size_t maxFds,
                         size_t& numFds)
{
    QStatus status = ER_OK;
    uint8_t* bufPos = (uint8_t*)buffer;
    maxFds = 0;
    size_t bytesRead = 0;
    while (numBytes > 0) {
        size_t toRead = (std::min)(numBytes, MAX_PULL);
        if ((maxFds > 0) && (numFds == 0)) {
            numFds = maxFds;
            status = source.PullBytesAndFds(bufPos, toRead, bytesRead, fdList, maxFds, PULL_TIMEOUT(toRead));
            if ((status == ER_OK) && (numFds > 0)) {
                QCC_DbgHLPrintf(("Message was accompanied by %d handles", numFds));
            }
        } else {
            status = source.PullBytes(bufPos, toRead, bytesRead, PULL_TIMEOUT(toRead));
        }
        if (status != ER_OK) {
            /*
             * Once we have started to unmarshal a message we must finish so we
             * ignore alerts on the rx thread.
             */
            if (status == ER_ALERTED_THREAD) {
                QCC_LogError(status, ("PullExact ALERTED continuing"));
                continue;
            }
            QCC_DbgPrintf(("PullExact %s", QCC_StatusText(status)));
            break;
        }
        assert(bytesRead > 0);
        bufPos += bytesRead;
        numBytes -= bytesRead;
    }
    return status;
}

/*
 * Map from from wire protocol values to our enumeration type
 */
static const AllJoynFieldType FieldTypeMapping[] = {
    ALLJOYN_HDR_FIELD_INVALID,           /*  0 */
    ALLJOYN_HDR_FIELD_PATH,              /*  1 */
    ALLJOYN_HDR_FIELD_INTERFACE,         /*  2 */
    ALLJOYN_HDR_FIELD_MEMBER,            /*  3 */
    ALLJOYN_HDR_FIELD_ERROR_NAME,        /*  4 */
    ALLJOYN_HDR_FIELD_REPLY_SERIAL,      /*  5 */
    ALLJOYN_HDR_FIELD_DESTINATION,       /*  6 */
    ALLJOYN_HDR_FIELD_SENDER,            /*  7 */
    ALLJOYN_HDR_FIELD_SIGNATURE,         /*  8 */
    ALLJOYN_HDR_FIELD_HANDLES,           /*  9 */
    ALLJOYN_HDR_FIELD_UNKNOWN,           /* 10 */
    ALLJOYN_HDR_FIELD_UNKNOWN,           /* 11 */
    ALLJOYN_HDR_FIELD_UNKNOWN,           /* 12 */
    ALLJOYN_HDR_FIELD_UNKNOWN,           /* 13 */
    ALLJOYN_HDR_FIELD_UNKNOWN,           /* 14 */
    ALLJOYN_HDR_FIELD_UNKNOWN,           /* 14 */
    ALLJOYN_HDR_FIELD_TIMESTAMP,         /* 16 */
    ALLJOYN_HDR_FIELD_TIME_TO_LIVE,      /* 17 */
    ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN, /* 18 */
    ALLJOYN_HDR_FIELD_SESSION_ID,        /* 19 */
    ALLJOYN_HDR_FIELD_UNKNOWN            /* 20 */
};


/*
 * Perform consistency checks on the header
 */
QStatus _Message::HeaderChecks(bool pedantic)
{
    QStatus status = ER_OK;
    switch (msgHeader.msgType) {
    case MESSAGE_SIGNAL:
        if (hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].typeId == ALLJOYN_INVALID) {
            status = ER_BUS_INTERFACE_MISSING;
            break;
        }

    /* Falling through */
    case MESSAGE_METHOD_CALL:
        if (hdrFields.field[ALLJOYN_HDR_FIELD_PATH].typeId == ALLJOYN_INVALID) {
            status = ER_BUS_PATH_MISSING;
            break;
        }
        if (hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].typeId == ALLJOYN_INVALID) {
            status = ER_BUS_MEMBER_MISSING;
            break;
        }
        break;

    case MESSAGE_ERROR:
        if (hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].typeId == ALLJOYN_INVALID) {
            status = ER_BUS_ERROR_NAME_MISSING;
            break;
        }

    /* Falling through */
    case MESSAGE_METHOD_RET:
        if (hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].typeId == ALLJOYN_INVALID) {
            status = ER_BUS_REPLY_SERIAL_MISSING;
            break;
        }
        break;

    default:
        break;
    }
    /*
     * Check that the header field values have the correct types and are all well formed
     */
    if ((ER_OK == status) && pedantic) {
        for (uint32_t fieldId = ALLJOYN_HDR_FIELD_PATH; fieldId < ArraySize(hdrFields.field); fieldId++) {
            status = PedanticCheck(&hdrFields.field[fieldId], fieldId);
            if (status != ER_OK) {
                QCC_LogError(status, ("Invalid header field (fieldId=%d)", fieldId));
            }
        }
    }
    return status;
}

QStatus _Message::Unmarshal(RemoteEndpoint& endpoint, bool checkSender, bool pedantic, uint32_t timeout)
{
    QStatus status;
    size_t pktSize;
    uint8_t* endOfHdr;
    qcc::SocketFd fdList[qcc::SOCKET_MAX_FILE_DESCRIPTORS];
    size_t maxFds = endpoint.GetFeatures().handlePassing ? ArraySize(fdList) : 0;
    MsgArg* senderField = &hdrFields.field[ALLJOYN_HDR_FIELD_SENDER];
    Source& source = endpoint.GetSource();

    if (!bus->IsStarted()) {
        return ER_BUS_BUS_NOT_STARTED;
    }

    rcvEndpointName = endpoint.GetUniqueName();

    /*
     * Clear out any stale message state
     */
    msgBuf = NULL;
    delete [] _msgBuf;
    _msgBuf = NULL;
    ClearHeader();
    /*
     * Read the message header
     */
    size_t pulled;
    if (maxFds > 0) {
        numHandles = maxFds;
        status = source.PullBytesAndFds(&msgHeader, sizeof(MessageHeader), pulled, fdList, numHandles, timeout ? timeout : Event::WAIT_FOREVER);
    } else {
        numHandles = 0;
        status = source.PullBytes(&msgHeader, sizeof(MessageHeader), pulled, timeout ? timeout : Event::WAIT_FOREVER);
    }
    if (status != ER_OK) {
        goto ExitUnmarshal;
    }
    if (pulled < sizeof(MessageHeader)) {
        status = PullExact(source, (uint8_t*)(&msgHeader) + pulled, sizeof(MessageHeader) - pulled, fdList, maxFds, numHandles);
        if (status != ER_OK) {
            goto ExitUnmarshal;
        }
    }
    /*
     * Check if we need to swizzle the endianness
     */
    endianSwap = msgHeader.endian != myEndian;
    /*
     * Perform the endian swap on the header values and write the local process endianess into the
     * header.
     */
    if (endianSwap) {
        /*
         * Check we don't have a bogus header flag
         */
        if ((msgHeader.endian != ALLJOYN_LITTLE_ENDIAN) && (msgHeader.endian != ALLJOYN_BIG_ENDIAN)) {
            status = ER_BUS_BAD_HEADER_FIELD;
            QCC_LogError(status, ("Message header has invalid endian flag %d", msgHeader.endian));
            goto ExitUnmarshal;
        }
        msgHeader.bodyLen = EndianSwap32(msgHeader.bodyLen);
        msgHeader.serialNum = EndianSwap32(msgHeader.serialNum);
        msgHeader.headerLen = EndianSwap32(msgHeader.headerLen);
        QCC_DbgPrintf(("Incoming endianSwap"));
    }
    /*
     * Sanity check on the header size
     */
    if (msgHeader.headerLen > MAX_HEADER_LEN) {
        status = ER_BUS_BAD_HEADER_LEN;
        QCC_LogError(status, ("Message header length %d is invalid", msgHeader.headerLen));
        goto ExitUnmarshal;
    }
    /*
     * Calculate the size of the buffer we need
     */
    pktSize = ((msgHeader.headerLen + 7) & ~7) + msgHeader.bodyLen;
    /*
     * Check we are not exceeding the maximum allowed packet length. Note pktSize calc can
     * wraparound so we need to check the body length too.
     */
    if ((pktSize > ALLJOYN_MAX_PACKET_LEN) || (msgHeader.bodyLen > ALLJOYN_MAX_PACKET_LEN)) {
        status = ER_BUS_BAD_BODY_LEN;
        QCC_LogError(status, ("Message body length %d is invalid", msgHeader.bodyLen));
        goto ExitUnmarshal;
    }
    /*
     * Padding the end of the buffer ensures we can unmarshal a few bytes beyond the end of the
     * message reducing the places where we need to check for bufEOD when unmarshaling the body.
     */
    bufSize = sizeof(msgHeader) + ((pktSize + 7) & ~7) + sizeof(uint64_t);
    _msgBuf = new uint8_t[bufSize + 7];
    msgBuf = (uint64_t*)((uintptr_t)(_msgBuf + 7) & ~7); /* Align to 8 byte boundary */
    /*
     * Copy header into the buffer
     */
    memcpy(msgBuf, &msgHeader, sizeof(msgHeader));
    /*
     * Restore endianess in the buffered version of the message header.
     */
    if (endianSwap) {
        MessageHeader* hdr = (MessageHeader*)msgBuf;
        hdr->bodyLen = EndianSwap32(hdr->bodyLen);
        hdr->serialNum = EndianSwap32(hdr->serialNum);
        hdr->headerLen = EndianSwap32(hdr->headerLen);
    }
    bufPos = (uint8_t*)msgBuf + sizeof(msgHeader);
    bufEOD = bufPos + pktSize;
    endOfHdr = bufPos + msgHeader.headerLen;
    /*
     * Zero fill the pad at the end of the buffer
     */
    memset(bufEOD, 0, (uint8_t*)msgBuf + bufSize - bufEOD);

    QCC_DbgPrintf(("Msg type:%d headerLen: %d Attempting to read %d bytes", msgHeader.msgType, msgHeader.headerLen, pktSize));

    status = PullExact(source, bufPos, pktSize, fdList, maxFds, numHandles);
    if (status != ER_OK) {
        goto ExitUnmarshal;
    }
    /*
     * Parse the received header fields - each header starts on an 8 byte boundary
     */
    while (bufPos < endOfHdr) {
        bufPos = AlignPtr(bufPos, 8);
        AllJoynFieldType fieldId = (*bufPos >= ArraySize(FieldTypeMapping)) ? ALLJOYN_HDR_FIELD_UNKNOWN : FieldTypeMapping[*bufPos];
        if (++bufPos > endOfHdr) {
            break;
        }
        /*
         * An invalid field type is an error
         */
        if (fieldId == ALLJOYN_HDR_FIELD_INVALID) {
            status = ER_BUS_BAD_HEADER_FIELD;
            goto ExitUnmarshal;
        }
        size_t sigLen = (size_t)(*bufPos++);
        const char* sigPtr = (char*)(bufPos);
        /*
         * Skip over the signature
         */
        bufPos += 1 + sigLen;
        if (bufPos > endOfHdr) {
            break;
        }
        if (fieldId == ALLJOYN_HDR_FIELD_UNKNOWN) {
            MsgArg unknownHdr;
            /*
             * Unknown fields are parsed but otherwise ignored
             */
            status = ParseValue(&unknownHdr, sigPtr);
        } else {
            /*
             * Currently all header fields have a single character type code
             */
            if ((sigLen != 1) || (sigPtr[0] != HeaderFields::FieldType[fieldId]) || (sigPtr[1] != 0)) {
                status = ER_BUS_BAD_HEADER_FIELD;
            } else {
                status = ParseValue(&hdrFields.field[fieldId], sigPtr);
            }
        }
        if (*sigPtr != 0) {
            status = ER_BUS_BAD_HEADER_FIELD;
        }
        if (status != ER_OK) {
            goto ExitUnmarshal;
        }
    }
    if (bufPos != endOfHdr) {
        status = ER_BUS_BAD_HEADER_LEN;
        QCC_LogError(status, ("Unmarshal bad header length %d != %d\n", bufPos - (uint8_t*)msgBuf, msgHeader.headerLen));
        goto ExitUnmarshal;
    }
    /*
     * Header is always padded to end on an 8 byte boundary
     */
    bufPos = AlignPtr(bufPos, 8);
    bodyPtr = bufPos;
    /*
     * If header is compressed try to expand it
     */
    if (msgHeader.flags & ALLJOYN_FLAG_COMPRESSED) {
        uint32_t token = hdrFields.field[ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN].v_uint32;
        QCC_DbgPrintf(("Expanding compressed header token %u", token));
        if (hdrFields.field[ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN].typeId == ALLJOYN_INVALID) {
            status = ER_BUS_MISSING_COMPRESSION_TOKEN;
            goto ExitUnmarshal;
        }
        const HeaderFields* expFields = bus->GetInternal().GetCompressionRules()->GetExpansion(token);
        if (!expFields) {
            QCC_DbgPrintf(("No expansion for token %u", token));
            status = ER_BUS_CANNOT_EXPAND_MESSAGE;
            goto ExitUnmarshal;
        }
        /*
         * Expand the compressed fields. Don't overwrite headers we received in the message.
         */
        for (size_t id = 0; id < ArraySize(hdrFields.field); id++) {
            if (HeaderFields::Compressible[id] && (hdrFields.field[id].typeId == ALLJOYN_INVALID)) {
                hdrFields.field[id] = expFields->field[id];
            }
        }
        hdrFields.field[ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN].typeId = ALLJOYN_INVALID;
    }
    /*
     * Check the validity of the message header
     */
    status = HeaderChecks(pedantic);
    /*
     * Check if there are handles accompanying this message and if we expect them.
     */
    if (status == ER_OK) {
        const uint32_t expectFds = (hdrFields.field[ALLJOYN_HDR_FIELD_HANDLES].typeId == ALLJOYN_INVALID) ? 0 : hdrFields.field[ALLJOYN_HDR_FIELD_HANDLES].v_uint32;
        if (!endpoint.GetFeatures().handlePassing) {
            /*
             * Handles are not allowed if handle passing is not enabled.
             */
            if (expectFds || numHandles) {
                status = ER_BUS_HANDLES_NOT_ENABLED;
                QCC_LogError(status, ("Handle passing was not negotiated on this connection"));
            }
        } else if (expectFds != numHandles) {
            status = ER_BUS_HANDLES_MISMATCH;
            QCC_LogError(status, ("Wrong number of handles accompanied this message: expected %d got %d", expectFds, numHandles));
        }
    }
    if (status != ER_OK) {
        goto ExitUnmarshal;
    }
    /*
     * If we know the endpoint name we should check it
     */
    if (checkSender) {
        /*
         * If the message didn't specify a sender (unusual but unfortunately the spec allows it) or the
         * sender field is not the expected unique name we set the sender field.
         */
        if ((senderField->typeId == ALLJOYN_INVALID) || (rcvEndpointName != senderField->v_string.str)) {
            QCC_DbgHLPrintf(("Replacing missing or bad sender field %s by %s", senderField->ToString().c_str(), rcvEndpointName.c_str()));
            status = ReMarshal(rcvEndpointName.c_str());
        }
    }
    if (senderField->typeId != ALLJOYN_INVALID) {
        PeerState peerState = bus->GetInternal().GetPeerStateTable()->GetPeerState(senderField->v_string.str);
        bool unreliable = hdrFields.field[ALLJOYN_HDR_FIELD_TIME_TO_LIVE].typeId != ALLJOYN_INVALID;
        bool secure = (msgHeader.flags & ALLJOYN_FLAG_ENCRYPTED) != 0;
        /*
         * Check the serial number
         */
        if (!peerState->IsValidSerial(msgHeader.serialNum, secure, unreliable)) {
            /*
             * Treat all out-of-order or repeat messages specially.
             * This can happen even on reliable transports if message replies come in from a remote endpoint
             * after they have been timed out locally. It can also happen for broadcast messages on a distributed
             * bus when there are "circular" (redundant) connections between nodes.
             */
            status = ER_BUS_INVALID_HEADER_SERIAL;
            goto ExitUnmarshal;
        }
        /*
         * If the message has a timestamp turn it into an estimated local time
         */
        if (hdrFields.field[ALLJOYN_HDR_FIELD_TIMESTAMP].typeId != ALLJOYN_INVALID) {
            timestamp = peerState->EstimateTimestamp(hdrFields.field[ALLJOYN_HDR_FIELD_TIMESTAMP].v_uint32);
        } else {
            timestamp = qcc::GetTimestamp();
        }
        /*
         * If the message is unreliable check its timestamp has not expired.
         */
        if (unreliable) {
            ttl = hdrFields.field[ALLJOYN_HDR_FIELD_TIME_TO_LIVE].v_uint16;
            if (IsExpired()) {
                status = ER_BUS_TIME_TO_LIVE_EXPIRED;
                goto ExitUnmarshal;
            }
        }
    }

    /*
     * Toggle the autostart flag bit which is a 0 over the air but we prefer as a 1.
     */
    msgHeader.flags ^= ALLJOYN_FLAG_AUTO_START;


ExitUnmarshal:

    /*
     * If we unmarshaled handles we need to copy them into the message. Note we do this event if in
     * the case of an unmarshal error so the handles will be closed.
     */
    if (numHandles > 0) {
        handles = new qcc::SocketFd[numHandles];
        memcpy(handles, fdList, numHandles * sizeof(qcc::SocketFd));
    }
    switch (status) {
    case ER_OK:
        QCC_DbgHLPrintf(("Received %s via endpoint %s", Description().c_str(), rcvEndpointName.c_str()));
        QCC_DbgPrintf(("\n%s", ToString().c_str()));
        break;

    case ER_BUS_CANNOT_EXPAND_MESSAGE:
        /*
         * A compressed message could not be expanded so return the message as received and leave it
         * up to the upper-layer code to decide what to do. In most cases the upper-layer will queue
         * the message while it calls to the sender to get the needed expansion information.
         */
        QCC_DbgHLPrintf(("Received compressed message of len %d (via endpoint %s)\n%s", pktSize, rcvEndpointName.c_str(), ToString().c_str()));
        break;

    case ER_BUS_TIME_TO_LIVE_EXPIRED:
        /*
         * The message was succesfully unmarshalled but was stale so let the upper-layer decide
         * whether the error is recoverable or not.
         */
        QCC_DbgHLPrintf(("Time to live expired for (via endpoint %s) message:\n%s", rcvEndpointName.c_str(), ToString().c_str()));
        break;

    case ER_BUS_INVALID_HEADER_SERIAL:
        /*
         * The message was succesfully unmarshalled but was out-of-order so let the upper-layer
         * decide whether the error is recoverable or not.
         */
        QCC_DbgHLPrintf(("Serial number was invalid for (via endpoint %s) message:\n%s", rcvEndpointName.c_str(), ToString().c_str()));
        break;

    case ER_ALERTED_THREAD:
        /*
         * The rx thread was alerted before any data was read - just return this status code.
         */
        QCC_LogError(status, ("Message::Unmarshal rx thread was alerted for endpoint %s", endpoint.GetUniqueName().c_str()));
        break;

    default:
        /*
         * There was an unrecoverable failure while unmarshaling the message, cleanup before we return.
         */
        msgBuf = NULL;
        delete [] _msgBuf;
        _msgBuf = NULL;
        ClearHeader();
        if ((status != ER_SOCK_OTHER_END_CLOSED) && (status != ER_STOPPING_THREAD)) {
            QCC_LogError(status, ("Failed to unmarshal message received on %s", endpoint.GetUniqueName().c_str()));
        }
    }
    return status;
}

QStatus _Message::AddExpansionRule(uint32_t token, const MsgArg* expansionArg)
{
    /*
     * Validate the expansion response.
     */
    if (msgHeader.msgType != MESSAGE_METHOD_RET) {
        return ER_FAIL;
    }
    if (!expansionArg || !expansionArg->HasSignature("a(yv)")) {
        return ER_BUS_SIGNATURE_MISMATCH;
    }
    /*
     * Unpack the expansion into a standard header field structure.
     */
    QStatus status = ER_BUS_HDR_EXPANSION_INVALID;
    HeaderFields expFields;
    for (size_t i = 0; i < ArraySize(expFields.field); i++) {
        expFields.field[i].typeId = ALLJOYN_INVALID;
    }
    const MsgArg* field = expansionArg->v_array.elements;
    for (size_t i = 0; i < expansionArg->v_array.numElements; i++, field++) {
        const MsgArg* id = &(field->v_struct.members[0]);
        const MsgArg* variant =  &(field->v_struct.members[1]);
        /*
         * Note we don't assign the MsgArg because that will cause unnecessary string copies.
         */
        AllJoynFieldType fieldId = (id->v_byte >= ArraySize(FieldTypeMapping)) ? ALLJOYN_HDR_FIELD_UNKNOWN : FieldTypeMapping[id->v_byte];
        if (!HeaderFields::Compressible[fieldId]) {
            QCC_DbgPrintf(("Expansion has invalid field id %d", fieldId));
            goto ExitAddExpansion;
        }
        if (variant->v_variant.val->typeId != HeaderFields::FieldType[fieldId]) {
            QCC_DbgPrintf(("Expansion for field %d has wrong type %s", fieldId, variant->v_variant.val->ToString().c_str()));
            goto ExitAddExpansion;
        }
        switch (fieldId) {
        case ALLJOYN_HDR_FIELD_PATH:
            expFields.field[fieldId].typeId = ALLJOYN_OBJECT_PATH;
            expFields.field[fieldId].v_objPath.str = variant->v_variant.val->v_string.str;
            expFields.field[fieldId].v_objPath.len = variant->v_variant.val->v_string.len;
            break;

        case ALLJOYN_HDR_FIELD_INTERFACE:
        case ALLJOYN_HDR_FIELD_MEMBER:
        case ALLJOYN_HDR_FIELD_DESTINATION:
        case ALLJOYN_HDR_FIELD_SENDER:
            expFields.field[fieldId].typeId = ALLJOYN_STRING;
            expFields.field[fieldId].v_string.str = variant->v_variant.val->v_string.str;
            expFields.field[fieldId].v_string.len = variant->v_variant.val->v_string.len;
            break;

        case ALLJOYN_HDR_FIELD_SIGNATURE:
            expFields.field[fieldId].typeId = ALLJOYN_SIGNATURE;
            expFields.field[fieldId].v_signature.sig = variant->v_variant.val->v_signature.sig;
            expFields.field[fieldId].v_signature.len = variant->v_variant.val->v_signature.len;
            break;

        case ALLJOYN_HDR_FIELD_UNKNOWN:
            QCC_DbgPrintf(("Unknown header field %d in expansion", id->v_byte));
            goto ExitAddExpansion;

        default:
            expFields.field[fieldId] = *variant->v_variant.val;
            break;
        }
    }
    /*
     * Add the expansion to the compression engine.
     */
    bus->GetInternal().GetCompressionRules()->AddExpansion(expFields, token);
    return ER_OK;

ExitAddExpansion:
    return status;
}

}
