/**
 * @file
 * Common classes unit tests
 */

/******************************************************************************
 *
 *
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

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <string>
#include <signal.h>
#include <stdio.h>
#include <vector>
#include <deque>

#include <qcc/Debug.h>
#include <qcc/FileStream.h>
#include <qcc/ManagedObj.h>
#include <qcc/String.h>
#include <qcc/Timer.h>
#include <Status.h>

#define QCC_MODULE "COMMON"

using namespace std;
using namespace qcc;

#define TEST_ASSERT(cond)                       \
    do {                                            \
        if (!(cond)) {                              \
            QCC_LogError(ER_FAIL, ("%s", # cond));   \
            return ER_FAIL;                         \
        }                                           \
    } while (0)

struct Managed {
    Managed() : val(0) {
        printf("Created Managed\n");
    }

    ~Managed() {
        printf("Destroyed Managed\n");
    }

    void SetValue(int val) { this->val = val; }

    int GetValue(void) const { return val; }

  private:
    int val;
};

static QStatus testManagedObj(void)
{

    ManagedObj<Managed> foo0;
    TEST_ASSERT(0 == (*foo0).GetValue());

    ManagedObj<Managed> foo1;
    foo1->SetValue(1);
    TEST_ASSERT(0 == foo0->GetValue());
    TEST_ASSERT(1 == foo1->GetValue());

    foo0 = foo1;
    TEST_ASSERT(1 == foo0->GetValue());
    TEST_ASSERT(1 == foo1->GetValue());

    foo0->SetValue(0);
    TEST_ASSERT(0 == foo0->GetValue());
    TEST_ASSERT(0 == foo1->GetValue());

    return ER_OK;
}

static QStatus testString()
{
    const char* testStr = "abcdefgdijk";

    qcc::String s(testStr);
    TEST_ASSERT(0 == ::strcmp(testStr, s.c_str()));
    TEST_ASSERT(::strlen(testStr) == s.size());

    /* Test find_first_of */
    TEST_ASSERT(3 == s.find_first_of('d'));
    TEST_ASSERT(3 == s.find_first_of('d', 3));
    TEST_ASSERT(3 == s.find_first_of("owed", 3));
    TEST_ASSERT(qcc::String::npos == s.find_first_of('d', 8));

    /* Test find_last_of */
    TEST_ASSERT(7 == s.find_last_of('d'));
    TEST_ASSERT(3 == s.find_last_of('d', 7));
    TEST_ASSERT(qcc::String::npos == s.find_last_of('d', 2));

    /* Test find_*_not_of */
    qcc::String ss = "xyxyxyx" + s + "xy";
    TEST_ASSERT(ss.find_first_not_of("xy") == 7);
    TEST_ASSERT(ss.find_last_not_of("xy") == 17);

    /* Test empty */
    TEST_ASSERT(false == s.empty());
    s.clear();
    TEST_ASSERT(true == s.empty());
    TEST_ASSERT(0 == s.size());

    /* Test operator= */
    s = "123456";
    TEST_ASSERT(::strcmp(s.c_str(), "123456") == 0);

    /* test copy constructor and char& operator[] */
    String s2 = "abcdefg";
    String t2 = s2;
    TEST_ASSERT(t2.c_str() == s2.c_str());
    TEST_ASSERT(t2 == "abcdefg");
    t2[1] = 'B';
    TEST_ASSERT(0 == ::strcmp(s2.c_str(), "abcdefg"));
    TEST_ASSERT(0 == ::strcmp(t2.c_str(), "aBcdefg"));

    /* Test append */
    String pre = "abcd";
    String post = "efgh";
    pre.append(post);
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcdefgh"));
    TEST_ASSERT(pre.size() == ::strlen("abcdefgh"));
    TEST_ASSERT(0 == ::strcmp(post.c_str(), "efgh"));
    TEST_ASSERT(post.size() == ::strlen("efgh"));

    pre.append("ijklm", 4);
    TEST_ASSERT(pre.size() == ::strlen("abcdefghijkl"));
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcdefghijkl"));

    /* Test erase */
    pre.erase(4, 2);
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcdghijkl"));

    /* Test erasing past the end of the string. It should stop
       at the string size. */
    pre.erase(pre.size() - 1, 100);
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcdghijk"));

    /* Test erasing after the end of the string. It should be
       a no-op and should not trigger any crash. */
    pre.erase(pre.size(), 2);
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcdghijk"));

    pre.erase(pre.size() + 1, 100);
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcdghijk"));

    /* Test resize */
    pre.resize(4, 'x');
    TEST_ASSERT(pre.size() == 4);
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcd"));

    pre.resize(8, 'x');
    TEST_ASSERT(pre.size() == 8);
    TEST_ASSERT(0 == ::strcmp(pre.c_str(), "abcdxxxx"));

    /* Test reserve */
    pre.reserve(100);
    const char* preAppend = pre.c_str();
    pre.append("y", 92);
    TEST_ASSERT(preAppend == pre.c_str());

    /* Test insert */
    String s5 = "abcdijkl";
    s5.insert(4, "efgh");
    TEST_ASSERT(::strcmp(s5.c_str(), "abcdefghijkl") == 0);

    /* Test operator== and operator!= */
    String s6 = "abcdefghijkl";
    TEST_ASSERT(s5 == s6);
    TEST_ASSERT(!(s5 != s6));

    /* Test operator< */
    TEST_ASSERT(!(s5 < s6));
    TEST_ASSERT(!(s6 < s5));
    s6.append('m');
    TEST_ASSERT(s5 < s6);
    TEST_ASSERT(!(s6 < s5));

    /* Test String(size_t, char, size_t) */
    String s3(8, 's', 8);
    TEST_ASSERT(0 == ::strcmp(s3.c_str(), "ssssssss"));
    TEST_ASSERT(s3.size() == ::strlen("ssssssss"));

    /* Test const char& operator[] */
    const char* testChars = "abcdefgh";
    qcc::String s7 = testChars;
    const char* orig = s7.c_str();
    TEST_ASSERT(s7.size() == ::strlen(testChars));
    for (size_t i = 0; i < s7.size(); ++i) {
        char c = s7[i];
        TEST_ASSERT(c == testChars[i]);
    }
    TEST_ASSERT(orig == s7.c_str());

    /* Test iterators */
    String s4(strlen(testChars), 'x');
    String::iterator it = s4.begin();
    for (size_t i = 0; i < s4.size(); ++i) {
        *it++ = testChars[i];
    }
    String::const_iterator cit = s4.begin();
    TEST_ASSERT(strlen(testChars) == s4.size());
    size_t i = 0;
    while (cit != s4.end()) {
        TEST_ASSERT(*cit++ == testChars[i++]);
    }
    TEST_ASSERT(i == strlen(testChars));

    s = testStr;
    TEST_ASSERT(s[0] == 'a');
    TEST_ASSERT(s[11] == '\0');

    /* Test substr */
    s2 = s.substr(0, 4) + "1234";
    TEST_ASSERT(s2 == "abcd1234");
    TEST_ASSERT(s2.substr(4, 1) == "1");
    TEST_ASSERT(s2.substr(1000, 1) == "");
    TEST_ASSERT(s.compare(1, 2, s2, 1, 2) == 0);

    /* Test += */
    s = "";
    for (size_t i = 0; i < 1000; ++i) {
        s += "foo";
        TEST_ASSERT(s.size() == 3 * (i + 1));
    }

    /* Test resize */
    s.erase(3, s.size() - 6);
    TEST_ASSERT(s.size() == 6);
    TEST_ASSERT(s == "foofoo");
    s.resize(s.size() + 3, 'x');
    TEST_ASSERT(s == "foofooxxx");

    return ER_OK;
}

