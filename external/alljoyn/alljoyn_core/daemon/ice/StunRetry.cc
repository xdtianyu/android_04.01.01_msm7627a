/**
 * @file StunRetry.cc
 *
 *
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
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

#include <qcc/time.h>
#include <StunRetry.h>

/** @internal */
#define QCC_MODULE "STUNRETRY"

using namespace qcc;

bool CheckRetry::AnyRetriesNotSent(void)
{
    return (sendAttempt < MAX_SEND_ATTEMPTS - 1);
}


bool CheckRetry::RetryTimedOut(void)
{
    return ((GetTimestamp() - queuedTime) >= maxReceiveWaitMsec[sendAttempt]);
}


bool CheckRetry::RetryAvailable(void)
{
    return (AnyRetriesNotSent() && RetryTimedOut());
}

double CheckRetry::GetQueuedTimeOffset(void)
{
    return (queuedTime + maxReceiveWaitMsec[sendAttempt]);
}


bool CheckRetry::IncrementAttempts(void)
{
    bool attemptsRemaining = false;

    if (sendAttempt < MAX_SEND_ATTEMPTS - 1) {
        sendAttempt++;
        // record time of this attempt
        queuedTime = GetTimestamp();
        attemptsRemaining = true;
    } else {
        attemptsRemaining = false;
    }

    return attemptsRemaining;
}





/*
 *****************************************************************************************
 *****************************************************************************************
 *****************************************************************************************
 */

Retransmit::~Retransmit(void)
{
}


Retransmit::RetransmitState Retransmit::GetState(void)
{
    RetransmitState state;

    state = retransmitState;

    return state;
}

void Retransmit::SetState(RetransmitState state)
{
    retransmitState = state;
}

QStatus Retransmit::GetErrorCode(void) const {
    return receivedErrorCode;
}

void Retransmit::SetErrorCode(QStatus errorCode) {
    receivedErrorCode = errorCode;
}

void Retransmit::IncrementAttempts()
{
    if (sendAttempt < MAX_SEND_ATTEMPTS) {
        sendAttempt++;
    }

    // record time of this attempt
    queuedTime = GetTimestamp();

    retransmitState = AwaitingResponse;
}

uint16_t Retransmit::GetMaxReceiveWaitMsec(void)
{
    uint16_t wait = 0;

    // ToDo RFC 5389 7.2.1, and draft-ietf-mmusic-ice-?

    // because we pre-increment, index is off by 1
    if (sendAttempt - 1 < MAX_SEND_ATTEMPTS) {
        wait = maxReceiveWaitMsec[sendAttempt - 1];
    }

    return wait;
}

uint32_t Retransmit::GetAwaitingTransmitTimeMsecs(void)
{
    uint32_t transmitTime;

    transmitTime = GetTimestamp() - queuedTime;

    return transmitTime;
}

void Retransmit::RecordKeepaliveTime(void)
{
    // record time of this attempt
    queuedTime = GetTimestamp();
}

bool Retransmit::AnyRetriesNotSent(void)
{
    return (sendAttempt < MAX_SEND_ATTEMPTS - 1);
}


bool Retransmit::RetryTimedOut(void)
{
    return ((GetTimestamp() - queuedTime) >= maxReceiveWaitMsec[sendAttempt]);
}


bool Retransmit::RetryAvailable(void)
{
    return (AnyRetriesNotSent() && RetryTimedOut());
}
