#ifndef _ALLJOYN_INTERFACEDESCRIPTION_H
#define _ALLJOYN_INTERFACEDESCRIPTION_H
/**
 * @file
 * This file defines types for statically describing a message bus interface
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

#include <alljoyn/Message.h>
#include <Status.h>

/// @cond ALLJOYN_DEV
/*!
   \def QCC_MODULE
   Internal usage
 */
#define QCC_MODULE "ALLJOYN"
/// @endcond

namespace ajn {

/** @name Access type */
// @{
static const uint8_t PROP_ACCESS_READ  = 1; /**< Read Access type */
static const uint8_t PROP_ACCESS_WRITE = 2; /**< Write Access type */
static const uint8_t PROP_ACCESS_RW    = 3; /**< Read-Write Access type */
// @}
/** @name Annotation flags */
// @{
static const uint8_t MEMBER_ANNOTATE_NO_REPLY   = 1; /**< No reply annotate flag */
static const uint8_t MEMBER_ANNOTATE_DEPRECATED = 2; /**< Deprecated annotate flag */
// @}

/**
 * @class InterfaceDescription
 * Class for describing message bus interfaces. %InterfaceDescription objects describe the methods,
 * signals and properties of a BusObject or ProxyBusObject.
 *
 * Calling ProxyBusObject::AddInterface(const char*) adds the AllJoyn interface described by an
 * %InterfaceDescription to a ProxyBusObject instance. After an  %InterfaceDescription has been
 * added, the methods described in the interface can be called. Similarly calling
 * BusObject::AddInterface adds the interface and its methods, properties, and signal to a
 * BusObject. After an interface has been added method handlers for the methods described in the
 * interface can be added by calling BusObject::AddMethodHandler or BusObject::AddMethodHandlers.
 *
 * An %InterfaceDescription can be constructed piecemeal by calling InterfaceDescription::AddMethod,
 * InterfaceDescription::AddMember(), and InterfaceDescription::AddProperty(). Alternatively,
 * calling ProxyBusObject::ParseXml will create the %InterfaceDescription instances for that proxy
 * object directly from an XML string. Calling ProxyBusObject::IntrospectRemoteObject or
 * ProxyBusObject::IntrospectRemoteObjectAsync also creates the %InterfaceDescription
 * instances from XML but in this case the XML is obtained by making a remote Introspect method
 * call on a bus object.
 */

class InterfaceDescription {

    friend class BusAttachment;
    friend class XmlHelper;

  public:

    /**
     * Structure representing the member to be added to the Interface
     */
    struct Member {
        const InterfaceDescription* iface;   /**< Interface that this member belongs to */
        AllJoynMessageType memberType;          /**< %Member type */
        qcc::String name;                    /**< %Member name */
        qcc::String signature;               /**< Method call IN arguments (NULL for signals) */
        qcc::String returnSignature;         /**< Signal or method call OUT arguments */
        qcc::String argNames;                /**< Comma separated list of argument names - can be NULL */
        uint8_t annotation;                  /**< Exclusive OR of flags MEMBER_ANNOTATE_NO_REPLY and MEMBER_ANNOTATE_DEPRECATED */
        qcc::String accessPerms;              /**< Required permissions to invoke this call */

        /** %Member constructor */
        Member(const InterfaceDescription* iface, AllJoynMessageType type, const char* name,
               const char* signature, const char* returnSignature, const char* argNames, uint8_t annotation, const char* accessPerms)
            : iface(iface),
            memberType(type),
            name(name),
            signature(signature ? signature : ""),
            returnSignature(returnSignature ? returnSignature : ""),
            argNames(argNames ? argNames : ""),
            annotation(annotation),
            accessPerms(accessPerms ? accessPerms : "") { }

        /**
         * Equality. Two members are defined to be equal if their members are equal except for iface which is ignored for equality.
         * @param o   Member to compare against this member.
         * @return    true iff o == this member.
         */
        bool operator==(const Member& o) const {
            return ((memberType == o.memberType) && (name == o.name) && (signature == o.signature)
                    && (returnSignature == o.returnSignature) && (annotation == o.annotation));
        }
    };

