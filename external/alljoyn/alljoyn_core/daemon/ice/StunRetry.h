#ifndef _STUNRETRY_H
#define _STUNRETRY_H
/**
 * @file StunRetry.h
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

#include <qcc/platform.h>
#include <qcc/time.h>
#include <StunTransactionID.h>
#include <qcc/Thread.h>
#include <qcc/Mutex.h>
#include "Status.h"

using namespace std;
using namespace qcc;

/** @internal */
#define QCC_MODULE "STUNRETRY"

static const uint8_t MAX_SEND_ATTEMPTS = 9;

class CheckRetry {
  public:
    CheckRetry() : sendAttempt(),
        queuedTime(),
        transactionValid(false),
        transaction()
    {
        /* Set the response reception time interval for all attempts to 500ms */
        for (uint8_t i = 0; i < MAX_SEND_ATTEMPTS; i++) {
            maxReceiveWaitMsec[i] = 500;
        }

        /* Set the response reception time interval for the first attempt to 200ms
         * and the second attempt to 400ms */
        if (MAX_SEND_ATTEMPTS >= 2) {
            maxReceiveWaitMsec[0] = 200;
            maxReceiveWaitMsec[1] = 400;
        }
    }

    ~CheckRetry(void) { }


    CheckRetry* Duplicate(void)
    {
        CheckRetry* duplicate = new CheckRetry();

        duplicate->sendAttempt = sendAttempt;
        duplicate->queuedTime = queuedTime;
        duplicate->transactionValid = transactionValid;
        duplicate->transaction = transaction;

        return duplicate;
    }



    void Init(void)
    {
        sendAttempt = 0;
        queuedTime = 0;
        transactionValid = false;
    }

    void SetTransactionID(StunTransactionID& tid) { transaction = tid; transactionValid = true; }

    StunTransactionID GetTransactionID(void) const { return transaction; }

    bool GetTransactionID(StunTransactionID& tid) const
    {
        if (transactionValid) {
            tid = transaction;
        }
        return transactionValid;
    }

    bool IsTransactionValid(void) const { return transactionValid; }

    bool AnyRetriesNotSent(void);

    bool RetryTimedOut(void);

    bool RetryAvailable(void);

    bool IncrementAttempts(void);

    double GetQueuedTimeOffset(void);

  private:

    uint8_t sendAttempt;
    uint32_t queuedTime;
    bool transactionValid;
    StunTransactionID transaction;

    uint16_t maxReceiveWaitMsec[MAX_SEND_ATTEMPTS];
};




class Retransmit {
  public:
    Retransmit() : sendAttempt(), receivedErrorCode(ER_OK),
        retransmitState(AwaitingTransmitSlot),
        queuedTime(0), transactionValid(), transaction()
    {
        /* Set the response reception time interval for all attempts to 500ms */
        for (uint8_t i = 0; i < MAX_SEND_ATTEMPTS; i++) {
            maxReceiveWaitMsec[i] = 500;
        }

        /* Set the response reception time interval for the first attempt to 200ms
         * and the second attempt to 400ms */
        if (MAX_SEND_ATTEMPTS >= 2) {
            maxReceiveWaitMsec[0] = 200;
            maxReceiveWaitMsec[1] = 400;
        }
    }

    ~Retransmit(void);

    typedef enum {
        AwaitingTransmitSlot,       /**< Awaiting pacing slot for transmit (or retransmit) */
        AwaitingResponse,           /**< Awaiting response from server */
        NoResponseToAllRetries,     /**< All retries sent with no successful response */
        ReceivedAuthenticateResponse,   /**< Received a authentication response. */
        ReceivedErrorResponse,      /**< Received an error response. */
        ReceivedSuccessResponse,    /**< Received a successful response. StunTurn completed */
        Error                       /**< Failed in send or receive. StunTurn completed */
    } RetransmitState;              /**< State of this Stun process object */


    /**
     * Get the state
     *
     * @return  The state of the Stun object.
     */
    void SetState(RetransmitState state);

    RetransmitState GetState(void);

    void SetErrorCode(QStatus errorCode);

    QStatus GetErrorCode(void) const;

    void SetTransactionID(StunTransactionID& tid) { transaction = tid; transactionValid = true; }

    bool GetTransactionID(StunTransactionID& tid) const { tid = transaction; return transactionValid; }

    void IncrementAttempts();

    void RecordKeepaliveTime(void);

    // Make it appear this has been waiting for longest time
    void PrematurelyAge(void) { queuedTime = 0; }

    uint16_t GetMaxReceiveWaitMsec(void);

    uint32_t GetAwaitingTransmitTimeMsecs(void);

    bool AnyRetriesNotSent(void);

    bool RetryTimedOut(void);

    bool RetryAvailable(void);

  private:

    uint8_t sendAttempt;
    QStatus receivedErrorCode;
    RetransmitState retransmitState;
    uint32_t queuedTime;
    bool transactionValid;
    StunTransactionID transaction;

    uint16_t maxReceiveWaitMsec[MAX_SEND_ATTEMPTS];
};

#undef QCC_MODULE
#endif
