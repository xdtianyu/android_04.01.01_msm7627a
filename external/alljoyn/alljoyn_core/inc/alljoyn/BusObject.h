#ifndef _ALLJOYN_LOCALBUSOBJECT_H
#define _ALLJOYN_LOCALBUSOBJECT_H
/**
 * @file
 *
 * This file defines the base class for message bus objects that
 * are implemented and registered locally.
 *
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
#include <qcc/String.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/Session.h>
#include <Status.h>

namespace ajn {

/// @cond ALLJOYN_DEV
/** @internal Forward references */
class BusAttachment;
class MethodTable;
/// @endcond

/**
 * Message Bus Object base class
 */
class BusObject : public MessageReceiver {

    friend class MethodTable;
    friend class LocalEndpoint;

  public:

    /**
     * Return the path for the object
     *
     * @return Object path
     */
    const char* GetPath() { return path.c_str(); }

    /**
     * Get the name of this object.
     * The name is the last component of the path.
     *
     * @return Last component of object path.
     */
    qcc::String GetName();

    /**
     * %BusObject constructor.
     *
     * @param bus            Bus that this object exists on.
     * @param path           Object path for object.
     * @param isPlaceholder  Place-holder objects are created by the bus itself and serve only
     *                       as parent objects (in the object path sense) to other objects.
     */
    BusObject(BusAttachment& bus, const char* path, bool isPlaceholder = false);

    /**
     * %BusObject destructor.
     */
    virtual ~BusObject();

  protected:

    /**
     * Type used to add multiple methods at one time.
     * @see AddMethodHandlers()
     */
    typedef struct {
        const InterfaceDescription::Member* member;  /**< Pointer to method's member */
        MessageReceiver::MethodHandler handler;      /**< Method implementation */
    } MethodEntry;

    /** Bus associated with object */
    BusAttachment& bus;

    /**
     * Reply to a method call.
     *
     * @param msg      The method call message
     * @param args     The reply arguments (can be NULL)
     * @param numArgs  The number of arguments
     * @return
     *      - #ER_OK if successful
     *      - An error status otherwise
     */
    QStatus MethodReply(const Message& msg, const MsgArg* args = NULL, size_t numArgs = 0);

    /**
     * Reply to a method call with an error message.
     *
     * @param msg              The method call message
     * @param error            The name of the error
     * @param errorMessage     An error message string
     * @return
     *      - #ER_OK if successful
     *      - An error status otherwise
     */
    QStatus MethodReply(const Message& msg, const char* error, const char* errorMessage = NULL);

    /**
     * Reply to a method call with an error message.
     *
     * @param msg        The method call message
     * @param status     The status code for the error
     * @return
     *      - #ER_OK if successful
     *      - An error status otherwise
     */
    QStatus MethodReply(const Message& msg, QStatus status);

    /**
     * Send a signal.
     *
     * @param destination      The unique or well-known bus name or the signal recipient (NULL for broadcast signals)
     * @param sessionId        A unique SessionId for this AllJoyn session instance
     * @param signal           Interface member of signal being emitted.
     * @param args             The arguments for the signal (can be NULL)
     * @param numArgs          The number of arguments
     * @param timeToLive       If non-zero this specifies in milliseconds the useful lifetime for this
     *                         signal. If delivery of the signal is delayed beyond the timeToLive due to
     *                         network congestion or other factors the signal may be discarded. There is
     *                         no guarantee that expired signals will not still be delivered.
     * @param flags            Logical OR of the message flags for this signals. The following flags apply to signals:
     *                         - If ::ALLJOYN_FLAG_GLOBAL_BROADCAST is set broadcast signal (null destination) will be forwarded across bus-to-bus connections.
     *                         - If ::ALLJOYN_FLAG_COMPRESSED is set the header is compressed for destinations that can handle header compression.
     *                         - If ::ALLJOYN_FLAG_ENCRYPTED is set the message is authenticated and the payload if any is encrypted.
     * @return
     *      - #ER_OK if successful
     *      - An error status otherwise
     */
    QStatus Signal(const char* destination,
                   SessionId sessionId,
                   const InterfaceDescription::Member& signal,
                   const MsgArg* args = NULL,
                   size_t numArgs = 0,
                   uint16_t timeToLive = 0,
                   uint8_t flags = 0);


