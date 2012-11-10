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

#include <qcc/platform.h>

#include <qcc/Debug.h>
#include <qcc/Timer.h>
#include <Status.h>

#define QCC_MODULE  "TIMER"

#define WORKER_IDLE_TIMEOUT_MS  20
#define FALLBEHIND_WARNING_MS   500

using namespace std;
using namespace qcc;

int32_t qcc::Alarm::nextId = 0;

namespace qcc {

class TimerThread : public Thread {
  public:

    enum {
        STOPPED,    /**< Thread must be started via Start() */
        STARTING,   /**< Thread has been Started but is not ready to service requests */
        IDLE,       /**< Thrad is sleeping. Waiting to be alerted via Alert() */
        RUNNING,    /**< Thread is servicing an AlarmTriggered callback */
        STOPPING    /**< Thread is stopping due to extended idle time. Not ready for Start or Alert */
    } state;

    TimerThread(const String& name, int index, Timer* timer) :
        Thread(name),
        state(STOPPED),
        hasTimerLock(false),
        index(index),
        timer(timer),
        currentAlarm(NULL)
    { }

    virtual ~TimerThread() { }

    bool hasTimerLock;

    QStatus Start(void* arg, ThreadListener* listener);

    const Alarm* GetCurrentAlarm() const { return currentAlarm; }

    int GetIndex() const { return index; }

  protected:
    virtual ThreadReturn STDCALL Run(void* arg);

  private:
    const int index;
    Timer* timer;
    const Alarm* currentAlarm;
};

}

Timer::Timer(const char* name, bool expireOnExit, uint32_t concurency, bool preventReentrancy) :
    expireOnExit(expireOnExit),
    timerThreads(concurency),
    isRunning(false),
    controllerIdx(0),
    preventReentrancy(preventReentrancy),
    nameStr(name)
{
    for (uint32_t i = 0; i < timerThreads.size(); ++i) {
        timerThreads[i] = new TimerThread(nameStr, i, this);
    }
}

Timer::~Timer()
{
    Stop();
    Join();
    for (uint32_t i = 0; i < timerThreads.size(); ++i) {
        delete timerThreads[i];
        timerThreads[i] = NULL;
    }
}

QStatus Timer::Start()
{
    QStatus status = ER_OK;
    lock.Lock();
    if (!isRunning) {
        controllerIdx = 0;
        isRunning = true;
        status = timerThreads[0]->Start(NULL, this);
        isRunning = false;
        if (status == ER_OK) {
            uint64_t startTs = GetTimestamp64();
            while (timerThreads[0]->state != TimerThread::IDLE) {
                if ((startTs + 5000) < GetTimestamp64()) {
                    status = ER_FAIL;
                    break;
                } else {
                    lock.Unlock();
                    Sleep(2);
                    lock.Lock();
                }
            }
        }
        isRunning = (status == ER_OK);
    }
    lock.Unlock();
    return status;
}

QStatus Timer::Stop()
{
    QStatus status = ER_OK;
    lock.Lock();
    isRunning = false;
    lock.Unlock();
    for (size_t i = 0; i < timerThreads.size(); ++i) {
        lock.Lock();
        QStatus tStatus = timerThreads[i]->Stop();
        lock.Unlock();
        status = (status == ER_OK) ? tStatus : status;
    }
    return status;
}

QStatus Timer::Join()
{
    QStatus status = ER_OK;
    for (size_t i = 0; i < timerThreads.size(); ++i) {
        QStatus tStatus = timerThreads[i]->Join();
        status = (status == ER_OK) ? tStatus : status;
    }
    return status;
}

QStatus Timer::AddAlarm(const Alarm& alarm)
{
    QStatus status = ER_OK;
    lock.Lock();
    if (isRunning) {
        bool alertThread = alarms.empty() || (alarm < *alarms.begin());
        alarms.insert(alarm);

        if (alertThread && (controllerIdx >= 0)) {
            TimerThread* tt = timerThreads[controllerIdx];
            if (tt->state == TimerThread::IDLE) {
                status = tt->Alert();
            }
        }
    } else {
        status = ER_TIMER_EXITING;
    }
    lock.Unlock();
    return status;
}

