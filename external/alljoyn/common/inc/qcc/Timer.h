/**
 * @file
 *
 * Timer thread
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
#ifndef _QCC_TIMER_H
#define _QCC_TIMER_H

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/atomic.h>
#include <set>

#include <qcc/Mutex.h>
#include <qcc/Thread.h>
#include <qcc/time.h>

namespace qcc {

/** @internal Forward declaration */
class Timer;
class Alarm;
class TimerThread;

/**
 * An alarm listener is capable of receiving alarm callbacks
 */
class AlarmListener {
    friend class Timer;
    friend class TimerThread;

  public:
    /**
     * Virtual destructor for derivable class.
     */
    virtual ~AlarmListener() { }

  private:

    /**
     * @param alarm  The alarm that was triggered.
     * @param status The reason the alarm was triggered. This will be either:
     *               #ER_OK               The normal case.
     *               #ER_TIMER_EXITING    The timer thread is exiting.
     */
    virtual void AlarmTriggered(const Alarm& alarm, QStatus reason) = 0;
};

class Alarm {
    friend class Timer;
    friend class TimerThread;

  public:

    /** Disable timeout value */
    static const uint32_t WAIT_FOREVER = static_cast<uint32_t>(-1);

    /**
     * Create a default (unusable) alarm.
     */
    Alarm() : listener(NULL), periodMs(0), context(NULL), id(IncrementAndFetch(&nextId)) { }

    /**
     * Create an alarm that can be added to a Timer.
     *
     * @param absoluteTime    Alarm time.
     * @param listener        Object to call when alarm is triggered.
     * @param periodMs        Periodicity of alarm in ms or 0 for no repeat.
     * @param context         Opaque context passed to listener callback.
     */
    Alarm(Timespec absoluteTime, AlarmListener* listener, uint32_t periodMs = 0, void* context = NULL)
        : alarmTime(absoluteTime), listener(listener), periodMs(periodMs), context(context), id(IncrementAndFetch(&nextId)) { }

    /**
     * Create an alarm that can be added to a Timer.
     *
     * @param relativeTimed   Number of ms from now that alarm will trigger.
     * @param listener        Object to call when alarm is triggered.
     * @param periodMs        Periodicity of alarm in ms or 0 for no repeat.
     * @param context         Opaque context passed to listener callback.
     */
    Alarm(uint32_t relativeTime, AlarmListener* listener, uint32_t periodMs = 0, void* context = NULL)
        : alarmTime(), listener(listener), periodMs(periodMs), context(context), id(IncrementAndFetch(&nextId))
    {
        if (relativeTime == WAIT_FOREVER) {
            alarmTime = END_OF_TIME;
        } else {
            GetTimeNow(&alarmTime);
            alarmTime += relativeTime;
        }
    }

    /**
     * Get context associated with alarm.
     *
     * @return User defined context.
     */
    void* GetContext(void) const { return context; }

    /**
     * Get the absolute alarmTime in milliseconds
     */
    uint64_t GetAlarmTime() const { return alarmTime.GetAbsoluteMillis(); }

    /**
     * Return true if this Alarm's time is less than the passed in alarm's time
     */
    bool operator<(const Alarm& other) const
    {
        return (alarmTime < other.alarmTime) || ((alarmTime == other.alarmTime) && (id < other.id));
    }

    /**
     * Return true if two alarm instances are equal.
     */
    bool operator==(const Alarm& other) const
    {
        return (alarmTime == other.alarmTime) && (id == other.id);
    }

  private:

    static int32_t nextId;
    Timespec alarmTime;
    AlarmListener* listener;
    uint32_t periodMs;
    mutable void* context;
    int32_t id;
};


class Timer : public ThreadListener {
    friend class TimerThread;

  public:

    /**
     * Constructor
     *
     * @param name               Name for the thread.
     * @param expireOnExit       If true call all pending alarms when this thread exits.
     * @param concurency         Dispatch up to this number of alarms concurently (using multiple threads).
     * @param prevenReentrancy   Prevent re-entrant call of AlarmTriggered.
     */
    Timer(const char* name = "timer", bool expireOnExit = false, uint32_t concurency = 1, bool preventReentrancy = false);