    /**
     * Structure representing properties of the Interface
     */
    struct Property {
        qcc::String name;              /**< %Property name */
        qcc::String signature;         /**< %Property type */
        uint8_t access;                /**< Access is #PROP_ACCESS_READ, #PROP_ACCESS_WRITE, or #PROP_ACCESS_RW */

        /** Property constructor */
        Property(const char* name, const char* signature, uint8_t access)
            : name(name), signature(signature ? signature : ""), access(access) { }

        /** Equality */
        bool operator==(const Property& o) const {
            return ((name == o.name) && (signature == o.signature) && (access == o.access));
        }
    };

    /**
     * Add a member to the interface.
     *
     * @param type        Message type.
     * @param name        Name of member.
     * @param inputSig    Signature of input parameters or NULL for none.
     * @param outSig      Signature of output parameters or NULL for none.
     * @param argNames    Comma separated list of input and then output arg names used in annotation XML.
     * @param annotation  Annotation flags.
     * @param accessPerms Required permissions to invoke this call
     *
     * @return
     *      - #ER_OK if successful
     *      - #ER_BUS_MEMBER_ALREADY_EXISTS if member already exists
     */
    QStatus AddMember(AllJoynMessageType type, const char* name, const char* inputSig, const char* outSig, const char* argNames, uint8_t annotation = 0, const char* accessPerms = 0);

    /**
     * Lookup a member description by name
     *
     * @param name  Name of the member to lookup
     * @return
     *      - Pointer to member.
     *      - NULL if does not exist.
     */
    const Member* GetMember(const char* name) const;

    /**
     * Get all the members.
     *
     * @param members     A pointer to a Member array to receive the members. Can be NULL in
     *                    which case no members are returned and the return value gives the number
     *                    of members available.
     * @param numMembers  The size of the Member array. If this value is smaller than the total
     *                    number of members only numMembers will be returned.
     *
     * @return  The number of members returned or the total number of members if members is NULL.
     */
    size_t GetMembers(const Member** members = NULL, size_t numMembers = 0) const;

    /**
     * Check for existence of a member. Optionally check the signature also.
     * @remark
     * if the a signature is not provided this method will only check to see if
     * a member with the given @c name exists.  If a signature is provided a
     * member with the given @c name and @c signature must exist for this to return true.
     *
     * @param name       Name of the member to lookup
     * @param inSig      Input parameter signature of the member to lookup
     * @param outSig     Output parameter signature of the member to lookup (leave NULL for signals)
     * @return true if the member name exists.
     */
    bool HasMember(const char* name, const char* inSig = NULL, const char* outSig = NULL);

    /**
     * Add a method call member to the interface.
     *
     * @param name        Name of method call member.
     * @param inputSig    Signature of input parameters or NULL for none.
     * @param outSig      Signature of output parameters or NULL for none.
     * @param argNames    Comma separated list of input and then output arg names used in annotation XML.
     * @param annotation  Annotation flags.
     * @param accessPerms Access permission requirements on this call
     *
     * @return
     *      - #ER_OK if successful
     *      - #ER_BUS_MEMBER_ALREADY_EXISTS if member already exists
     */
    QStatus AddMethod(const char* name, const char* inputSig, const char* outSig, const char* argNames, uint8_t annotation = 0, const char* accessPerms = 0)
    {
        return AddMember(MESSAGE_METHOD_CALL, name, inputSig, outSig, argNames, annotation, accessPerms);
    }

    /**
     * Lookup a member method description by name
     *
     * @param name  Name of the method to lookup
     * @return
     *      - Pointer to member.
     *      - NULL if does not exist.
     */
    const Member* GetMethod(const char* name) const
    {
        const Member* method = GetMember(name);
        return (method && method->memberType == MESSAGE_METHOD_CALL) ? method : NULL;
    }

    /**
     * Add a signal member to the interface.
     *
     * @param name        Name of method call member.
     * @param sig         Signature of parameters or NULL for none.
     * @param argNames    Comma separated list of arg names used in annotation XML.
     * @param annotation  Annotation flags.
     * @param accessPerms Access permission requirements on this call
     *
     * @return
     *      - #ER_OK if successful
     *      - #ER_BUS_MEMBER_ALREADY_EXISTS if member already exists
     */
    QStatus AddSignal(const char* name, const char* sig, const char* argNames, uint8_t annotation = 0, const char* accessPerms = 0)
    {
        return AddMember(MESSAGE_SIGNAL, name, sig, NULL, argNames, annotation, accessPerms);
    }