bool Timer::RemoveAlarm(const Alarm& alarm, bool blockIfTriggered)
{
    bool foundAlarm = false;
    lock.Lock();
    if (isRunning) {
        if (alarm.periodMs) {
            multiset<Alarm>::iterator it = alarms.begin();
            while (it != alarms.end()) {
                if (it->id == alarm.id) {
                    foundAlarm = true;
                    alarms.erase(it);
                    break;
                }
                ++it;
            }
        } else {
            multiset<Alarm>::iterator it = alarms.find(alarm);
            if (it != alarms.end()) {
                foundAlarm = true;
                alarms.erase(it);
            }
        }
        if (blockIfTriggered && !foundAlarm) {
            /*
             * There might be a call in progress to the alarm that is being removed.
             * RemoveAlarm must not return until this alarm is finished.
             */
            for (size_t i = 0; i < timerThreads.size(); ++i) {
                if (timerThreads[i] == Thread::GetThread()) {
                    continue;
                }
                const Alarm* curAlarm = timerThreads[i]->GetCurrentAlarm();
                while (isRunning && curAlarm && (*curAlarm == alarm)) {
                    lock.Unlock();
                    qcc::Sleep(2);
                    lock.Lock();
                    curAlarm = timerThreads[i]->GetCurrentAlarm();
                }
            }
        }
    }
    lock.Unlock();
    return foundAlarm;
}

QStatus Timer::ReplaceAlarm(const Alarm& origAlarm, const Alarm& newAlarm, bool blockIfTriggered)
{
    QStatus status = ER_NO_SUCH_ALARM;
    lock.Lock();
    if (isRunning) {
        multiset<Alarm>::iterator it = alarms.find(origAlarm);
        if (it != alarms.end()) {
            alarms.erase(it);
            status = AddAlarm(newAlarm);
        } else if (blockIfTriggered) {
            /*
             * There might be a call in progress to origAlarm.
             * RemoveAlarm must not return until this alarm is finished.
             */
            for (size_t i = 0; i < timerThreads.size(); ++i) {
                if (timerThreads[i] == Thread::GetThread()) {
                    continue;
                }
                const Alarm* curAlarm = timerThreads[i]->GetCurrentAlarm();
                while (isRunning && curAlarm && (*curAlarm == origAlarm)) {
                    lock.Unlock();
                    qcc::Sleep(2);
                    lock.Lock();
                    curAlarm = timerThreads[i]->GetCurrentAlarm();
                }
            }
        }
    }
    lock.Unlock();
    return status;
}

bool Timer::RemoveAlarm(const AlarmListener& listener, Alarm& alarm)
{
    bool removedOne = false;
    lock.Lock();
    if (isRunning) {
        for (multiset<Alarm>::iterator it = alarms.begin(); it != alarms.end(); ++it) {
            if (it->listener == &listener) {
                alarms.erase(it);
                removedOne = true;
                break;
            }
        }
        /*
         * This function is most likely being called because the listener is about to be freed. If there
         * are no alarms remaining check that we are not currently servicing an alarm for this listener.
         * If we are, wait until the listener returns.
         */
        if (!removedOne) {
            for (size_t i = 0; i < timerThreads.size(); ++i) {
                if (timerThreads[i] == Thread::GetThread()) {
                    continue;
                }
                const Alarm* curAlarm = timerThreads[i]->GetCurrentAlarm();
                while (isRunning && curAlarm && (curAlarm->listener == &listener)) {
                    lock.Unlock();
                    qcc::Sleep(5);
                    lock.Lock();
                    curAlarm = timerThreads[i]->GetCurrentAlarm();
                }
            }
        }
    }
    lock.Unlock();
    return removedOne;
}

