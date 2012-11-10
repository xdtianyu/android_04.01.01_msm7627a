#ifndef _ALLJOYN_XMLHELPER_H
#define _ALLJOYN_XMLHELPER_H
/**
 * @file
 *
 * This file defines a class for traversing introspection XML.
 *
 */

/******************************************************************************
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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

#ifndef __cplusplus
#error Only include BusUtil.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/XmlElement.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>

#include <Status.h>

namespace ajn {

/**
 * XmlHelper is a utility class for traversing introspection XML.
 */
class XmlHelper {
  public:

    XmlHelper(BusAttachment* bus, const char* ident) : bus(bus), ident(ident) { }

    /**
     * Traverse the XML tree adding all interfaces to the bus. Nodes are ignored.
     *
     * @param root  The root can be an <interface> or <node> element.
     *
     * @return #ER_OK if the XML was well formed and the interfaces were added.
     *         #ER_BUS_BAD_XML if the XML was not as expected.
     *         #Other errors indicating the interfaces were not succesfully added.
     */
    QStatus AddInterfaceDefinitions(const qcc::XmlElement* root) {
        if (root) {
            if (root->GetName() == "interface") {
                return ParseInterface(root, NULL);
            } else if (root->GetName() == "node") {
                return ParseNode(root, NULL);
            }
        }
        return ER_BUS_BAD_XML;
    }

    /**
     * Traverse the XML tree recursively adding all nodes as children of a parent proxy object.
     *
     * @param parent  The parent proxy object to add the children too.
     * @param root    The root must be a <node> element.
     *
     * @return #ER_OK if the XML was well formed and the children were added.
     *         #ER_BUS_BAD_XML if the XML was not as expected.
     *         #Other errors indicating the children were not succesfully added.
     */
    QStatus AddProxyObjects(ProxyBusObject& parent, const qcc::XmlElement* root) {
        if (root && (root->GetName() == "node")) {
            return ParseNode(root, &parent);
        } else {
            return ER_BUS_BAD_XML;
        }
    }

  private:

    QStatus ParseNode(const qcc::XmlElement* elem, ProxyBusObject* obj);
    QStatus ParseInterface(const qcc::XmlElement* elem, ProxyBusObject* obj);

    BusAttachment* bus;
    const char* ident;

};
}

#endif
