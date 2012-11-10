#ifndef _ALLJOYN_REMOTEBUSOBJECT_H
#define _ALLJOYN_REMOTEBUSOBJECT_H
/**
 * @file
 * This file defines the class ProxyBusObject.
 * The ProxyBusObject represents a single object registered  registered on the bus.
 * ProxyBusObjects are used to make method calls on these remotely located DBus objects.
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
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/Session.h>

#include <Status.h>

namespace qcc {
/** @internal Forward references */
class Mutex;
}

namespace ajn {

/** @internal Forward references */
class BusAttachment;

/**
 * Each %ProxyBusObject instance represents a single DBus/AllJoyn object registered
 * somewhere on the bus. ProxyBusObjects are used to make method calls on these
 * remotely located DBus objects.
 */
class ProxyBusObject : public MessageReceiver {
    friend class XmlHelper;
    friend class AllJoynObj;

  public:

    /**
     * The default timeout for method calls
     */
    static const uint32_t DefaultCallTimeout = 25000;

    /**
     * Pure virtual base class implemented by classes that wish to receive
     * ProxyBusObject related messages.
     *
     * @internal Do not use this pattern for creating public Async versions of the APIs.  See
     * BusAttachment::JoinSessionAsync() instead.
     */
    class Listener {
      public:
        /**
         * Callback registered with IntrospectRemoteObjectAsync()
         *
         * @param status ER_OK if successful
         * @param obj       Remote bus object that was introspected
         * @param context   Context passed in IntrospectRemoteObjectAsync()
         */
        typedef void (ProxyBusObject::Listener::* IntrospectCB)(QStatus status, ProxyBusObject* obj, void* context);
    };

    /**
     * Create a default (unusable) %ProxyBusObject.
     * This constructor exist only to support assignment.
     */
    ProxyBusObject();

    /**
     * Create an empty proxy object that refers to an object at given remote service name. Note
     * that the created proxy object does not contain information about the interfaces that the
     * actual remote object implements with the exception that org.freedesktop.DBus.Peer
     * interface is special-cased (per the DBus spec) and can always be called on any object. Nor
     * does it contain information about the child objects that the actual remote object might
     * contain.
     *
     * To fill in this object with the interfaces and child object names that the actual remote
     * object describes in its introspection data, call IntrospectRemoteObject() or
     * IntrospectRemoteObjectAsync().
     *
     * @param bus        The bus.
     * @param service    The remote service name (well-known or unique).
     * @param path       The absolute (non-relative) object path for the remote object.
     * @param sessionId  The session id the be used for communicating with remote object.
     */
    ProxyBusObject(BusAttachment& bus, const char* service, const char* path, SessionId sessionId);

    /**
     *  %ProxyBusObject destructor.
     */
    virtual ~ProxyBusObject();

    /**
     * Return the absolute object path for the remote object.
     *
     * @return Object path
     */
    const qcc::String& GetPath(void) const { return path; }

    /**
     * Return the remote service name for this object.
     *
     * @return Service name (typically a well-known service name but may be a unique name)
     */
    const qcc::String& GetServiceName(void) const { return serviceName; }

    /**
     * Return the session Id for this object.
     *
     * @return Session Id
     */
    SessionId GetSessionId(void) const { return sessionId; }

    /**
     * Query the remote object on the bus to determine the interfaces and
     * children that exist. Use this information to populate this proxy's
     * interfaces and children.
     *
     * This call causes messages to be send on the bus, therefore it cannot
     * be called within AllJoyn callbacks (method/signal/reply handlers or
     * ObjectRegistered callbacks, etc.)
     *
     * @return
     *      - #ER_OK if successful
     *      - An error status otherwise
     */
    QStatus IntrospectRemoteObject();

    /**
     * Query the remote object on the bus to determine the interfaces and
     * children that exist. Use this information to populate this object's
     * interfaces and children.
     *
     * This call executes asynchronously. When the introspection response
     * is received from the actual remote object, this ProxyBusObject will
     * be updated and the callback will be called.
     *
     * This call exists primarily to allow introspection of remote objects
     * to be done inside AllJoyn method/signal/reply handlers and ObjectRegistered
     * callbacks.
     *
     * @param listener  Pointer to the object that will receive the callback.
     * @param callback  Method on listener that will be called.
     * @param context   User defined context which will be passed as-is to callback.
     * @return
     *      - #ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus IntrospectRemoteObjectAsync(ProxyBusObject::Listener* listener, ProxyBusObject::Listener::IntrospectCB callback, void* context);

    /**
     * Get a property from an interface on the remote object.
     *
     * @param iface       Name of interface to retrieve property from.
     * @param property    The name of the property to get.
     * @param[out] value  Property value.
     *
     * @return
     *      - #ER_OK if the property was obtained.
     *      - #ER_BUS_OBJECT_NO_SUCH_INTERFACE if the no such interface on this remote object.
     *      - #ER_BUS_NO_SUCH_PROPERTY if the property does not exist
     */
    QStatus GetProperty(const char* iface, const char* property, MsgArg& value) const;