void Timer::RemoveAlarmsWithListener(const AlarmListener& listener)
{
    Alarm a;
    while (RemoveAlarm(listener, a)) {
    }
}

bool Timer::HasAlarm(const Alarm& alarm)
{
    bool ret = false;
    lock.Lock();
    if (isRunning) {
        ret = alarms.count(alarm) != 0;
    }
    lock.Unlock();
    return ret;
}

QStatus TimerThread::Start(void* arg, ThreadListener* listener)
{
    QStatus status = ER_OK;
    timer->lock.Lock();
    if (timer->isRunning) {
        status = Thread::Start(arg, listener);
        state = TimerThread::STARTING;
    }
    timer->lock.Unlock();
    return status;
}

ThreadReturn STDCALL TimerThread::Run(void* arg)
{
    QCC_DbgPrintf(("TimerThread::Run()"));

    /*
     * Enter the main loop with the timer lock held.
     */
    timer->lock.Lock();

    while (!IsStopping()) {
        QCC_DbgPrintf(("TimerThread::Run(): Looping."));
        Timespec now;
        GetTimeNow(&now);
        bool isController = (timer->controllerIdx == index);

        QCC_DbgPrintf(("TimerThread::Run(): isController == %d", isController));
        QCC_DbgPrintf(("TimerThread::Run(): controllerIdx == %d", timer->controllerIdx));

        /*
         * If the controller has relinquished its role and is off executing a
         * handler, the first thread back assumes the role of controller.  The
         * controller ensured that some thread would be awakened to come back
         * and do this if there was one idle or stopped.  If all threads were
         * off executing alarms, the first one back will assume the controller
         * role.
         */
        if (!isController && (timer->controllerIdx == -1)) {
            timer->controllerIdx = index;
            isController = true;
            QCC_DbgPrintf(("TimerThread::Run(): Assuming controller role, idx == %d", timer->controllerIdx));
        }

        /*
         * Check for something to do, either now or at some (alarm) time in the
         * future.
         */
        if (!timer->alarms.empty()) {
            QCC_DbgPrintf(("TimerThread::Run(): Alarms pending"));
            const Alarm& topAlarm = *(timer->alarms.begin());
            int64_t delay = topAlarm.alarmTime - now;

            /*
             * There is an alarm waiting to go off, but there is some delay
             * until the next alarm is scheduled to pop, so we might want to
             * sleep.  If there is a delay (the alarm is not due now) we sleep
             * if we're the controller thread or if we're a worker and the delay
             * time is low enough to make it worthwhile for the worker not to
             * stop.
             */
            if ((delay > 0) && (isController || (delay < WORKER_IDLE_TIMEOUT_MS))) {
                QCC_DbgPrintf(("TimerThread::Run(): Next alarm delay == %d", delay));
                state = IDLE;
                timer->lock.Unlock();
                Event evt(static_cast<uint32_t>(delay), 0);
                Event::Wait(evt);
                timer->lock.Lock();
                stopEvent.ResetEvent();
            } else if (isController || (delay <= 0)) {
                QCC_DbgPrintf(("TimerThread::Run(): Next alarm is due now"));
                /*
                 * There is an alarm waiting to go off.  We are either the
                 * controller or the alarm is past due.  If the alarm is past
                 * due, we want to print an error message if we are getting too
                 * far behind.
                 *
                 * Note that this does not necessarily mean that something bad
                 * is happening.  In the case of a threadpool, for example,
                 * since all threads are dispatched "now," corresponding alarms
                 * will always be late.  It is the case, however, that a
                 * generally busy system would begin to fall behind, and it
                 * would be useful to know this.  Therefore we do log a message
                 * if the system gets too far behind.  We define "too far" by
                 * the constant FALLBEHIND_WARNING_MS.
                 */
                if (delay < 0 && abs(delay) > FALLBEHIND_WARNING_MS) {
                    QCC_LogError(ER_TIMER_FALLBEHIND, ("TimerThread::Run(): Timer \"%s\" alarm is late by %ld ms",
                                                       Thread::GetThreadName(), abs(delay)));
                }

                TimerThread* tt = NULL;

                /*
                 * There may be several threads wandering through this code.
                 * One of them is acting as the controller, whose job it is to
                 * wake up or spin up threads to replace it when it goes off to
                 * execute an alarm.  If there are no more alarms to execute,
                 * the controller goes idle, but worker threads stop and exit.
                 *
                 * At this point, we know we have an alarm to execute.  Ideally
                 * we just want to directly execute the alarm without doing a
                 * context switch, so whatever thread (worker or controller) is
                 * executing should handle the alarm.
                 *
                 * Since the alarm may take an arbitrary length of time to
                 * complete, if we are the controller, we need to make sure that
                 * there is another thread that can assume the controller role
                 * if we are off executing an alarm.  An idle thread means it is
                 * waiting for an alarm to become due.  A stopped thread means
                 * it has exited since it found no work to do.
                 */
                if (isController) {
                    QCC_DbgPrintf(("TimerThread::Run(): Controller looking for worker"));

                    /*
                     * Look for an idle or stopped worker to execute alarm callback for us.
                     */
                    for (size_t i = 0; i < timer->timerThreads.size(); ++i) {
                        if (i != static_cast<size_t>(index)) {
                            if (timer->timerThreads[i]->state == TimerThread::IDLE) {
                                tt = timer->timerThreads[i];
                                QCC_DbgPrintf(("TimerThread::Run(): Found idle worker at index %d", i));
                                break;
                            } else if (timer->timerThreads[i]->state == TimerThread::STOPPED  && !timer->timerThreads[i]->IsRunning()) {
                                tt = timer->timerThreads[i];
                                QCC_DbgPrintf(("TimerThread::Run(): Found stopped worker at index %d", i));
                            }
                        }
                    }

                    /*
                     * If <tt> is non-NULL, then we have located a thread that
                     * will be able to take over the controller role if
                     * required, so either wake it up or start it depending on
                     * its current state.
                     */
                    if (tt) {
                        QCC_DbgPrintf(("TimerThread::Run(): Have timer thread (tt)"));
                        if (tt->state == TimerThread::IDLE) {
                            QCC_DbgPrintf(("TimerThread::Run(): Alert()ing idle timer thread (tt)"));
                            QStatus status = tt->Alert();
                            if (status != ER_OK) {
                                QCC_LogError(status, ("Error alerting timer thread %s", tt->GetName()));
                            }
                        } else if (tt->state == TimerThread::STOPPED) {
                            QCC_DbgPrintf(("TimerThread::Run(): Start()ing stopped timer thread (tt)"));
                            QStatus status = tt->Start(NULL, timer);
                            if (status != ER_OK) {
                                QCC_LogError(status, ("Error starting timer thread %s", tt->GetName()));
                            }
                        }
                    }
                }

                /*
                 * There is an alarm due to be executed now, and we are either
                 * the controller thread or a worker thread executing now.  in
                 * either case, we are going to handle the alarm at the head of
                 * the list.
                 */
                QCC_DbgPrintf(("TimerThread::Run(): Alarm due, the current thread is handling it"));
                multiset<Alarm>::iterator it = timer->alarms.begin();
                Alarm top = *it;
                timer->alarms.erase(it);
                currentAlarm = &top;
                state = RUNNING;

                /*
                 * If we are the controller, then we are going to have to yield
                 * our role since the alarm may take an arbitrary length of time
                 * to execute.  The next thread that wends its way through this
                 * run loop will assume the role.
                 */
                if (isController) {
                    timer->controllerIdx = -1;
                    GetTimeNow(&timer->yieldControllerTime);
                    QCC_DbgPrintf(("TimerThread::Run(): Yielding controller role"));
                    isController = false;
                }

                stopEvent.ResetEvent();
                timer->lock.Unlock();
                hasTimerLock = timer->preventReentrancy;
                if (hasTimerLock) {
                    timer->reentrancyLock.Lock();
                }
                QCC_DbgPrintf(("TimerThread::Run(): ******** AlarmTriggered()"));
                (top.listener->AlarmTriggered)(top, ER_OK);
                if (hasTimerLock) {
                    timer->reentrancyLock.Unlock();
                }
                timer->lock.Lock();
                currentAlarm = NULL;

                if (0 != top.periodMs) {
                    top.alarmTime += top.periodMs;
                    if (top.alarmTime < now) {
                        top.alarmTime = now;
                    }
                    QCC_DbgPrintf(("TimerThread::Run(): Adding back periodic alarm"));
                    timer->AddAlarm(top);
                }
            } else {
                /*
                 * This is a worker (non-controller) thread with nothing to do
                 * immediately, so we just stop it until we have a need for it
                 * to be consuming resources.
                 */
                QCC_DbgPrintf(("TimerThread::Run(): Worker with nothing to do"));
                state = STOPPING;
                break;
            }
        } else {
            /*
             * The alarm list is empty, so we only have a need to have a single
             * controller thread running.  If we are that controller, we wait
             * until there is something to do.  If we are not that controller,
             * we just stop running so we don't consume resources.
             */
            QCC_DbgPrintf(("TimerThread::Run(): Alarm list is empty"));
            if (isController) {
                QCC_DbgPrintf(("TimerThread::Run(): Controller going idle"));
                state = IDLE;
                stopEvent.ResetEvent();
                timer->lock.Unlock();
                Event evt(Event::WAIT_FOREVER, 0);
                Event::Wait(evt);
                timer->lock.Lock();
            } else {
                QCC_DbgPrintf(("TimerThread::Run(): non-Controller stopping"));
                state = STOPPING;
                break;
            }
        }
    }

    /*
     * We entered the main loop with the lock taken, so we need to give it here.
     */
    state = STOPPING;
    timer->lock.Unlock();
    return (ThreadReturn) 0;
}