/* This test assumes that ./testFile, ./testDir, and //testDir don't exist prior to running */
static QStatus testFileSink()
{
    const char* pass[] = {
        "testFile",               /* Creation of file */
        "testFile",               /* Open existing file */
        "testDir/foo",            /* Creation of both directory and file */
        "testDir/bar",            /* Creation of file in existing directory */
        "testDir/../testDir/foo", /* Normalize paths and open existing file */
        "testDir//bar",           /* Normalize path for extra slashes */
//#ifdef _WIN32                     /* Do not have permissions to create a file at root for other OSs without super user access */
//        "//testDir/foo",          /* Leading slashes */
//#endif
        "testDir/dir/foo",        /* Create multiple directories */
        "testDir/dir/bar",        /* Creation of file in existing nested directory */
        NULL
    };
    for (const char** pathname = pass; *pathname; ++pathname) {
        FileSink f = FileSink(*pathname, FileSink::PRIVATE);
        TEST_ASSERT(f.IsValid());
    }

    const char* xfail[] = {
        "testDir/dir",  /* Create a file that is already an existing directory */
#if !defined(_WIN32)
        "//testDir/foo", /* Do not have permissions to create a file at root for other OSs without super user access */
#endif
        NULL
    };
    for (const char** pathname = xfail; *pathname; ++pathname) {
        FileSink f = FileSink(*pathname, FileSink::PRIVATE);
        TEST_ASSERT(!f.IsValid());
    }

    return ER_OK;
}