    /**
     * Get all properties from an interface on the remote object.
     *
     * @param iface       Name of interface to retrieve all properties from.
     * @param[out] values Property values returned as an array of dictionary entries, signature "a{sv}".
     *
     * @return
     *      - #ER_OK if the property was obtained.
     *      - #ER_BUS_OBJECT_NO_SUCH_INTERFACE if the no such interface on this remote object.
     *      - #ER_BUS_NO_SUCH_PROPERTY if the property does not exist
     */
    QStatus GetAllProperties(const char* iface, MsgArg& values) const;

    /**
     * Set a property on an interface on the remote object.
     *
     * @param iface     Interface that holds the property
     * @param property  The name of the property to set
     * @param value     The value to set
     *
     * @return
     *      - #ER_OK if the property was set
     *      - #ER_BUS_OBJECT_NO_SUCH_INTERFACE if the no such interface on this remote object.
     *      - #ER_BUS_NO_SUCH_PROPERTY if the property does not exist
     */
    QStatus SetProperty(const char* iface, const char* property, MsgArg& value) const;

    /**
     * Set a uint32 property.
     *
     * @param iface     Interface that holds the property
     * @param property  The name of the property to set
     * @param u         The uint32 value to set
     *
     * @return
     *      - #ER_OK if the property was set
     *      - #ER_BUS_OBJECT_NO_SUCH_INTERFACE if the no such interface on this remote object.
     *      - #ER_BUS_NO_SUCH_PROPERTY if the property does not exist
     */
    QStatus SetProperty(const char* iface, const char* property, uint32_t u) const { MsgArg arg("u", u); return SetProperty(iface, property, arg); }

    /**
     * Set an int32 property.
     *
     * @param iface     Interface that holds the property
     * @param property  The name of the property to set
     * @param i         The int32 value to set
     *
     * @return
     *      - #ER_OK if the property was set
     *      - #ER_BUS_OBJECT_NO_SUCH_INTERFACE if the no such interface on this remote object.
     *      - #ER_BUS_NO_SUCH_PROPERTY if the property does not exist
     */
    QStatus SetProperty(const char* iface, const char* property, int32_t i) const { MsgArg arg("i", i); return SetProperty(iface, property, arg); }

    /**
     * Set a string property using a C string.
     *
     * @param iface     Interface that holds the property
     * @param property  The name of the property to set
     * @param s         The string value to set
     *
     * @return
     *      - #ER_OK if the property was set
     *      - #ER_BUS_NO_SUCH_PROPERTY if the property does not exist
     */
    QStatus SetProperty(const char* iface, const char* property, const char* s) const { MsgArg arg("s", s); return SetProperty(iface, property, arg); }

    /**
     * Set a string property using a qcc::String.
     *
     * @param iface     Interface that holds the property
     * @param property  The name of the property to set
     * @param s         The string value to set
     *
     * @return
     *      - #ER_OK if the property was set
     *      - #ER_BUS_OBJECT_NO_SUCH_INTERFACE if the no such interface on this remote object.
     *      - #ER_BUS_NO_SUCH_PROPERTY if the property does not exist
     */
    QStatus SetProperty(const char* iface, const char* property, const qcc::String& s) const { MsgArg arg("s", s.c_str()); return SetProperty(iface, property, arg); }

    /**
     * Returns the interfaces implemented by this object. Note that all proxy bus objects
     * automatically inherit the "org.freedesktop.DBus.Peer" which provides the built-in "ping"
     * method, so this method always returns at least that one interface.
     *
     * @param ifaces     A pointer to an InterfaceDescription array to receive the interfaces. Can be NULL in
     *                   which case no interfaces are returned and the return value gives the number
     *                   of interface available.
     * @param numIfaces  The size of the InterfaceDescription array. If this value is smaller than the total
     *                   number of interfaces only numIfaces will be returned.
     *
     * @return  The number of interfaces returned or the total number of interfaces if ifaces is NULL.
     */
    size_t GetInterfaces(const InterfaceDescription** ifaces = NULL, size_t numIfaces = 0) const;

