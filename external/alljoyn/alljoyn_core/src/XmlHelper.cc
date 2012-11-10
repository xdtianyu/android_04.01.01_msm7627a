/**
 * @file
 *
 * This file implements the XMlHelper class.
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

#include <assert.h>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/XmlElement.h>

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/Message.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>

#include "BusUtil.h"
#include "XmlHelper.h"
#include "SignatureUtils.h"

#include <Status.h>

#define QCC_MODULE "ALLJOYN"

using namespace qcc;
using namespace std;

namespace ajn {

QStatus XmlHelper::ParseInterface(const XmlElement* elem, ProxyBusObject* obj)
{
    QStatus status = ER_OK;

    assert(elem->GetName() == "interface");

    qcc::String ifName = elem->GetAttribute("name");
    if (!IsLegalInterfaceName(ifName.c_str())) {
        status = ER_BUS_BAD_INTERFACE_NAME;
        QCC_LogError(status, ("Invalid interface name \"%s\" in XML introspection data for %s", ifName.c_str(), ident));
        return status;
    }

    /* Get "secure" annotation */
    // TODO Think problem is here
    bool secure = false;
    vector<XmlElement*>::const_iterator ifIt = elem->GetChildren().begin();
    while (ifIt != elem->GetChildren().end()) {
        const XmlElement* ifChildElem = *ifIt++;
        qcc::String ifChildName = ifChildElem->GetName();
        if ((ifChildName == "annotation") && (ifChildElem->GetAttribute("name") == org::alljoyn::Bus::Secure)) {
            secure = (ifChildElem->GetAttribute("value") == "true");
            break;
        }
    }

    /* Create a new interface */
    InterfaceDescription intf(ifName.c_str(), secure);

    /* Iterate over <method>, <signal> and <property> elements */
    ifIt = elem->GetChildren().begin();
    while ((ER_OK == status) && (ifIt != elem->GetChildren().end())) {
        const XmlElement* ifChildElem = *ifIt++;
        const qcc::String& ifChildName = ifChildElem->GetName();
        const qcc::String& memberName = ifChildElem->GetAttribute("name");
        if ((ifChildName == "method") || (ifChildName == "signal")) {
            if (IsLegalMemberName(memberName.c_str())) {

                bool isMethod = (ifChildName == "method");
                bool isSignal = (ifChildName == "signal");
                bool isFirstArg = true;
                qcc::String inSig;
                qcc::String outSig;
                qcc::String argList;
                uint8_t annotations = 0;

                /* Iterate over member children */
                const vector<XmlElement*>& argChildren = ifChildElem->GetChildren();
                vector<XmlElement*>::const_iterator argIt = argChildren.begin();
                while ((ER_OK == status) && (argIt != argChildren.end())) {
                    const XmlElement* argElem = *argIt++;
                    if (argElem->GetName() == "arg") {
                        if (!isFirstArg) {
                            argList += ',';
                        }
                        isFirstArg = false;
                        const qcc::String& typeAtt = argElem->GetAttribute("type");

                        if (typeAtt.empty()) {
                            status = ER_BUS_BAD_XML;
                            QCC_LogError(status, ("Malformed <arg> tag (bad attributes)"));
                            break;
                        }

                        argList += argElem->GetAttribute("name");
                        if (isSignal || (argElem->GetAttribute("direction") == "in")) {
                            inSig += typeAtt;
                        } else {
                            outSig += typeAtt;
                        }
                    } else if (argElem->GetName() == "annotation") {
                        const qcc::String& nameAtt = argElem->GetAttribute("name");
                        const qcc::String& valueAtt = argElem->GetAttribute("value");

                        if (nameAtt == org::freedesktop::DBus::AnnotateDeprecated && valueAtt == "true") {
                            annotations |= MEMBER_ANNOTATE_DEPRECATED;
                        } else if (nameAtt == org::freedesktop::DBus::AnnotateNoReply && valueAtt == "true") {
                            annotations |= MEMBER_ANNOTATE_NO_REPLY;
                        }
                    }
                }

                /* Add the member */
                if ((ER_OK == status) && (isMethod || isSignal)) {
                    status = intf.AddMember(isMethod ? MESSAGE_METHOD_CALL : MESSAGE_SIGNAL,
                                            memberName.c_str(),
                                            inSig.c_str(),
                                            outSig.c_str(),
                                            argList.c_str(),
                                            annotations);
                }
            } else {
                status = ER_BUS_BAD_MEMBER_NAME;
                QCC_LogError(status, ("Illegal member name \"%s\" introspection data for %s", memberName.c_str(), ident));
            }
        } else if (ifChildName == "property") {
            const qcc::String& sig = ifChildElem->GetAttribute("type");
            const qcc::String& accessStr = ifChildElem->GetAttribute("access");
            if (!SignatureUtils::IsCompleteType(sig.c_str())) {
                status = ER_BUS_BAD_SIGNATURE;
                QCC_LogError(status, ("Invalid signature for property %s in introspection data from %s", memberName.c_str(), ident));
            } else if (memberName.empty()) {
                status = ER_BUS_BAD_BUS_NAME;
                QCC_LogError(status, ("Invalid name attribute for property in introspection data from %s", ident));
            } else {
                uint8_t access = 0;
                if (accessStr == "read") access = PROP_ACCESS_READ;
                if (accessStr == "write") access = PROP_ACCESS_WRITE;
                if (accessStr == "readwrite") access = PROP_ACCESS_RW;
                status = intf.AddProperty(memberName.c_str(), sig.c_str(), access);
            }
        } else if (ifChildName != "annotation") {
            status = ER_FAIL;
            QCC_LogError(status, ("Unknown element \"%s\" found in introspection data from %s", ifChildName.c_str(), ident));
            break;
        }
    }
    /* Add the interface with all its methods, signals and properties */
    if (ER_OK == status) {
        InterfaceDescription* newIntf = NULL;
        status = bus->CreateInterface(intf.GetName(), newIntf);
        if (ER_OK == status) {
            /* Assign new interface */
            *newIntf = intf;
            newIntf->Activate();
            if (obj) {
                obj->AddInterface(*newIntf);
            }
        } else if (ER_BUS_IFACE_ALREADY_EXISTS == status) {
            /* Make sure definition matches existing one */
            const InterfaceDescription* existingIntf = bus->GetInterface(intf.GetName());
            if (existingIntf) {
                if (*existingIntf == intf) {
                    if (obj) {
                        obj->AddInterface(*existingIntf);
                    }
                    status = ER_OK;
                } else {
                    status = ER_BUS_INTERFACE_MISMATCH;
                    QCC_LogError(status, ("XML interface does not match existing definition for \"%s\"", intf.GetName()));
                }
            } else {
                status = ER_FAIL;
                QCC_LogError(status, ("Failed to retrieve existing interface \"%s\"", intf.GetName()));
            }
        } else {
            QCC_LogError(status, ("Failed to create new inteface \"%s\"", intf.GetName()));
        }
    }
    return status;
}

