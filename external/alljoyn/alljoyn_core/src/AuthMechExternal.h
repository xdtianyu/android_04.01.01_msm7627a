#ifndef _ALLJOYN_AUTHMECHEXTERNAL_H
#define _ALLJOYN_AUTHMECHEXTERNAL_H
/**
 * @file
 * This file defines the class for the DBUS EXTERNAL authentication method
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

#ifndef __cplusplus
#error Only include AuthMechExternal.h in C++ code.
#endif

#include <qcc/platform.h>

#include <qcc/Mutex.h>
#include <qcc/Util.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include "AuthMechanism.h"


namespace ajn {

/**
 * Derived class for the DBUS External authentication method
 *
 * %AuthMechExternal inherits from AuthMechanism
 */
class AuthMechExternal : public AuthMechanism {
  public:

    /**
     * Returns the static name for this authentication method
     *
     * @return string "EXTERNAL"
     *
     * @see AuthMechExternal::GetName
     */
    static const char* AuthName() { return "EXTERNAL"; }

    /**
     * Returns the name for this authentication method
     *
     * returns the same result as \b AuthMechExternal::AuthName;
     *
     * @return the static name for the anonymous authentication mechanism.
     *
     */
    const char* GetName() { return AuthName(); }

    /**
     * Function of type MKeyStoreAuthMechanismManager::AuthMechFactory
     *
     * @param keyStore   The key store avaiable to this authentication mechansim.
     * @param listener   The listener to register (listener is not used by AuthMechExternal)
     *
     * @return An object of class AuthMechExternal
     */
    static AuthMechanism* Factory(KeyStore& keyStore, ProtectedAuthListener& listener) { return new AuthMechExternal(keyStore, listener); }

    /**
     * Client sends the user id in the initial response
     *
     * @param result  Returns:
     *                - ALLJOYN_AUTH_OK        if the authentication is complete,
     *                - ALLJOYN_AUTH_CONTINUE  if the authentication is incomplete
     *
     * @return the user id
     *
     */
    qcc::String InitialResponse(AuthResult& result) { result = ALLJOYN_AUTH_CONTINUE; return qcc::U32ToString(qcc::GetUid()); }

    /**
     * Responses flow from clients to servers.
     *        EXTERNAL always responds with OK
     * @param challenge Challenge provided by the server; the external authentication
     *                  mechanism will ignore this parameter.
     * @param result    Return ALLJOYN_AUTH_OK
     *
     * @return an empty string.
     */
    qcc::String Response(const qcc::String& challenge, AuthResult& result) { result = ALLJOYN_AUTH_OK; return ""; }

    /**
     * Server's challenge to be sent to the client
     *
     * the external authentication mechanism always responds with an empty
     * string and AuthResult of ALLJOYN_AUTH_OK when InitialChallenge is called.
     *
     * @param result Returns ALLJOYN_AUTH_OK
     *
     * @return an empty string
     *
     * @see AuthMechanism::InitialChallenge
     */
    qcc::String InitialChallenge(AuthResult& result) { result = ALLJOYN_AUTH_OK; return ""; }

    /**
     * Server's challenge to be sent to the client.
     *        EXTERNAL doesn't send anything after the initial challenge
     * @param response Response used by server
     * @param result Returns ALLJOYN_AUTH_OK.
     *
     * @return an empty string
     *
     * @see AuthMechanism::Challenge
     */
    qcc::String Challenge(const qcc::String& response, AuthResult& result) { result = ALLJOYN_AUTH_OK; return ""; }

    ~AuthMechExternal() { }

  private:

    AuthMechExternal(KeyStore& keyStore, ProtectedAuthListener& listener) : AuthMechanism(keyStore, listener) { }

};

}

#endif
