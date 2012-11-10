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
#include <gtest/gtest.h>

#include <deque>

#include <qcc/Timer.h>
#include <Status.h>

using namespace std;
using namespace qcc;

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
            printf("Failed Triggered Alarm: status=%s, a.alarmTime=%lu, a.context=%p, expectedTimeMs=%lu\n",
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
//TODO this tests multiple things break this test into parts.
TEST(TimerTest, timer) {
    Timer t1;
    Timespec ts;
    QStatus status = t1.Start();
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);

    MyAlarmListener alarmListener1(1);
    MyAlarmListener alarmListener10(10000);

    /* Simple relative alarm */
    void* context = (void*) 0x12345678;
    Alarm a1(1000, &alarmListener1, 0, context);
    status = t1.AddAlarm(a1);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    GetTimeNow(&ts);
    ASSERT_TRUE(testNextAlarm(ts + 1000, context));

    /* Recurring simple alarm */
    Alarm a2(1000, &alarmListener1, 1000);
    status = t1.AddAlarm(a2);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    GetTimeNow(&ts);
    ASSERT_TRUE(testNextAlarm(ts + 1000, 0));
    ASSERT_TRUE(testNextAlarm(ts + 2000, 0));
    ASSERT_TRUE(testNextAlarm(ts + 3000, 0));
    ASSERT_TRUE(testNextAlarm(ts + 4000, 0));
    t1.RemoveAlarm(a2);

    /* Stop and Start */
    status = t1.Stop();
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    status = t1.Join();
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    status = t1.Start();
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);

    /* Test concurrency */
    Timer t2("testTimer", true, 3);
    status = t2.Start();
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);

    Alarm a3(1, &alarmListener10);
    status = t2.AddAlarm(a3);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    Alarm a4(1, &alarmListener10);
    status = t2.AddAlarm(a4);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    Alarm a5(1, &alarmListener10);
    status = t2.AddAlarm(a5);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    Alarm a6(1, &alarmListener10);
    status = t2.AddAlarm(a6);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    Alarm a7(1, &alarmListener10);
    status = t2.AddAlarm(a7);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    Alarm a8(1, &alarmListener10);
    status = t2.AddAlarm(a8);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);

    GetTimeNow(&ts);
    ASSERT_TRUE(testNextAlarm(ts + 1, 0));
    ASSERT_TRUE(testNextAlarm(ts + 1, 0));
    ASSERT_TRUE(testNextAlarm(ts + 1, 0));
    ASSERT_TRUE(testNextAlarm(ts + 10001, 0));
    ASSERT_TRUE(testNextAlarm(ts + 10001, 0));
    ASSERT_TRUE(testNextAlarm(ts + 10001, 0));

    /* Test ReplaceTimer */
    Timer t3("testTimer");
    status = t3.Start();
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);

    Alarm ar1(2000, &alarmListener1);
    Alarm ar2(5000, &alarmListener1);
    GetTimeNow(&ts);
    status = t3.AddAlarm(ar1);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);
    status = t3.ReplaceAlarm(ar1, ar2);
    ASSERT_EQ(ER_OK, status) << "Status: " << QCC_StatusText(status);

    ASSERT_TRUE(testNextAlarm(ts + 5000, 0));
}