    /**
     * Destructor.
     */
    ~Timer();

    /**
     * Start the timer.
     *
     * @return  ER_OK if successful.
     */
    QStatus Start();

    /**
     * Stop the timer (and its associated threads).
     *
     * @return ER_OK if successful.
     */
    QStatus Stop();

    /**
     * Join the timer.
     * Block the caller until all the timer's threads are stopped.
     *
     * @return ER_OK if successful.
     */
    QStatus Join();

    /**
     * Return true if Timer is running.
     *
     * @return true iff timer is running.
     */
    bool IsRunning() const { return isRunning; }

    /**
     * Associate an alarm with a timer.
     *
     * @param alarm     Alarm to add.
     */
    QStatus AddAlarm(const Alarm& alarm);

    /**
     * Disassociate an alarm from a timer.
     *
     * @param alarm             Alarm to remove.
     * @param blockIfTriggered  If alarm has already been triggered, block the caller until AlarmTriggered returns.
     * @return  true iff the given alarm was found and removed.
     */
    bool RemoveAlarm(const Alarm& alarm, bool blockIfTriggered = true);

    /**
     * Remove any alarm for a specific listener returning the alarm. Returns a boolean if an alarm
     * was removed. This function is designed to be called in a loop to remove all alarms for a
     * specific listener. For example this function would be called from the listener's destructor.
     * The alarm is returned so the listener can free and resource referenced by the alarm.
     *
     * @param listener  The specific listener.
     * @param alarm     Alarm that was removed
     */
    bool RemoveAlarm(const AlarmListener& listener, Alarm& alarm);

    /**
     * Replace an existing Alarm.
     * Alarms that are  already "in-progress" (meaning they are scheduled for callback) cannot be replaced.
     * In this case, RemoveAlarm will return ER_NO_SUCH_ALARM and may optionally block until the triggered
     * alarm's AlarmTriggered callback has returned.
     *
     * @param origAlarm    Existing alarm to be replaced.
     * @param newAlarm     Alarm that will replace origAlarm if found.
     * @param blockIfTriggered  If alarm has already been triggered, block the caller until AlarmTriggered returns.
     *
     * @return  ER_OK if alarm was replaced
     *          ER_NO_SUCH_ALARM if origAlarm was already triggered or didn't exist.
     *          Any other error indicates that adding newAlarm failed (orig alarm will have been removed in this case).
     */
    QStatus ReplaceAlarm(const Alarm& origAlarm, const Alarm& newAlarm, bool blockIfTriggered = true);

    /**
     * Remove all pending alarms with a given alarm listener.
     *
     * @param listener   AlarmListener whose alarms will be removed from this timer.
     */
    void RemoveAlarmsWithListener(const AlarmListener& listener);

    /*
     * Test if the specified alarm is associated with this timer.
     *
     * @param alarm     Alarm to check.
     *
     * @return  Returns true if the alarm is associated with this timer, false if not.
     */
    bool HasAlarm(const Alarm& alarm);

    /**
     * Allow the currently executing AlarmTriggered callback to be reentered if another alarm is triggered.
     * Calling this method has no effect if timer was created with preventReentrancy == false;
     * Calling this method can only be made from within the AlarmTriggered timer callback.
     */
    void EnableReentrancy();

    /**
     * Check whether the current TimerThread is holding the lock
     *
     * @return true if the current thread is a timer thread that holds the reentrancy lock
     */
    bool ThreadHoldsLock() const;

    /**
     * Get the name of the Timer thread pool
     *
     * @return the name of the timer thread(s)
     */
    const qcc::String& GetName() const
    { return nameStr; }

    /**
     * TimerThread ThreadExit callback.
     * For internal use only.
     */
    void ThreadExit(qcc::Thread* thread);

  private:

    Mutex lock;
    std::multiset<Alarm, std::less<Alarm> >  alarms;
    Alarm* currentAlarm;
    bool expireOnExit;
    std::vector<TimerThread*> timerThreads;
    bool isRunning;
    int32_t controllerIdx;
    qcc::Timespec yieldControllerTime;
    bool preventReentrancy;
    Mutex reentrancyLock;
    qcc::String nameStr;
};

}

#endif