    /**
     * Lookup a member signal description by name
     *
     * @param name  Name of the signal to lookup
     * @return
     *      - Pointer to member.
     *      - NULL if does not exist.
     */
    const Member* GetSignal(const char* name) const
    {
        const Member* method = GetMember(name);
        return (method && method->memberType == MESSAGE_SIGNAL) ? method : NULL;
    }

    /**
     * Lookup a property description by name
     *
     * @param name  Name of the property to lookup
     * @return a structure representing the properties of the interface
     */
    const Property* GetProperty(const char* name) const;

    /**
     * Get all the properties.
     *
     * @param props     A pointer to a Property array to receive the properties. Can be NULL in
     *                  which case no properties are returned and the return value gives the number
     *                  of properties available.
     * @param numProps  The size of the Property array. If this value is smaller than the total
     *                  number of properties only numProperties will be returned.
     *
     *
     * @return  The number of properties returned or the total number of properties if props is NULL.
     */
    size_t GetProperties(const Property** props = NULL, size_t numProps = 0) const;

    /**
     * Add a property to the interface.
     *
     * @param name       Name of property.
     * @param signature  Property type.
     * @param access     #PROP_ACCESS_READ, #PROP_ACCESS_WRITE or #PROP_ACCESS_RW
     * @return
     *      - #ER_OK if successful.
     *      - #ER_BUS_PROPERTY_ALREADY_EXISTS if the property can not be added
     *                                        because it already exists.
     */
    QStatus AddProperty(const char* name, const char* signature, uint8_t access);

    /**
     * Check for existence of a property.
     *
     * @param name       Name of the property to lookup
     * @return true if the property exists.
     */
    bool HasProperty(const char* name) const { return GetProperty(name) != NULL; }

    /**
     * Check for existence of any properties
     *
     * @return  true if interface has any properties.
     */
    bool HasProperties() const { return GetProperties() != 0; }

    /**
     * Returns the name of the interface
     *
     * @return the interface name.
     */
    const char* GetName() const { return name.c_str(); }

    /**
     * Returns a description of the interface in introspection XML format
     * @return The interface description in introspection XML format.
     *
     * @param indent   Number of space chars to use in XML indentation.
     * @return The XML introspection data.
     */
    qcc::String Introspect(size_t indent = 0) const;

    /**
     * Activate this interface. An interface must be activated before it can be used. Activating an
     * interface locks the interface so that is can no longer be modified.
     */
    void Activate() { isActivated = true; }

    /**
     * Indicates if this interface is secure. Secure interfaces require end-to-end authentication.
     * The arguments for methods calls made to secure interfaces and signals emitted by secure
     * interfaces are encrypted.
     * @return true if the interface is secure.
     */
    bool IsSecure() const { return secure; }

    /**
     * Equality operation.
     */
    bool operator==(const InterfaceDescription& other) const;

    /**
     * Destructor
     */
    ~InterfaceDescription();

    /**
     * Copy constructor
     *
     * @param other  The InterfaceDescription being copied to this one.
     */
    InterfaceDescription(const InterfaceDescription& other);

  private:

    /**
     * Default constructor is private
     */
    InterfaceDescription() { }

    /**
     * Construct an interface with no methods or properties
     * This constructor cannot be used by any class other than the factory class (Bus).
     *
     * @param name   Fully qualified interface name.
     */
    InterfaceDescription(const char* name, bool secure);

    /**
     * Assignment operator
     *
     * @param other  The InterfaceDescription being copied to this one.
     */
    InterfaceDescription& operator=(const InterfaceDescription& other);

    struct Definitions;
    Definitions* defs;   /**< The definitions for this interface */

    qcc::String name;    /**< Name of interface */
    bool isActivated;    /**< Set to true when interface is activated */
    bool secure;         /**< Set to true if the interface is secure */
};

}

#undef QCC_MODULE
#endif
