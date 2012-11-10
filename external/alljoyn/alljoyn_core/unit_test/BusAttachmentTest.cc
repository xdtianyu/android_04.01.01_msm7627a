/******************************************************************************
 * Copyright 2012, Qualcomm Innovation Center, Inc.
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
#include <signal.h>
#include <stdio.h>
#include <vector>

#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/MsgArg.h>
#include <alljoyn/version.h>

#include <Status.h>

/* Header files included for Google Test Framework */
#include <gtest/gtest.h>
#include "ajTestCommon.h"
//#include <qcc/time.h>

using namespace std;
using namespace qcc;
using namespace ajn;
class BusAttachmentTest : public testing::Test {
  public:
    BusAttachment bus;

    BusAttachmentTest() : bus("BusAttachmentTest", false) { };

    virtual void SetUp() {
        QStatus status = ER_OK;
        status = bus.Start();
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
        status = bus.Connect(getConnectArg().c_str());
        ASSERT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
    }

    virtual void TearDown() {
        bus.Stop();
    }

};

// disabled for mbus-1215
TEST_F(BusAttachmentTest, FindName_Same_Name)
{
    QStatus status = ER_OK;

    const char* requestedName = "org.alljoyn.bus.BusAttachmentTest.advertise";

    /* flag indicates that Fail if name cannot be immediatly obtained */
    status = bus.FindAdvertisedName(requestedName);
    EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);

    status = bus.FindAdvertisedName(requestedName);
    EXPECT_EQ(ER_ALLJOYN_FINDADVERTISEDNAME_REPLY_ALREADY_DISCOVERING, status) << "  Actual Status: " << QCC_StatusText(status);


    status = bus.CancelFindAdvertisedName(requestedName);
    EXPECT_EQ(ER_OK, status) << "  Actual Status: " << QCC_StatusText(status);
}

TEST_F(BusAttachmentTest, FindName_Null_Name)
{
    QStatus status = ER_OK;

    const char* requestedName = NULL;

    /* flag indicates that Fail if name cannot be immediatly obtained */
    status = bus.FindAdvertisedName(requestedName);
    EXPECT_EQ(ER_BAD_ARG_1, status) << "  Actual Status: " << QCC_StatusText(status);
}