    /**
     * Returns a pointer to an interface description. Returns NULL if the object does not implement
     * the requested interface.
     *
     * @param iface  The name of interface to get.
     *
     * @return
     *      - A pointer to the requested interface description.
     *      - NULL if requested interface is not implemented or not found
     */
    const InterfaceDescription* GetInterface(const char* iface) const;

    /**
     * Tests if this object implements the requested interface.
     *
     * @param iface  The interface to check
     *
     * @return  true if the object implements the requested interface
     */
    bool ImplementsInterface(const char* iface) const { return GetInterface(iface) != NULL; }

    /**
     * Add an interface to this ProxyBusObject.
     *
     * Occasionally, AllJoyn library user may wish to call a method on
     * a %ProxyBusObject that was not reported during introspection of the remote object.
     * When this happens, the InterfaceDescription will have to be registered with the
     * Bus manually and the interface will have to be added to the %ProxyBusObject using this method.
     * @remark
     * The interface added via this call must have been previously registered with the
     * Bus. (i.e. it must have come from a call to Bus::GetInterface()).
     *
     * @param iface    The interface to add to this object. Must come from Bus::GetInterface().
     * @return
     *      - #ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus AddInterface(const InterfaceDescription& iface);

    /**
     * Add an existing interface to this object using the interface's name.
     *
     * @param name   Name of existing interface to add to this object.
     * @return
     *      - #ER_OK if successful.
     *      - An error status otherwise.
     */
    QStatus AddInterface(const char* name);

    /**
     * Returns an array of ProxyBusObjects for the children of this %ProxyBusObject.
     *
     * @param children     A pointer to an %ProxyBusObject array to receive the children. Can be NULL in
     *                     which case no children are returned and the return value gives the number
     *                     of children available.
     * @param numChildren  The size of the %ProxyBusObject array. If this value is smaller than the total
     *                     number of children only numChildren will be returned.
     *
     * @return  The number of children returned or the total number of children if children is NULL.
     */
    size_t GetChildren(ProxyBusObject** children = NULL, size_t numChildren = 0);

    /**
     * Get a path descendant ProxyBusObject (child) by its relative path name.
     *
     * For example, if this ProxyBusObject's path is @c "/foo/bar", then you can
     * retrieve the ProxyBusObject for @c "/foo/bar/bat/baz" by calling
     * @c GetChild("bat/baz")
     *
     * @param path the relative path for the child.
     *
     * @return
     *      - The (potentially deep) descendant ProxyBusObject
     *      - NULL if not found.
     */
    ProxyBusObject* GetChild(const char* path);

    /**
     * Add a child object (direct or deep object path descendant) to this object.
     * If you add a deep path descendant, this method will create intermediate
     * ProxyBusObject children as needed.
     *
     * @remark
     *  - It is an error to try to add a child that already exists.
     *  - It is an error to try to add a child that has an object path that is not a descendant of this object's path.
     *
     * @param child  Child ProxyBusObject
     * @return
     *      - #ER_OK if successful.
     *      - #ER_BUS_BAD_CHILD_PATH if the path is a bad path
     *      - #ER_BUS_OBJ_ALREADY_EXISTS the the object already exists on the ProxyBusObject
     */
    QStatus AddChild(const ProxyBusObject& child);

    /**
     * Remove a child object and any descendants it may have.
     *
     * @param path   Absolute or relative (to this ProxyBusObject) object path.
     * @return
     *      - #ER_OK if successful.
     *      - #ER_BUS_BAD_CHILD_PATH if the path given was not a valid path
     *      - #ER_BUS_OBJ_NOT_FOUND if the Child object was not found
     *      - #ER_FAIL any other unexpected error.
     */
    QStatus RemoveChild(const char* path);