static std::deque<std::pair<QStatus, Alarm> > triggeredAlarms;
static Mutex triggeredAlarmsLock;

static bool testNextAlarm(const Timespec& expectedTime, void* context)
{
    static const int jitter = 100;

    bool ret = false;
    triggeredAlarmsLock.Lock();
    uint32_t startTime = GetTimestamp();
    while (triggeredAlarms.empty() && (GetTimestamp() < (startTime + 20000))) {
        triggeredAlarmsLock.Unlock();
        qcc::Sleep(5);
        triggeredAlarmsLock.Lock();
    }
    if (!triggeredAlarms.empty()) {
        pair<QStatus, Alarm> p = triggeredAlarms.front();
        triggeredAlarms.pop_front();
        Timespec ts;
        GetTimeNow(&ts);
        uint64_t alarmTime = ts.GetAbsoluteMillis();
        uint64_t expectedTimeMs = expectedTime.GetAbsoluteMillis();
        ret = (p.first == ER_OK) && (context == p.second.GetContext()) && (alarmTime >= expectedTimeMs) && (alarmTime < (expectedTimeMs + jitter));
        if (!ret) {
            printf("Failed Triggered Alarm: status=%s, a.alarmTime=%llu, a.context=%p, expectedTimeMs=%llu\n",
                   QCC_StatusText(p.first), alarmTime, p.second.GetContext(), expectedTimeMs);
        }
    }
    triggeredAlarmsLock.Unlock();
    return ret;
}

class MyAlarmListener : public AlarmListener {
  public:
    MyAlarmListener(uint32_t delay) : AlarmListener(), delay(delay)
    {
    }
    void AlarmTriggered(const Alarm& alarm, QStatus reason)
    {
        triggeredAlarmsLock.Lock();
        triggeredAlarms.push_back(pair<QStatus, Alarm>(reason, alarm));
        triggeredAlarmsLock.Unlock();
        qcc::Sleep(delay);
    }
  private:
    const uint32_t delay;
};