void Timer::ThreadExit(Thread* thread)
{
    TimerThread* tt = static_cast<TimerThread*>(thread);
    lock.Lock();
    if ((tt->GetIndex() == controllerIdx) && expireOnExit) {
        /* Call all alarms */
        while (!alarms.empty()) {
            /*
             * Note it is possible that the callback will call RemoveAlarm()
             */
            multiset<Alarm>::iterator it = alarms.begin();
            Alarm alarm = *it;
            alarms.erase(it);
            lock.Unlock();
            tt->hasTimerLock = preventReentrancy;
            if (tt->hasTimerLock) {
                reentrancyLock.Lock();
            }
            alarm.listener->AlarmTriggered(alarm, ER_TIMER_EXITING);
            if (tt->hasTimerLock) {
                reentrancyLock.Unlock();
            }
            lock.Lock();
        }
    }
    tt->state = TimerThread::STOPPED;
    lock.Unlock();
    tt->Join();
}

void Timer::EnableReentrancy()
{
    Thread* thread = Thread::GetThread();
    if (nameStr == thread->GetName()) {
        TimerThread* tt = static_cast<TimerThread*>(thread);
        if (tt->hasTimerLock) {
            tt->hasTimerLock = false;
            reentrancyLock.Unlock();
        }
    } else {
        QCC_DbgPrintf(("Invalid call to Timer::EnableReentrancy from thread %s; only allowed from %s", Thread::GetThreadName(), nameStr.c_str()));
    }
}

bool Timer::ThreadHoldsLock() const
{
    Thread* thread = Thread::GetThread();
    if (nameStr == thread->GetName()) {
        TimerThread* tt = static_cast<TimerThread*>(thread);
        return tt->hasTimerLock;
    }

    return false;
}