    /**
     * Make a synchronous method call from this object
     *
     * @param method       Method being invoked.
     * @param args         The arguments for the method call (can be NULL)
     * @param numArgs      The number of arguments
     * @param replyMsg     The reply message received for the method call
     * @param timeout      Timeout specified in milliseconds to wait for a reply
     * @param flags        Logical OR of the message flags for this method call. The following flags apply to method calls:
     *                     - If #ALLJOYN_FLAG_ENCRYPTED is set the message is authenticated and the payload if any is encrypted.
     *                     - If #ALLJOYN_FLAG_COMPRESSED is set the header is compressed for destinations that can handle header compression.
     *                     - If #ALLJOYN_FLAG_AUTO_START is set the bus will attempt to start a service if it is not running.
     *
     *
     * @return
     *      - #ER_OK if the method call succeeded and the reply message type is #MESSAGE_METHOD_RET
     *      - #ER_BUS_REPLY_IS_ERROR_MESSAGE if the reply message type is #MESSAGE_ERROR
     */
    QStatus MethodCall(const InterfaceDescription::Member& method,
                       const MsgArg* args,
                       size_t numArgs,
                       Message& replyMsg,
                       uint32_t timeout = DefaultCallTimeout,
                       uint8_t flags = 0) const;

    /**
     * Make a synchronous method call from this object
     *
     * @param ifaceName    Name of interface.
     * @param methodName   Name of method.
     * @param args         The arguments for the method call (can be NULL)
     * @param numArgs      The number of arguments
     * @param replyMsg     The reply message received for the method call
     * @param timeout      Timeout specified in milliseconds to wait for a reply
     * @param flags        Logical OR of the message flags for this method call. The following flags apply to method calls:
     *                     - If #ALLJOYN_FLAG_ENCRYPTED is set the message is authenticated and the payload if any is encrypted.
     *                     - If #ALLJOYN_FLAG_COMPRESSED is set the header is compressed for destinations that can handle header compression.
     *                     - If #ALLJOYN_FLAG_AUTO_START is set the bus will attempt to start a service if it is not running.
     *
     * @return
     *      - #ER_OK if the method call succeeded and the reply message type is #MESSAGE_METHOD_RET
     *      - #ER_BUS_REPLY_IS_ERROR_MESSAGE if the reply message type is #MESSAGE_ERROR
     */
    QStatus MethodCall(const char* ifaceName,
                       const char* methodName,
                       const MsgArg* args,
                       size_t numArgs,
                       Message& replyMsg,
                       uint32_t timeout = DefaultCallTimeout,
                       uint8_t flags = 0) const;

    /**
     * Make a fire-and-forget method call from this object. The caller will not be able to tell if
     * the method call was successful or not. This is equivalent to calling MethodCall() with
     * flags == ALLJOYN_FLAG_NO_REPLY_EXPECTED. Because this call doesn't block it can be made from
     * within a signal handler.
     *
     * @param ifaceName    Name of interface.
     * @param methodName   Name of method.
     * @param args         The arguments for the method call (can be NULL)
     * @param numArgs      The number of arguments
     * @param flags        Logical OR of the message flags for this method call. The following flags apply to method calls:
     *                     - If #ALLJOYN_FLAG_ENCRYPTED is set the message is authenticated and the payload if any is encrypted.
     *                     - If #ALLJOYN_FLAG_COMPRESSED is set the header is compressed for destinations that can handle header compression.
     *                     - If #ALLJOYN_FLAG_AUTO_START is set the bus will attempt to start a service if it is not running.
     *
     * @return
     *      - #ER_OK if the method call succeeded
     */
    QStatus MethodCall(const char* ifaceName,
                       const char* methodName,
                       const MsgArg* args,
                       size_t numArgs,
                       uint8_t flags = 0) const
    {
        return MethodCallAsync(ifaceName, methodName, NULL, NULL, args, numArgs, NULL, 0, flags |= ALLJOYN_FLAG_NO_REPLY_EXPECTED);
    }

    /**
     * Make a fire-and-forget method call from this object. The caller will not be able to tell if
     * the method call was successful or not. This is equivalent to calling MethodCall() with
     * flags == ALLJOYN_FLAG_NO_REPLY_EXPECTED. Because this call doesn't block it can be made from
     * within a signal handler.
     *
     *
     * @param method       Method being invoked.
     * @param args         The arguments for the method call (can be NULL)
     * @param numArgs      The number of arguments
     * @param flags        Logical OR of the message flags for this method call. The following flags apply to method calls:
     *                     - If #ALLJOYN_FLAG_ENCRYPTED is set the message is authenticated and the payload if any is encrypted.
     *                     - If #ALLJOYN_FLAG_COMPRESSED is set the header is compressed for destinations that can handle header compression.
     *                     - If #ALLJOYN_FLAG_AUTO_START is set the bus will attempt to start a service if it is not running.
     *
     * @return
     *      - #ER_OK if the method call succeeded and the reply message type is #MESSAGE_METHOD_RET
     */
    QStatus MethodCall(const InterfaceDescription::Member& method,
                       const MsgArg* args,
                       size_t numArgs,
                       uint8_t flags = 0) const
    {
        return MethodCallAsync(method, NULL, NULL, args, numArgs, NULL, 0, flags |= ALLJOYN_FLAG_NO_REPLY_EXPECTED);
    }

