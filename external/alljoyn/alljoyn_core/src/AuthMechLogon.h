#ifndef _ALLJOYN_AUTHMECHLOGON_H
#define _ALLJOYN_AUTHMECHLOGON_H
/**
 * @file
 *
 * This file defines the class for the AllJoyn SRP logon authentication mechanism
 *
 */

/******************************************************************************
 * Copyright 2010-2012, Qualcomm Innovation Center, Inc.
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
#error Only include AuthMechLogon.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Crypto.h>

#include "AuthMechanism.h"

namespace ajn {

/**
 * Derived class for the ALLJOYN_SRP_LOGON authentication mechanism
 */
class AuthMechLogon : public AuthMechanism {
  public:

    /**
     * Returns the static name for this authentication method
     * @return string "ALLJOYN_SRP_LOGON"
     * @see AuthMechLogon GetName
     */
    static const char* AuthName() { return "ALLJOYN_SRP_LOGON"; }

    /**
     * Returns the name for this authentication method
     * returns the same result as @b AuthMechLogon::AuthName;
     * @return the static name for the Pin authentication mechanism.
     */
    const char* GetName() { return AuthName(); }

    /**
     * Initialize this authentication mechanism.
     *
     * @param authRole  Indicates if the authentication method is initializing as a challenger or a responder.
     * @param authPeer  The bus name of the remote peer that is being authenticated.
     *
     * @return ER_OK if the authentication mechanism was succesfully initialized
     *         otherwise an error status.
     */
    QStatus Init(AuthRole authRole, const qcc::String& authPeer);

    /**
     * Function of type AuthMechanismManager::AuthMechFactory. The listener cannot be NULL for
     * this authentication mechanism.
     *
     * @param keyStore   The key store avaiable to this authentication mechansim.
     * @param listener   The listener to register
     *
     * @return  An object of class AuthMechLogon
     */
    static AuthMechanism* Factory(KeyStore& keyStore, ProtectedAuthListener& listener) { return new AuthMechLogon(keyStore, listener); }

    /**
     * Client initiates the conversation by sending a random nonce
     */
    qcc::String InitialResponse(AuthResult& result);

    /**
     * Client's response to a challenge from the server
     */
    qcc::String Response(const qcc::String& challenge, AuthResult& result);

    /**
     * Server's challenge to be sent to the client
     * @param response Response used by server
     * @param result Server Challenge sent to the client
     */
    qcc::String Challenge(const qcc::String& response, AuthResult& result);

    /**
     * Helper function that generates a logon entry string for this authentication
     * mechanism.
     *
     * @param keyStore      The keystore to add the logon entry.
     * @param userName      The user name to use for generating the logon entry.
     * @param password      The password to use for generating the logon entry.
     *
     * @return  ER_OK if the logon entry was generated and stored.
     */
    static QStatus AddLogonEntry(KeyStore& keyStore, const char* userName, const char* password);

    /**
     * Indicates that this authentication mechanism is interactive and requires application or user
     * input.
     *
     * @return  Always returns true for this authentication mechanism.
     */
    bool IsInteractive() { return true; }

    /**
     * Indicates if an authentication mechanism was mutual or one sided. Some authentication
     * mechanisms can be either mutual or one-side others are always one or the other.
     *
     * @return This authentication mechanism is always one-sided. The responder is authenticated to
     *         the challenger but the reverse is not true.
     */
    bool IsMutual() { return false; }

    ~AuthMechLogon() { }

  private:

    /**
     * Objects must be created using the factory function
     */
    AuthMechLogon(KeyStore& keyStore, ProtectedAuthListener& listener);

    /**
     * Computes the master secret.
     */
    void ComputeMS();

    /**
     * Compute the verifier string.
     */
    qcc::String ComputeVerifier(const char* label);

    uint8_t step;
    qcc::Crypto_SRP srp;
    qcc::String clientRandom;
    qcc::String serverRandom;

    /**
     * Temporary storage for credentials.
     */
    AuthListener::Credentials creds;

    /**
     * Hash of all the challenges and responses used for final verification.
     */
    qcc::Crypto_SHA1 msgHash;
};

}

#endif
