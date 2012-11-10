/******************************************************************************
 *
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

#include <gtest/gtest.h>
#include <qcc/ManagedObj.h>

using namespace qcc;

struct Managed {
    Managed() : val(0) {
        //printf("Created Managed\n");
    }

    ~Managed() {
        //printf("Destroyed Managed\n");
    }

    void SetValue(int val) { this->val = val; }

    int GetValue(void) const { return val; }

  private:
    int val;
};

TEST(ManagedObjTest, ManagedObj) {
    ManagedObj<Managed> foo0;
    EXPECT_EQ(0, (*foo0).GetValue());

    ManagedObj<Managed> foo1;
    foo1->SetValue(1);
    EXPECT_EQ(0, foo0->GetValue());
    EXPECT_EQ(1, foo1->GetValue());

    foo0 = foo1;
    EXPECT_EQ(1, foo0->GetValue());
    EXPECT_EQ(1, foo1->GetValue());

    foo0->SetValue(0);
    EXPECT_EQ(0, foo0->GetValue());
    EXPECT_EQ(0, foo1->GetValue());

}