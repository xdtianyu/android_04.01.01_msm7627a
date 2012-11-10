/**
 * @file
 *
 * Platform specific header files that defines time related functions
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

#include <time.h>
#include <stdio.h>

#if defined(QCC_OS_DARWIN)
#include <sys/time.h>
#endif

#include <qcc/time.h>



using namespace qcc;

static void platform_gettime(struct timespec* ts)
{

#if defined(QCC_OS_DARWIN)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_MONOTONIC, ts);
#endif
}

static time_t s_clockOffset = 0;

uint32_t qcc::GetTimestamp(void)
{
    struct timespec ts;
    uint32_t ret_val;

    platform_gettime(&ts);

    if (0 == s_clockOffset) {
        s_clockOffset = ts.tv_sec;
    }

    ret_val = ((uint32_t)(ts.tv_sec - s_clockOffset)) * 1000;
    ret_val += (uint32_t)ts.tv_nsec / 1000000;

    return ret_val;
}

uint64_t qcc::GetTimestamp64(void)
{
    struct timespec ts;
    uint64_t ret_val;

    platform_gettime(&ts);

    if (0 == s_clockOffset) {
        s_clockOffset = ts.tv_sec;
    }

    ret_val = ((uint32_t)(ts.tv_sec - s_clockOffset)) * 1000;
    ret_val += (uint32_t)ts.tv_nsec / 1000000;

    return ret_val;
}

void qcc::GetTimeNow(Timespec* ts)
{
    struct timespec _ts;
    platform_gettime(&_ts);
    ts->seconds = _ts.tv_sec;
    ts->mseconds = _ts.tv_nsec / 1000000;
}

qcc::String qcc::UTCTime()
{
    static const char* Day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* Month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    char buf[32];
    time_t t;
    time(&t);
    struct tm* utc = gmtime(&t);
    snprintf(buf, 32, "%s, %02d %s %04d %02d:%02d:%02d GMT",
             Day[utc->tm_wday],
             utc->tm_mday,
             Month[utc->tm_mon],
             1900 + utc->tm_year,
             utc->tm_hour,
             utc->tm_min,
             utc->tm_sec);

    return buf;
}
