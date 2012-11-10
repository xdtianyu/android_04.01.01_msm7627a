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
#ifndef _QCC_TIME_H
#define _QCC_TIME_H

#include <qcc/platform.h>
#include <qcc/String.h>

namespace qcc {


/**
 * Actually more than 500 million years from now but who's counting.
 */
static const uint64_t END_OF_TIME = static_cast<uint64_t>(-1);

typedef enum {
    TIME_ABSOLUTE,
    TIME_RELATIVE
} TimeBase;

/*
 * Forward declaration.
 */
struct Timespec;

/**
 * Get the current time.
 * @param ts  [OUT] Timespec filled with current time.
 */
void GetTimeNow(Timespec* ts);

/** Timespec */
struct Timespec {

    uint32_t seconds;       /**< Number of seconds since EPOCH */
    uint16_t mseconds;      /**< Milliseconds in EPOCH */

    Timespec() : seconds(0), mseconds(0) { }

    /**
     * Construct a Timespec that refers to an absolute (EPOCH based) time expressed in milliseconds
     * or a future time relative to the current time now.
     *
     * @param millis   Time in milliseconds.
     * @param base     Indicates if time is relative to now or absolute.
     */
    Timespec(uint64_t millis, TimeBase base = TIME_ABSOLUTE) {
        if (base == TIME_ABSOLUTE) {
            seconds = (uint32_t)(millis / 1000);
            mseconds = (uint16_t)(millis % 1000);
        } else {
            GetTimeNow(this);
            seconds += (uint32_t)((mseconds + millis) / 1000);
            mseconds = (uint16_t)((mseconds + millis) % 1000);
        }
    }

    Timespec& operator+=(const Timespec& other) {
        seconds += other.seconds + (mseconds + other.mseconds) / 1000;
        mseconds = (uint16_t)((mseconds + other.mseconds) % 1000);
        return *this;
    }

    Timespec& operator+=(uint32_t ms) {
        seconds += (ms + mseconds) / 1000;
        mseconds = (uint16_t)((ms + mseconds) % 1000);
        return *this;
    }

    bool operator<(const Timespec& other) const {
        return (seconds < other.seconds) || ((seconds == other.seconds) && (mseconds < other.mseconds));
    }

    bool operator<=(const Timespec& other) const {
        return (seconds < other.seconds) || ((seconds == other.seconds) && (mseconds <= other.mseconds));
    }

    bool operator==(const Timespec& other) const {
        return (seconds == other.seconds) && (mseconds == other.mseconds);
    }

    uint64_t GetAbsoluteMillis() const { return ((uint64_t)seconds * 1000) + (uint64_t)mseconds; }

};

/**
 * Return (non-absolute) timestamp in milliseconds.
 * Deprecated due to rollover every 8 days.
 * @return  timestamp in milliseconds.
 */
uint32_t GetTimestamp(void);

/**
 * Return (non-absolute) timestamp in milliseconds.
 * @return  timestamp in milliseconds.
 */
uint64_t GetTimestamp64(void);

inline Timespec operator+(const Timespec& tsa, const Timespec& tsb)
{
    Timespec ret;
    ret.seconds = tsa.seconds + tsb.seconds + (tsa.mseconds + tsb.mseconds) / 1000;
    ret.mseconds = (tsa.mseconds + tsb.mseconds) % 1000;
    return ret;
}

inline Timespec operator+(const Timespec& ts, uint32_t ms)
{
    Timespec ret;
    ret.seconds = ts.seconds + (ts.mseconds + ms) / 1000;
    ret.mseconds = (uint16_t)((ts.mseconds + ms) % 1000);
    return ret;
}

inline int64_t operator-(const Timespec& ts1, const Timespec& ts2)
{
    return ((int64_t)ts1.seconds - (int64_t)ts2.seconds) * 1000 + (int64_t)ts1.mseconds - (int64_t)ts2.mseconds;
}

/**
 * Return a formatted string for current UTC date and time. Format conforms to RFC 1123
 * e.g. "Tue, 30 Aug 2011 17:01:45 GMT"
 *
 * @return  The formatted date/time string.
 */
qcc::String UTCTime();

}
#endif
