/**
 * @file
 *
 * This file tests SRP against the RFC 5054 test vector
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

#include <qcc/Crypto.h>
#include <qcc/Debug.h>
#include <qcc/KeyBlob.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>
#include <qcc/Debug.h>
#include <qcc/BigNum.h>

#include <alljoyn/version.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/AuthListener.h>

#include <Status.h>

/* Private files included for unit testing */
#include <SASLEngine.h>

#include <gtest/gtest.h>

#define QCC_MODULE "CRYPTO"

using namespace qcc;
using namespace std;
using namespace ajn;

TEST(SRPTest, RFC_5246_test_vector) {
    Crypto_SRP srp;
    QStatus status = srp.TestVector();
    EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " Sign failed";
}


TEST(SRPTest, Basic_API) {
    /*
     * Basic API test.
     */
    QStatus status = ER_OK;
    String toClient;
    String toServer;
    String verifier;
    KeyBlob serverPMS;
    KeyBlob clientPMS;
    String user = "someuser";
    String pwd = "a-secret-password";

    for (int i = 0; i < 1; ++i) {
        {
            Crypto_SRP client;
            Crypto_SRP server;

            status = server.ServerInit(user, pwd, toClient);
            ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ServerInit failed";

            status = client.ClientInit(toClient, toServer);
            ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ClientInit failed";

            status = server.ServerFinish(toServer);
            ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ServerFinish failed";

            status = client.ClientFinish(user, pwd);
            ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ClientFinish failed";

            /*
             * Check premaster secrets match
             */
            server.GetPremasterSecret(serverPMS);
            client.GetPremasterSecret(clientPMS);
            ASSERT_EQ(clientPMS.GetSize(), serverPMS.GetSize())
            << "Premaster secrets have different sizes"
            << "Premaster secret = " << BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str();

            ASSERT_EQ(0, memcmp(serverPMS.GetData(), clientPMS.GetData(), serverPMS.GetSize()))
            << "Premaster secrets don't match\n"
            << "client = " << BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str() << "\n"
            << "server = " << BytesToHexString(clientPMS.GetData(), clientPMS.GetSize()).c_str() << "\n"
            << "Premaster secret = " << BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str();

            //printf("Premaster secret = %s\n", BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str());
            verifier = server.ServerGetVerifier();
        }
    }

    //###### Checking verifier ########
    /*
     * Test using the verifier from the previous test.
     */
    {
        Crypto_SRP client;
        Crypto_SRP server;

        status = server.ServerInit(verifier, toClient);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ServerInit failed";

        status = client.ClientInit(toClient, toServer);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ClientInit failed";

        status = server.ServerFinish(toServer);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ServerFinish failed";

        status = client.ClientFinish(user, pwd);
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status) << " SRP ClientFinish failed";

        /*
         * Check premaster secrets match
         */
        server.GetPremasterSecret(serverPMS);
        client.GetPremasterSecret(clientPMS);
        ASSERT_EQ(clientPMS.GetSize(), serverPMS.GetSize())
        << "Premaster secrets have different sizes"
        << "Premaster secret = " << BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str();

        ASSERT_EQ(0, memcmp(serverPMS.GetData(), clientPMS.GetData(), serverPMS.GetSize()))
        << "Premaster secrets don't match\n"
        << "client = " << BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str() << "\n"
        << "server = " << BytesToHexString(clientPMS.GetData(), clientPMS.GetSize()).c_str() << "\n"
        << "Premaster secret = " << BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str();

        qcc::String serverRand = RandHexString(64);
        qcc::String clientRand = RandHexString(64);
        uint8_t masterSecret[48];

        //printf("testing pseudo random function\n");

        status = Crypto_PseudorandomFunction(serverPMS, "foobar", serverRand + clientRand, masterSecret, sizeof(masterSecret));
        ASSERT_EQ(ER_OK, status)
        << "  Actual Status: " << QCC_StatusText(status) << " SRP ClientFinish failed\n"
        << "Master secret = " << BytesToHexString(masterSecret, sizeof(masterSecret)).c_str();
    }
}

class MyAuthListener : public AuthListener {
    bool RequestCredentials(const char* authMechanism, const char* authPeer, uint16_t authCount, const char* userId, uint16_t credMask, Credentials& creds) {
        creds.SetPassword("123456");
        return true;
    }
    void AuthenticationComplete(const char* authMechanism, const char* authPeer, bool success) {
        printf("Authentication %s %s\n", authMechanism, success ? "succesful" : "failed");
    }
};

TEST(SRPTest, authentication_mechanism) {
    QStatus status = ER_OK;

    BusAttachment bus("srp");
    MyAuthListener myListener;
    bus.EnablePeerSecurity("ALLJOYN_SRP_KEYX", &myListener);

    ProtectedAuthListener listener;
    listener.Set(&myListener);

    SASLEngine responder(bus, ajn::AuthMechanism::RESPONDER, "ALLJOYN_SRP_KEYX", "1:1", listener);
    SASLEngine challenger(bus, ajn::AuthMechanism::CHALLENGER, "ALLJOYN_SRP_KEYX", "1:1", listener);

    SASLEngine::AuthState rState = SASLEngine::ALLJOYN_AUTH_FAILED;
    SASLEngine::AuthState cState = SASLEngine::ALLJOYN_AUTH_FAILED;

    qcc::String rStr;
    qcc::String cStr;

    while (status == ER_OK) {
        status = responder.Advance(cStr, rStr, rState);
        ASSERT_EQ(ER_OK, status) << "  Responder returned: " << QCC_StatusText(status);

        status = challenger.Advance(rStr, cStr, cState);
        ASSERT_EQ(ER_OK, status) << "  Challenger returned: " << QCC_StatusText(status);

        if ((rState == SASLEngine::ALLJOYN_AUTH_SUCCESS) && (cState == SASLEngine::ALLJOYN_AUTH_SUCCESS)) {
            break;
        }
    }
}