    /**
     * Make an asynchronous method call from this object
     *
     * @param method       Method being invoked.
     * @param receiver     The object to be called when the asych method call completes.
     * @param replyFunc    The function that is called to deliver the reply
     * @param args         The arguments for the method call (can be NULL)
     * @param numArgs      The number of arguments
     * @param receiver     The object to be called when the asych method call completes.
     * @param context      User-defined context that will be returned to the reply handler
     * @param timeout      Timeout specified in milliseconds to wait for a reply
     * @param flags        Logical OR of the message flags for this method call. The following flags apply to method calls:
     *                     - If #ALLJOYN_FLAG_ENCRYPTED is set the message is authenticated and the payload if any is encrypted.
     *                     - If #ALLJOYN_FLAG_COMPRESSED is set the header is compressed for destinations that can handle header compression.
     *                     - If #ALLJOYN_FLAG_AUTO_START is set the bus will attempt to start a service if it is not running.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus MethodCallAsync(const InterfaceDescription::Member& method,
                            MessageReceiver* receiver,
                            MessageReceiver::ReplyHandler replyFunc,
                            const MsgArg* args = NULL,
                            size_t numArgs = 0,
                            void* context = NULL,
                            uint32_t timeout = DefaultCallTimeout,
                            uint8_t flags = 0) const;

    /**
     * Make an asynchronous method call from this object
     *
     * @param ifaceName    Name of interface for method.
     * @param methodName   Name of method.
     * @param receiver     The object to be called when the asynchronous method call completes.
     * @param replyFunc    The function that is called to deliver the reply
     * @param args         The arguments for the method call (can be NULL)
     * @param numArgs      The number of arguments
     * @param context      User-defined context that will be returned to the reply handler
     * @param timeout      Timeout specified in milliseconds to wait for a reply
     * @param flags        Logical OR of the message flags for this method call. The following flags apply to method calls:
     *                     - If #ALLJOYN_FLAG_ENCRYPTED is set the message is authenticated and the payload if any is encrypted.
     *                     - If #ALLJOYN_FLAG_COMPRESSED is set the header is compressed for destinations that can handle header compression.
     *                     - If #ALLJOYN_FLAG_AUTO_START is set the bus will attempt to start a service if it is not running.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus MethodCallAsync(const char* ifaceName,
                            const char* methodName,
                            MessageReceiver* receiver,
                            MessageReceiver::ReplyHandler replyFunc,
                            const MsgArg* args = NULL,
                            size_t numArgs = 0,
                            void* context = NULL,
                            uint32_t timeout = DefaultCallTimeout,
                            uint8_t flags = 0) const;

    /**
     * Initialize this proxy object from an XML string. Calling this method does several things:
     *
     *  -# Create and register any new InterfaceDescription(s) that are mentioned in the XML.
     *     (Interfaces that are already registered with the bus are left "as-is".)
     *  -# Add all the interfaces mentioned in the introspection data to this ProxyBusObject.
     *  -# Recursively create any child ProxyBusObject(s) and create/add their associated @n
     *     interfaces as mentioned in the XML. Then add the descendant object(s) to the appropriate
     *     descendant of this ProxyBusObject (in the children collection). If the named child object
     *     already exists as a child of the appropriate ProxyBusObject, then it is updated
     *     to include any new interfaces or children mentioned in the XML.
     *
     * Note that when this method fails during parsing, the return code will be set accordingly.
     * However, any interfaces which were successfully parsed prior to the failure
     * may be registered with the bus. Similarly, any objects that were successfully created
     * before the failure will exist in this object's set of children.
     *
     * @param xml         An XML string in DBus introspection format.
     * @param identifier  An optional identifying string to include in error logging messages.
     *
     * @return
     *      - #ER_OK if parsing is completely successful.
     *      - An error status otherwise.
     */
    QStatus ParseXml(const char* xml, const char* identifier = NULL);

