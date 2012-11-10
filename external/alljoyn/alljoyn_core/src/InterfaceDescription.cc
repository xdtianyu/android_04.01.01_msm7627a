/**
 * @file
 *
 * This file implements the InterfaceDescription class
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
#include <qcc/String.h>
#include <qcc/StringMapKey.h>
#include <map>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/DBusStd.h>
#include <Status.h>

#include "SignatureUtils.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

static qcc::String NextArg(const char*& signature, qcc::String& argNames, bool inOut, size_t indent)
{
    qcc::String in(indent, ' ');
    qcc::String arg = in + "<arg";
    qcc::String argType;
    const char* start = signature;
    SignatureUtils::ParseCompleteType(signature);
    size_t len = signature - start;
    argType.append(start, len);

    if (!argNames.empty()) {
        size_t pos = argNames.find_first_of(',');
        arg += " name=\"" + argNames.substr(0, pos) + "\"";
        if (pos == qcc::String::npos) {
            argNames.clear();
        } else {
            argNames.erase(0, pos + 1);
        }
    }
    arg += " type=\"" + argType + "\" direction=\"";
    arg += inOut ? "in\"/>\n" : "out\"/>\n";
    return arg;
}

struct InterfaceDescription::Definitions {
    std::map<qcc::StringMapKey, Member> members;       /**< Interface members */
    std::map<qcc::StringMapKey, Property> properties;  /**< Interface properties */

    Definitions() { }
    Definitions(std::map<qcc::StringMapKey, Member> m, std::map<qcc::StringMapKey, Property> p) : members(m), properties(p) { }
};

InterfaceDescription::InterfaceDescription(const char* name, bool secure) :
    defs(new Definitions),
    name(name),
    isActivated(false),
    secure(secure)
{
}

InterfaceDescription::~InterfaceDescription()
{
    delete defs;
}

InterfaceDescription::InterfaceDescription(const InterfaceDescription& other) :
    defs(new Definitions(other.defs->members, other.defs->properties)),
    name(other.name),
    isActivated(false),
    secure(other.secure)
{
    /* Update the iface pointer in each member */
    std::map<qcc::StringMapKey, Member>::iterator mit = defs->members.begin();
    while (mit != defs->members.end()) {
        mit++->second.iface = this;
    }
}

InterfaceDescription& InterfaceDescription::operator=(const InterfaceDescription& other)
{
    if (this != &other) {
        name = other.name;
        defs->members = other.defs->members;
        defs->properties = other.defs->properties;
        secure = other.secure;

        /* Update the iface pointer in each member */
        std::map<qcc::StringMapKey, Member>::iterator mit = defs->members.begin();
        while (mit != defs->members.end()) {
            mit++->second.iface = this;
        }
    }
    return *this;
}

qcc::String InterfaceDescription::Introspect(size_t indent) const
{
    qcc::String in(indent, ' ');
    const qcc::String close = "\">\n";
    qcc::String xml = in + "<interface name=\"";

    xml += name + close;
    /*
     * Iterate over interface defs->members
     */
    std::map<qcc::StringMapKey, Member>::const_iterator mit = defs->members.begin();
    while (mit != defs->members.end()) {
        const Member& member = mit->second;
        qcc::String argNames = member.argNames;
        qcc::String mtype = (member.memberType == MESSAGE_METHOD_CALL) ? "method" : "signal";
        xml += in + "  <" + mtype + " name=\"" + member.name + close;

        /* Iterate over IN arguments */
        for (const char* sig = member.signature.c_str(); *sig;) {
            // always treat signals as direction=out
            xml += NextArg(sig, argNames, member.memberType != MESSAGE_SIGNAL, indent + 4);
        }
        /* Iterate over OUT arguments */
        for (const char* sig = member.returnSignature.c_str(); *sig;) {
            xml += NextArg(sig, argNames, false, indent + 4);
        }
        /*
         * Add annotations
         */
        if (member.annotation  & MEMBER_ANNOTATE_NO_REPLY) {
            xml += in + "    <annotation name=\"" + org::freedesktop::DBus::AnnotateNoReply + "\" value=\"true\"/>\n";
        }
        if (member.annotation  & MEMBER_ANNOTATE_DEPRECATED) {
            xml += in + "    <annotation name=\"" + org::freedesktop::DBus::AnnotateDeprecated + "\" value=\"true\"/>\n";
        }
        xml += in + "  </" + mtype + ">\n";
        ++mit;
    }
    /*
     * Iterate over interface properties
     */
    map<qcc::StringMapKey, Property>::const_iterator pit = defs->properties.begin();
    while (pit != defs->properties.end()) {
        const Property& property = pit->second;
        xml += in + "  <property name=\"" + property.name + "\" type=\"" + property.signature + "\"";
        if (property.access == PROP_ACCESS_READ) {
            xml += " access=\"read\"/>\n";
        } else if (property.access == PROP_ACCESS_WRITE) {
            xml += " access=\"write\"/>\n";
        } else {
            xml += " access=\"readwrite\"/>\n";
        }
        ++pit;
    }
    if (IsSecure()) {
        xml += in + "  <annotation name=\"" + org::alljoyn::Bus::Secure + "\" value=\"true\"/>\n";
    }
    xml += in + "</interface>\n";
    return xml;
}