QStatus XmlHelper::ParseNode(const XmlElement* root, ProxyBusObject* obj)
{
    QStatus status = ER_OK;

    assert(root->GetName() == "node");

    /* Iterate over <interface> and <node> elements */
    const vector<XmlElement*>& rootChildren = root->GetChildren();
    vector<XmlElement*>::const_iterator it = rootChildren.begin();
    while ((ER_OK == status) && (it != rootChildren.end())) {
        const XmlElement* elem = *it++;
        const qcc::String& elemName = elem->GetName();
        if (elemName == "interface") {
            status = ParseInterface(elem, obj);
        } else if (elemName == "node") {
            if (obj) {
                const qcc::String& relativePath = elem->GetAttribute("name");
                qcc::String childObjPath = obj->GetPath();
                if (0 || childObjPath.size() > 1) {
                    childObjPath += '/';
                }
                childObjPath += relativePath;
                if (!relativePath.empty() & IsLegalObjectPath(childObjPath.c_str())) {
                    /* Check for existing child with the same name. Use this child if found, otherwise create a new one */
                    ProxyBusObject* childObj = obj->GetChild(relativePath.c_str());
                    if (childObj) {
                        status = ParseNode(elem, childObj);
                    } else {
                        ProxyBusObject newChild(*bus, obj->GetServiceName().c_str(), childObjPath.c_str(), obj->sessionId);
                        status = ParseNode(elem, &newChild);
                        if (ER_OK == status) {
                            obj->AddChild(newChild);
                        }
                    }
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Failed to parse child object %s in introspection data for %s", childObjPath.c_str(), ident));
                    }
                } else {
                    status = ER_FAIL;
                    QCC_LogError(status, ("Illegal child object name \"%s\" specified in introspection for %s", relativePath.c_str(), ident));
                }
            } else {
                status = ParseNode(elem, NULL);
            }
        }
    }
    return status;
}

} // ajn::