    /**
     * Explicitly secure the connection to the remote peer for this proxy object. Peer-to-peer
     * connections can only be secured if EnablePeerSecurity() was previously called on the bus
     * attachment for this proxy object. If the peer-to-peer connection is already secure this
     * function does nothing. Note that peer-to-peer connections are automatically secured when a
     * method call requiring encryption is sent.
     *
     * This call causes messages to be send on the bus, therefore it cannot be called within AllJoyn
     * callbacks (method/signal/reply handlers or ObjectRegistered callbacks, etc.)
     *
     * @param forceAuth  If true, forces an re-authentication even if the peer connection is already
     *                   authenticated.
     *
     * @return
     *          - #ER_OK if the connection was secured or an error status indicating that the
     *            connection could not be secured.
     *          - #ER_BUS_NO_AUTHENTICATION_MECHANISM if BusAttachment::EnablePeerSecurity() has not been called.
     *          - #ER_AUTH_FAIL if the attempt(s) to authenticate the peer failed.
     *          - Other error status codes indicating a failure.
     */
    QStatus SecureConnection(bool forceAuth = false);

    /**
     * Asynchronously secure the connection to the remote peer for this proxy object. Peer-to-peer
     * connections can only be secured if EnablePeerSecurity() was previously called on the bus
     * attachment for this proxy object. If the peer-to-peer connection is already secure this
     * function does nothing. Note that peer-to-peer connections are automatically secured when a
     * method call requiring encryption is sent.
     *
     * Notification of success or failure is via the AuthListener passed to EnablePeerSecurity().
     *
     * @param forceAuth  If true, forces an re-authentication even if the peer connection is already
     *                   authenticated.
     *
     * @return
     *          - #ER_OK if securing could begin.
     *          - #ER_BUS_NO_AUTHENTICATION_MECHANISM if BusAttachment::EnablePeerSecurity() has not been called.
     *          - Other error status codes indicating a failure.
     */
    QStatus SecureConnectionAsync(bool forceAuth = false);

    /**
     * Assignment operator.
     *
     * @param other  The object being assigned from
     * @return a copy of the ProxyBusObject
     */
    ProxyBusObject& operator=(const ProxyBusObject& other);

    /**
     * Copy constructor
     *
     * @param other  The object being copied from.
     */
    ProxyBusObject(const ProxyBusObject& other);

    /**
     * Indicates if this is a valid (usable) proxy bus object.
     *
     * @return true if a valid proxy bus object, false otherwise.
     */
    bool IsValid() const { return bus != NULL; }

  private:

    /**
     * @internal
     * Method return handler used to process synchronous method calls.
     *
     * @param msg     Method return message
     * @param context Opaque context passed from method_call to method_return
     */
    void SyncReplyHandler(Message& msg, void* context);

    /**
     * @internal
     * Introspection method_reply handler. (Internal use only)
     */
    void IntrospectMethodCB(Message& message, void* context);

    /**
     * @internal
     * Set the B2B endpoint to use for all communication with remote object.
     * This method is for internal use only.
     */
    void SetB2BEndpoint(RemoteEndpoint* b2bEp);

    /**
     * @internal
     * Helper used to destruct and clean-up  ProxyBusObject::components member.
     */
    void DestructComponents();

    /**
     * @internal
     * Internal introspection xml parse tree type.
     */
    struct IntrospectionXml;

    /**
     * @internal
     * Parse a single introspection <node> element.
     *
     * @param parseNode  XML element (must be a <node>).
     *
     * @return
     *       - #ER_OK if completely successful.
     *       - An error status otherwise
     */
    static QStatus ParseNode(const IntrospectionXml& node);

    /**
     * @internal
     * Parse a single introspection <interface> element.
     *
     * @param parseNode  XML element (must be an <interface>).
     *
     * @return
     *       - #ER_OK if completely successful.
     *       - An error status otherwise
     */
    static QStatus ParseInterface(const IntrospectionXml& ifc);

    /** Bus associated with object */
    BusAttachment* bus;

    struct Components;
    Components* components;  /**< The subcomponents of this object */

    /** Object path of this object */
    qcc::String path;

    qcc::String serviceName;    /**< Remote destination */
    SessionId sessionId;        /**< Session to use for communicating with remote object */
    bool hasProperties;         /**< True if proxy object implements properties */
    RemoteEndpoint* b2bEp;      /**< B2B endpoint to use or NULL to indicates normal sessionId based routing */
    mutable qcc::Mutex* lock;   /**< Lock that protects access to components member */
    bool isExiting;             /**< true iff ProxyBusObject is in the process of begin destroyed */
};

}

#endif