QStatus InterfaceDescription::AddMember(AllJoynMessageType type,
                                        const char* name,
                                        const char* inSig,
                                        const char* outSig,
                                        const char* argNames,
                                        uint8_t annotation,
                                        const char* accessPerms)
{
    if (isActivated) {
        return ER_BUS_INTERFACE_ACTIVATED;
    }

    StringMapKey key = qcc::String(name);
    Member member(this, type, name, inSig, outSig, argNames, annotation, accessPerms);
    pair<StringMapKey, Member> item(key, member);
    pair<map<StringMapKey, Member>::iterator, bool> ret = defs->members.insert(item);
    return ret.second ? ER_OK : ER_BUS_MEMBER_ALREADY_EXISTS;
}

QStatus InterfaceDescription::AddProperty(const char* name, const char* signature, uint8_t access)
{
    if (isActivated) {
        return ER_BUS_INTERFACE_ACTIVATED;
    }

    StringMapKey key = qcc::String(name);
    Property prop(name, signature, access);
    pair<StringMapKey, Property> item(key, prop);
    pair<map<StringMapKey, Property>::iterator, bool> ret = defs->properties.insert(item);
    return ret.second ? ER_OK : ER_BUS_PROPERTY_ALREADY_EXISTS;
}

bool InterfaceDescription::operator==(const InterfaceDescription& other) const
{
    if (name != other.name) {
        return false;
    }

    if ((defs->members.size() != other.defs->members.size()) || (defs->properties.size() != other.defs->properties.size())) {
        return false;
    }

    map<qcc::StringMapKey, Member>::const_iterator mit = defs->members.begin();
    while (mit != defs->members.end()) {
        map<qcc::StringMapKey, Member>::const_iterator oMit = other.defs->members.find(mit->first);
        if (oMit == other.defs->members.end()) {
            return false;
        }
        if (!(oMit->second == mit->second)) {
            return false;
        }
        ++mit;
    }

    map<qcc::StringMapKey, Property>::const_iterator pit = defs->properties.begin();
    while (pit != defs->properties.end()) {
        map<qcc::StringMapKey, Property>::const_iterator oPit = other.defs->properties.find(pit->first);
        if (oPit == other.defs->properties.end()) {
            return false;
        }
        if (!(oPit->second == pit->second)) {
            return false;
        }
        ++pit;
    }
    return true;
}

size_t InterfaceDescription::GetProperties(const Property** props, size_t numProps) const
{
    size_t count = defs->properties.size();
    if (props) {
        count = min(count, numProps);
        map<qcc::StringMapKey, Property>::const_iterator pit = defs->properties.begin();
        for (size_t i = 0; i < count; i++, pit++) {
            props[i] = &(pit->second);
        }
    }
    return count;
}

const InterfaceDescription::Property* InterfaceDescription::GetProperty(const char* name) const
{
    std::map<qcc::StringMapKey, Property>::const_iterator pit = defs->properties.find(qcc::StringMapKey(name));

    return (pit == defs->properties.end()) ? NULL : &(pit->second);
}

size_t InterfaceDescription::GetMembers(const Member** members, size_t numMembers) const
{
    size_t count = defs->members.size();
    if (members) {
        count = min(count, numMembers);
        map<qcc::StringMapKey, Member>::const_iterator mit = defs->members.begin();
        for (size_t i = 0; i < count; i++, mit++) {
            members[i] = &(mit->second);
        }
    }
    return count;
}

const InterfaceDescription::Member* InterfaceDescription::GetMember(const char* name) const
{
    std::map<qcc::StringMapKey, Member>::const_iterator mit = defs->members.find(qcc::StringMapKey(name));

    return (mit == defs->members.end()) ? NULL : &(mit->second);
}

bool InterfaceDescription::HasMember(const char* name, const char* inSig, const char* outSig)
{
    const Member* member = GetMember(name);
    if (member == NULL) {
        return false;
    } else if ((inSig == NULL) && (outSig == NULL)) {
        return true;
    } else {
        bool found = true;
        if (inSig) {
            found = found && (member->signature.compare(inSig) == 0);
        }
        if (outSig && (member->memberType == MESSAGE_METHOD_CALL)) {
            found = found && (member->returnSignature.compare(outSig) == 0);
        }
        return found;
    }
}


}
