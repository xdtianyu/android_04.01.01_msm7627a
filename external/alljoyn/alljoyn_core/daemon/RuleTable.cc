/**
 * @file
 * MsgRouter is responsible for taking inbound messages and routing them
 * to an appropriate set of endpoints.
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

#include <cstring>

#include "RuleTable.h"

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <alljoyn/Message.h>

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

Rule::Rule(const char* ruleSpec, QStatus* outStatus) : type(MESSAGE_INVALID)
{
    QStatus status = ER_OK;
    const char* pos = ruleSpec;
    const char* finalPos = pos + strlen(ruleSpec);

    while (pos < finalPos) {
        const char* endPos = strchr(pos, ',');
        if (NULL == endPos) {
            endPos = finalPos;
        }
        const char* eqPos = strchr(pos, '=');
        if ((NULL == eqPos) || (eqPos >= endPos)) {
            status = ER_FAIL;
            QCC_LogError(status, ("Premature end of ruleSpec \"%s\"", ruleSpec));
            break;
        }
        ++eqPos;
        const char* begQuotePos = strchr(eqPos, '\'');
        const char* endQuotePos = NULL;
        if (begQuotePos && (++begQuotePos < finalPos)) {
            endQuotePos = strchr(begQuotePos, '\'');
        }
        if (!endQuotePos) {
            status = ER_FAIL;
            QCC_LogError(status, ("Quote mismatch in ruleSpec \"%s\"", ruleSpec));
            break;
        }
        if (0 == strncmp("type", pos, 4)) {
            if (0 == strncmp("signal", begQuotePos, endQuotePos - begQuotePos)) {
                type = MESSAGE_SIGNAL;
            } else if (0 == strncmp("method_call", begQuotePos, endQuotePos - begQuotePos)) {
                type = MESSAGE_METHOD_CALL;
            } else if (0 == strncmp("method_return", begQuotePos, endQuotePos - begQuotePos)) {
                type = MESSAGE_METHOD_RET;
            } else if (0 == strncmp("error", begQuotePos, endQuotePos - begQuotePos)) {
                type = MESSAGE_ERROR;
            } else {
                status = ER_FAIL;
                QCC_LogError(status, ("Invalid type value in ruleSpec \"%s\"", ruleSpec));
                break;
            }
        } else if (0 == strncmp("sender", pos, 6)) {
            sender = qcc::String(begQuotePos, endQuotePos - begQuotePos);
        } else if (0 == strncmp("interface", pos, 9)) {
            iface = qcc::String(begQuotePos, endQuotePos - begQuotePos);
        } else if (0 == strncmp("member", pos, 6)) {
            member = qcc::String(begQuotePos, endQuotePos - begQuotePos);
        } else if (0 == strncmp("path", pos, 4)) {
            path = qcc::String(begQuotePos, endQuotePos - begQuotePos);
        } else if (0 == strncmp("destination", pos, 11)) {
            destination = qcc::String(begQuotePos, endQuotePos - begQuotePos);
        } else if (0 == strncmp("arg", pos, 3)) {
            status = ER_NOT_IMPLEMENTED;
            QCC_LogError(status, ("arg keys are not supported in ruleSpec \"%s\"", ruleSpec));
            break;
        } else {
            status = ER_FAIL;
            QCC_LogError(status, ("Invalid key in ruleSpec \"%s\"", ruleSpec));
            break;
        }
        pos = endPos + 1;
    }
    if (outStatus) {
        *outStatus = status;
    }
}

bool Rule::IsMatch(const Message& msg)
{
    /* The fields of a rule (if specified) are logically anded together */
    if ((type != MESSAGE_INVALID) && (type != msg->GetType())) {
        return false;
    }
    if (!sender.empty() && (0 != strcmp(sender.c_str(), msg->GetSender()))) {
        return false;
    }
    if (!iface.empty() && (0 != strcmp(iface.c_str(), msg->GetInterface()))) {
        return false;
    }
    if (!member.empty() && (0 != strcmp(member.c_str(), msg->GetMemberName()))) {
        return false;
    }
    if (!path.empty() && (0 != strcmp(path.c_str(), msg->GetObjectPath()))) {
        return false;
    }
    if (!destination.empty() && (0 != strcmp(destination.c_str(), msg->GetDestination()))) {
        return false;
    }
    // @@ TODO Arg matches are not handled
    return true;
}

}