    /**
     * Add an interface to this object. If the interface has properties this will also add the
     * standard property access interface. An interface must be added before its method handlers can be
     * added. Note that the Peer interface (org.freedesktop.DBus.peer) is implicit on all objects and
     * cannot be explicitly added, and the Properties interface (org.freedesktop,DBus.Properties) is
     * automatically added when needed and cannot be explicitly added.
     *
     * Once an object is registered, it should not add any additional interfaces. Doing so would
     * confuse remote objects that may have already introspected this object.
     *
     * @param iface  The interface to add
     *
     * @return
     *      - #ER_OK if the interface was successfully added.
     *      - #ER_BUS_IFACE_ALREADY_EXISTS if the interface already exists.
     *      - An error status otherwise
     */
    QStatus AddInterface(const InterfaceDescription& iface);

    /**
     * Add a method handler to this object. The interface for the method handler must have already
     * been added by calling AddInterface().
     *
     * @param member   Interface member implemented by handler.
     * @param handler  Method handler.
     * @param context  An optional context. This is mainly intended for implementing language
     *                 bindings and should normally be NULL.
     *
     * @return
     *      - #ER_OK if the method handler was added.
     *      - An error status otherwise
     */
    QStatus AddMethodHandler(const InterfaceDescription::Member* member, MessageReceiver::MethodHandler handler, void* context = NULL);

    /**
     * Convenience method used to add a set of method handers at once.
     *
     * @param entries      Array of MehtodEntry
     * @param numEntries   Number of entries in array.
     *
     * @return
     *      - #ER_OK if all the methods were added
     *      - #ER_BUS_NO_SUCH_INTERFACE is method can not be added because interface does not exist.
     */
    QStatus AddMethodHandlers(const MethodEntry* entries, size_t numEntries);

    /**
     * Handle a bus request to read a property from this object.
     * BusObjects that implement properties should override this method.
     * The default version simply returns ER_BUS_NO_SUCH_PROPERTY
     *
     * @param ifcName    Identifies the interface that the property is defined on
     * @param propName  Identifies the the property to get
     * @param[out] val        Returns the property value. The type of this value is the actual value
     *                   type.
     * @return #ER_BUS_NO_SUCH_PROPERTY (Should be changed by user implementation of BusObject)
     */
    virtual QStatus Get(const char* ifcName, const char* propName, MsgArg& val) { return ER_BUS_NO_SUCH_PROPERTY; }

    /**
     * Handle a bus attempt to write a property value to this object.
     * BusObjects that implement properties should override this method.
     * This default version just replies with ER_BUS_NO_SUCH_PROPERTY
     *
     * @param ifcName    Identifies the interface that the property is defined on
     * @param propName  Identifies the the property to set
     * @param val        The property value to set. The type of this value is the actual value
     *                   type.
     * @return #ER_BUS_NO_SUCH_PROPERTY (Should be changed by user implementation of BusObject)
     */
    virtual QStatus Set(const char* ifcName, const char* propName, MsgArg& val) { return ER_BUS_NO_SUCH_PROPERTY; }

    /**
     * Returns a description of the object in the D-Bus introspection XML format.
     * This method can be overridden by derived classes in order to customize the
     * introspection XML presented to remote nodes. Note that to DTD description and
     * the root element are not generated.
     *
     * @param deep     Include XML for all descendants rather than stopping at direct children.
     * @param indent   Number of characters to indent the XML
     * @return Description of the object in D-Bus introspection XML format
     */
    virtual qcc::String GenerateIntrospection(bool deep = false, size_t indent = 0) const;

    /**
     * Called by the message bus when the object has been successfully registered. The object can
     * perform any initialization such as adding match rules at this time.
     */
    virtual void ObjectRegistered(void) { }

    /**
     * Called by the message bus when the object has been successfully unregistered
     * @remark
     * This base class implementation @b must be called explicitly by any overriding derived class.
     */
    virtual void ObjectUnregistered(void) { isRegistered = false; }

    /**
     * Default handler for a bus attempt to read a property value.
     * @remark
     * A derived class can override this function to provide a custom handler for the GetProp method
     * call. If overridden the custom handler must compose an appropriate reply message to return the
     * requested property value.
     *
     * @param member   Identifies the org.freedesktop.DBus.Properties.Get method.
     * @param msg      The Properties.Get request.
     */
    virtual void GetProp(const InterfaceDescription::Member* member, Message& msg);

