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

#include <alljoyn/version.h>
#include <gtest/gtest.h>
#include <qcc/String.h>
#include <ctype.h>

TEST(VersionInfoTest, VersionInfo) {
    /*
     * version is expected to be string 'v#.#.#' where # represents a
     * number of unknown length This test code is most likly more complex than
     * the code use to generate the string but it should handle any value
     * returned
     */
    size_t pos1 = 1;
    size_t pos2 = 0;
    qcc::String ver = ajn::GetVersion();
    EXPECT_EQ('v', ver[0]);
    pos2 = ver.find_first_of(".");
    /*
     * unusual use of the unary operator '+' makes gcc compiler see qcc::String::npos as a rvalue
     * this prevents an 'undefined reference' compiler error when building with gcc.
     */
    EXPECT_NE(+qcc::String::npos, pos2);
    qcc::String architectureLevel = ver.substr(pos1, pos2 - pos1);
    for (unsigned int i = 0; i < architectureLevel.length(); i++) {
        EXPECT_TRUE(isdigit(architectureLevel[i]))
        << "architectureLevel version expected to be a number : "
        << architectureLevel.c_str();
    }
    pos1 = pos2 + 1;
    pos2 = ver.find_first_of(".", pos1);
    EXPECT_NE(+qcc::String::npos, pos2);

    qcc::String apiLevel = ver.substr(pos1, pos2 - pos1);
    for (unsigned int i = 0; i < apiLevel.length(); i++) {
        EXPECT_TRUE(isdigit(apiLevel[i]))
        << "apiLevel version expected to be a number : "
        << apiLevel.c_str();
    }
    qcc::String release = ver.substr(pos2 + 1);
    for (unsigned int i = 0; i < release.length(); i++) {
        EXPECT_TRUE(isdigit(release[i]))
        << "Release version expected to be a number : "
        << release.c_str();
    }
}