static QStatus testTimer()
{
    Timer t1;
    Timespec ts;
    QStatus status = t1.Start();
    TEST_ASSERT(status == ER_OK);

    MyAlarmListener alarmListener1(1);
    MyAlarmListener alarmListener10(10000);

    /* Simple relative alarm */
    void* context = (void*) 0x12345678;
    Alarm a1(1000, &alarmListener1, 0, context);
    status = t1.AddAlarm(a1);
    TEST_ASSERT(status == ER_OK);
    GetTimeNow(&ts);
    TEST_ASSERT(testNextAlarm(ts + 1000, context));

    /* Recurring simple alarm */
    Alarm a2(1000, &alarmListener1, 1000);
    status = t1.AddAlarm(a2);
    TEST_ASSERT(status == ER_OK);
    GetTimeNow(&ts);
    TEST_ASSERT(testNextAlarm(ts + 1000, 0));
    TEST_ASSERT(testNextAlarm(ts + 2000, 0));
    TEST_ASSERT(testNextAlarm(ts + 3000, 0));
    TEST_ASSERT(testNextAlarm(ts + 4000, 0));
    t1.RemoveAlarm(a2);

    /* Stop and Start */
    status = t1.Stop();
    TEST_ASSERT(status == ER_OK);
    status = t1.Join();
    TEST_ASSERT(status == ER_OK);
    //status = t1.Start();
    //TEST_ASSERT(status == ER_OK);

    /* Test concurrency */
    Timer t2("testTimer", true, 3);
    status = t2.Start();
    TEST_ASSERT(status == ER_OK);

    Alarm a3(1, &alarmListener10);
    status = t2.AddAlarm(a3);
    TEST_ASSERT(status == ER_OK);
    Alarm a4(1, &alarmListener10);
    status = t2.AddAlarm(a4);
    TEST_ASSERT(status == ER_OK);
    Alarm a5(1, &alarmListener10);
    status = t2.AddAlarm(a5);
    TEST_ASSERT(status == ER_OK);
    Alarm a6(1, &alarmListener10);
    status = t2.AddAlarm(a6);
    TEST_ASSERT(status == ER_OK);
    Alarm a7(1, &alarmListener10);
    status = t2.AddAlarm(a7);
    TEST_ASSERT(status == ER_OK);
    Alarm a8(1, &alarmListener10);
    status = t2.AddAlarm(a8);
    TEST_ASSERT(status == ER_OK);

    GetTimeNow(&ts);
    TEST_ASSERT(testNextAlarm(ts + 1, 0));
    TEST_ASSERT(testNextAlarm(ts + 1, 0));
    TEST_ASSERT(testNextAlarm(ts + 1, 0));
    TEST_ASSERT(testNextAlarm(ts + 10001, 0));
    TEST_ASSERT(testNextAlarm(ts + 10001, 0));
    TEST_ASSERT(testNextAlarm(ts + 10001, 0));

    /* Test ReplaceTimer */
    Timer t3("testTimer");
    status = t3.Start();
    TEST_ASSERT(status == ER_OK);

    Alarm ar1(2000, &alarmListener1);
    Alarm ar2(5000, &alarmListener1);
    GetTimeNow(&ts);
    status = t3.AddAlarm(ar1);
    TEST_ASSERT(status == ER_OK);
    status = t3.ReplaceAlarm(ar1, ar2);
    TEST_ASSERT(status == ER_OK);

    TEST_ASSERT(testNextAlarm(ts + 5000, 0));

    return status;
}

/* Test structure describes an individual test */
struct Test {

    Test(string name, QStatus(*test)(void), string desc) : name(name), test(test), desc(desc) { }

    string name;
    QStatus (* test)(void);
    string desc;
};

/* Test List */
static Test tests[] = {
    Test("ManagedObj", testManagedObj, "Test ManagedObj implementation"),
    Test("String",     testString,     "Test String implementation"),
    Test("FileSink",   testFileSink,   "Test FileSink implementation"),
    Test("Timer",      testTimer,      "Test Timer implementation")
};


int main(int argc, char** argv)
{
    // signal(SIGINT, SigIntHandler);

#ifdef _WIN32
    WSADATA wsaData;
    WORD version = MAKEWORD(2, 0);
    int error = WSAStartup(version, &wsaData);
#endif

    int testsFailed = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(Test); ++i) {
        printf("----- STARTING TEST %s ------\n", tests[i].name.c_str());
        printf("----- DESCRIPTION: %s -----\n", tests[i].desc.c_str());
        QStatus result = tests[i].test();
        if (ER_OK == result) {
            printf("----- TEST %s RETURNED SUCCESSFULLY -----\n", tests[i].name.c_str());
        } else {
            printf("----- ERROR: TEST %s returned 0x%x (%s) -----\n", tests[i].name.c_str(), result, QCC_StatusText(result));
            testsFailed++;
        }
    }
    QCC_DbgPrintf(("There were %d test failures\n", testsFailed));

    if (testsFailed) {
        printf("\n FAILED \n");
    } else {
        printf("\n PASSED \n");
    }

    return testsFailed;
}