    /**
     * Default handler for a bus attempt to write a property value.
     * @remark
     * A derived class can override this function to provide a custom handler for the SetProp method
     * call. If overridden the custom handler must compose an appropriate reply message.
     *
     * @param member   Identifies the org.freedesktop.DBus.Properties.Set method.
     * @param msg      The Properties.Set request.
     */
    virtual void SetProp(const InterfaceDescription::Member* member, Message& msg);

    /**
     * Default handler for a bus attempt to read all properties on an interface.
     * @remark
     * A derived class can override this function to provide a custom handler for the GetAllProps
     * method call. If overridden the custom handler must compose an appropriate reply message
     * listing all properties on this object.
     *
     * @param member   Identifies the org.freedesktop.DBus.Properties.GetAll method.
     * @param msg      The Properties.GetAll request.
     */
    virtual void GetAllProps(const InterfaceDescription::Member* member, Message& msg);

    /**
     * Default handler for a bus attempt to read the object's introspection data.
     * @remark
     * A derived class can override this function to provide a custom handler for the GetProp method
     * call. If overridden the custom handler must compose an appropriate reply message.
     *
     * @param member   Identifies the @c org.freedesktop.DBus.Introspectable.Introspect method.
     * @param msg      The Introspectable.Introspect request.
     */
    virtual void Introspect(const InterfaceDescription::Member* member, Message& msg);

    /**
     * This method can be overridden to provide access to the context registered in the AddMethodHandler() call.
     *
     * @param member  The method being called.
     * @param handler The handler to call.
     * @param message The message containing the method call arguments.
     * @param context NULL or a private context passed in when the method handler was registered.
     */
    virtual void CallMethodHandler(MessageReceiver::MethodHandler handler, const InterfaceDescription::Member* member, Message& message, void* context) {
        (this->*handler)(member, message);
    }

  private:

    /**
     * Assignment operator is private.
     */
    BusObject& operator=(const BusObject& other) { return *this; }

    /**
     * Copy constructor is private.
     */
    BusObject(const BusObject& other) : bus(other.bus) { }

    /**
     * Add the registered methods for this object to a method table.
     *
     * @param methodTable   The method table to which this objects methods are added.
     */
    void InstallMethods(MethodTable& methodTable);

    /**
     * This utility method is called by the bus during object registration.
     * Do not call this object explicitly.
     *
     * @return
     *      - #ER_OK if all the methods were added
     *      - #ER_BUS_NO_SUCH_INTERFACE is method can not be added because interface does not exist.
     */
    QStatus DoRegistration();

    /**
     * Returns true if this object implements the given interface.
     *
     * @param  iface   The name of the interface to look for.
     *
     * @return true iff object implements interface.
     */
    bool ImplementsInterface(const char* iface);

    /**
     * Replace this object by another one. This may require unlinking the existing object from its
     * parent and children and linking in the new one.
     *
     * @param object  The object that is replacing this object.
     */
    void Replace(BusObject& object);

    /**
     * Add an object as a child of this object.
     *
     * @param child The object to add as a child of this object.
     */
    void AddChild(BusObject& child);

    /**
     * Remove a child from this object returning the removed child.
     *
     * @return  Returns the removed child or NULL if the object has no children.
     */
    BusObject* RemoveChild();

    /**
     * Remove a specific child from this object.
     *
     * @param obj  Pointer to the child to be removed.
     * @return
     *      - #ER_OK if successful.
     *      - #ER_BUS_NO_SUCH_OBJECT otherwise.
     */
    QStatus RemoveChild(BusObject& obj);

    /**
     * Indicate that this BusObject is being used by an alternate thread.
     * This BusObject should not be deleted till the remote thread has completed
     * using this object.
     * This will increment a counter for each thread that calls this method
     */
    void InUseIncrement();

    /**
     * Indicate that this BusObject is no longer being used by an alternate thread.
     * It is safe to delete the object when the inUse counter has reached zero.
     */
    void InUseDecrement();

    struct Components;
    Components* components; /**< Internal components of this object */

    /** Object path of this object */
    const qcc::String path;

    /** Parent object of this object (NULL if this is the root object) */
    BusObject* parent;

    /** true if object's ObjectRegistered callback has been called */
    bool isRegistered;

    /** true if object is a placeholder (i.e. only exists to be the parent of a more meaningful object instance) */
    bool isPlaceholder;


};

}

#endif
