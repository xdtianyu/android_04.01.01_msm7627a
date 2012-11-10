/*
Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _BTCES_PFAL_H_
#define _BTCES_PFAL_H_

/*------------------------------------------------------------------------------
                 BTC-ES OS PLATFORM ADAPTATION LAYER
------------------------------------------------------------------------------*/

/**
  @file btces_pfal.h

  The BTC-ES Interface for providing Platform OS abstraction required by BTC-ES
  for its operation.
*/

/*=============================================================================

                       EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order. Please
  use ISO format for dates.

  when        who  what, where, why
  ----------  ---  -----------------------------------------------------------
  2011-04-01   ag  Fixing Compilation Issue with new DBUS api's on Honeycomb
  2010-03-03   pj  Initial Open Source version

=============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "btces_types.h"
#include "btces_plat.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

#define BTCES_INVALID_WLAN_CHANS  (0xC000)   /* WLAN Channel range: 1-14, bits 0-13 */

#ifdef DBUS_OLD_APIS
#define dbus_watch_get_unix_fd dbus_watch_get_fd
#endif

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
typedef void (btces_pfal_timer_cb_type)(void *user_data_ptr);

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Enumerated types
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Structure definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Macros
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Constant values
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function declarations
 * -------------------------------------------------------------------------*/

/*==============================================================
FUNCTION:  btces_pfal_init
==============================================================*/
/**
    BTC-ES initializes the platform dependent layer to begin operation.

    Initialize all the PFAL API services so that BTC-ES can call them; any
    required native resources are dynamically allocated. This API must be
    called before any other btces_pfal_xxx() APIs, except as may be noted
    in other API descriptions.

    @see  btces_pfal_deinit

    @return  BTCES_OK: The PFAL layer was initialized successfully.
    @return  BTCES_FAIL: Initialization of the PFAL layer failed for some reason.
*/
BTCES_STATUS btces_pfal_init( void );

/*==============================================================
FUNCTION:  btces_pfal_deinit
==============================================================*/
/**
    BTC-ES is closing down, and is finished with the platform dependent layer.

    De-Initialize the PFAL layer and services; native resources are released.
    The dedicated mutex that is captured by btces_pfal_get_token() must be
    automatically released as part of this service.

    @see  btces_pfal_deinit
    @see  btces_pfal_get_token

    @return  none
*/
void btces_pfal_deinit( void );

/*==============================================================
FUNCTION:  btces_pfal_get_bt_power
==============================================================*/
/**
    BTC-ES needs to find out the current state of the Bluetooth subsystem,
    "On" or "Off"

    BTC-ES uses this API when it is initialized to find out the state of
    the native Bluetooth stack. Afterwards, BTC-ES relies on HCI Traffic
    Analysis and Native Event reporting to track the stack state.

    @return  BTCES_OK: The Bluetooth power state was returned successfully.
    @return  BTCES_FAIL: The function failed for some undefined reason.
    @return  BTCES_STATUS_NOT_INITIALIZED: The PFAL layer is not ready to perform operations.
    @return  BTCES_STATUS_INVALID_PARAMETERS: bt_power_ptr was NULL.
*/
BTCES_STATUS btces_pfal_get_bt_power
(
  int *bt_power_ptr
  /**< [out]: Pointer to where to store the Bluetooth power state;
  zero means "Off" non-zero means "On" */
);

/*==============================================================
FUNCTION:  btces_pfal_malloc
==============================================================*/
/**
    BTC-ES requests a block of memory to be allocated.

    Behaves like the C Standard Library malloc(). The allocated
    memory is not initialized.

    @see btces_pfal_free

    @return  Pointer to allocated memory or NULL.
*/
void * btces_pfal_malloc
(
  int size
  /**< [in]: The requested size, in bytes, of the memory block to be allocated. */
);

/*==============================================================
FUNCTION:  btces_pfal_free
==============================================================*/
/**
    BTC-ES requests a previously allocated block of memory to be freed.

    Behaves like C Standard Library free(). The memory must have been
    previously allocated by btces_pfal_malloc().

    @see btces_pfal_malloc

    @return  none
*/
void btces_pfal_free
(
  void *mem_ptr
  /**< [in]: A pointer to the memory block to be freed; a value of NULL is ignored. */
);

/*==============================================================
FUNCTION:  btces_pfal_get_token
==============================================================*/
/**
    The current execution thread of BTC-ES waits to be granted exclusive
    access to its data.

    BTC-ES uses this API to ensure it is the only execution thread within
    a region bounded by btces_pfal_get_token() and btces_pfal_release_token().
    The current execution thread will cease and defer to a different thread
    running BTC-ES code that has already acquired the token. There are
    additional specifications to this API that should be noted:

    * If multiple executions threads are waiting on the token, it is platform
      dependent which thread is scheduled to execute after the owning thread
      releases the token.
    * BTC-ES will not call this API more than once in the same execution thread,
      so the required behavior in that use case is unspecified.
    * If an execution thread of BTC-ES code calls btces_pfal_deinit(), then all
      other execution threads waiting on the token must resume, but a returned
      error is optional; BTC-ES will make a separate test to check if another
      thread has shut down BTC-ES during the wait.

    These specified behaviors are intended to simplify the implementation of
    this PFAL service.

    @see btces_pfal_release_token

    @return BTCES_OK: The token was acquired successfully (but BTC-ES will
            make an extra test to ensure that).
    @return BTCES_FAIL: The function failed for some undefined reason.
    @return BTCES_STATUS_NOT_INITIALIZED: The PFAL layer is not ready to
            perform operations.
*/
BTCES_STATUS btces_pfal_get_token( void );

/*==============================================================
FUNCTION:  btces_pfal_release_token
==============================================================*/
/**
    The current execution thread of BTC-ES no longer needs exclusive
    access to its data.

    BTC-ES uses this API to indicate it ending its protected
    operation that began after acquiring the token; see the
    specification of btces_pfal_get_token()for additional behaviors.

    @see btces_pfal_get_token

    @return none
*/
void btces_pfal_release_token( void );

/*==============================================================
FUNCTION:  btces_pfal_start_timer
==============================================================*/
/**
    BTC-ES starts up a timer to schedule a callback function to
    execute in the future.

    This service registers a callback to execute once after the
    specified time. There is not a means for BTC-ES to find out if
    the timer is still running, but BTC-ES may try to cancel the
    timer before it expires. BTC-ES will use unique values of
    user_data to distinguish between timeouts and thus guard against
    cancel / expiration race conditions.

    The current design plans for BTC-ES expects to only have the
    need for one timer at a time, so if BTC-ES schedules a timer
    while another one is running, the running timer may be canceled
    inside this function if possible.

    The callback will be written to tolerate being
    executed if BTC-ES is shut down, in case it is possible for
    the callback to execute after btces_pfal_deinit().

    @return BTCES_OK: The timeout callback was scheduled successfully.
    @return BTCES_FAIL: The function failed for some undefined reason.
    @return BTCES_STATUS_INVALID_PARAMETERS: timer_cb_ptr or timer_id_ptr
            was NULL or timeout was zero.
    @return BTCES_STATUS_NOT_INITIALIZED: The PFAL layer is not ready
            to perform operations.
*/
BTCES_STATUS btces_pfal_start_timer
(
  uint16  timeout_ms,
  /**< [in]: Time value in milliseconds when the timer is to
             expire and the callback executed. */
  btces_pfal_timer_cb_type  *timer_cb_ptr,
  /**< [in]: Pointer a service to be called by BTC-ES when the timer
             expires. The pointer cannot be NULL. The callback
             service is described by btces_pfal_timer_cb_type */
  void  *user_data,
  /**< [in]: Opaque user-supplied data. This same value will always
             be passed to the callback service indicated by timer_cb_ptr. */
  void  **timer_id_ptr
  /**< [out]: Pointer to opaque platform-supplied timer ID. This same value will
              be used if the timer is canceled via btces_pfal_stop_timer(). */
);

/*==============================================================
FUNCTION:  btces_pfal_stop_timer
==============================================================*/
/**
    BTC-ES cancels a running timer.

    This service requests the specified timer to be canceled, thus
    avoiding the associated callback from being executed. BTC-ES
    is not concerned if the timer already expired or timer_id is
    no longer valid.

    This service is provided to prevent an excessive number of
    obsolete timers from running, as BTC-ES only needs at most one
    running timer. Thus, a platform may choose to provide an empty
    implementation of this service.
*/
void btces_pfal_stop_timer
(
  void  *timer_id
  /**< [in]: Opaque timer ID, originally returned by btces_pfal_start_timer().
             If the specified timer is no longer running, there are no side
             effects visible to BTC-ES. */
);

/*==============================================================
FUNCTION:  btces_msg_*() functions
           BTCES_MSG_*() macros
==============================================================*/
/**
    BTC-ES reports messages intended for debug purposes. The platform
    decides how each message is treated depending on the native debug
    settings and the importance of the message.

    The BTCES_MSG_*() macros service provides a means for BTC-ES to provide
    debug output.  The usual format conversion specs as with printf() should be
    available in the host platform. The entire expression must be enclosed in
    a set of parenthesis, for example:

    BTCES_MSG_ERROR( ("BTC-ES: Unexpected value: %d" BTCES_EOL, val) );

    BTCES_EOL lets the platform define the preferred end-of-line character(s).

    These services allow BTC-ES debug messages to be selectively compiled out
    based on debug settings and the level of importance of the message.
    The platform specific file, btces_plat.h, will resolve these macros.

    Macro usage: BTCES_MSG_xxx( (const char *format, ...) );

    * BTCES_MSG_FATAL(): Used when BTC-ES is unable to proceed; the service
      is not expected not to return, but BTC-ES will attempt to shut down
      and return if it does.

    * BTCES_MSG_ERROR(): Used when BTC-ES detects an internal error or
      inconsistency. It is not used just because BTC-ES returns an error
      to the caller or gets an error from a PFAL service.

    * BTCES_MSG_HIGH(): Informative, but terse: Used by BTC-ES to report
      something important with a short amount of output.

    * BTCES_MSG_MEDIUM(): Informative: Used by BTC-ES for more typical reports.

    * BTCES_MSG_LOW(): Informative, verbose: Used by BTC-ES for more verbose
      reporting, such as function entry / exit, parameter values, etc.

    The macro BTCES_ASSERT(boolean_condition) makes use of BTCES_MSG_ERROR() to
    log a generic assertion failure message if condition evaluates to zero (i.e.,
    is false).

    @return none
*/

void btces_msg_fatal(const char *format, ...);
void btces_msg_error(const char *format, ...);
void btces_msg_high(const char *format, ...);
void btces_msg_medium(const char *format, ...);
void btces_msg_low(const char *format, ...);

/*==============================================================
FUNCTION:  btces_pfal_wlan_chan
==============================================================*/
/**
    BTC-ES tells the Bluetooth subsystem what channel(s) WLAN is using for
    AFH consideration, in the same format as btces_wlan_chan().

    This API informs the native Bluetooth subsystem what WLAN channels are in use
    (if any). The service is likely to take the information and construct a
    79-bit Bluetooth AFH channel mask to send to the Bluetooth SoC. The creation
    of the channel mask must decide, for each Bluetooth channel frequency, whether
    it should be removed from use based on the proximity of the WLAN channel
    frequencies in use.

    If the Channel Assessment feature in the Bluetooth SoC should be disabled
    while WLAN is using one or more channels, it is up to the platform code below
    this API to turn it off, and turn it back on later when WLAN is inactive.

    @see btces_wlan_chan

    @return BTCES_OK: The set of channels WLAN is using was accepted.
    @return BTCES_STATUS_INVALID_PARAMETERS: Invalid bit positions in wlan_channels were set to 1, so it was discarded.
    @return BTCES_STATUS_NOT_INITIALIZED: The PFAL layer is not ready to perform operations.
*/
BTCES_STATUS btces_pfal_wlan_chan
(
  uint16 wlan_channels
  /**<< [in] A 16 bit field with bits set = 1 to list the WLAN channels currently in use:
              Bit 0 (LSB): WLAN Channel 1 is in use (2412000 KHz)
              Bit 1: WLAN Channel 2 is in use (2417000 KHz)
              Bit n: WLAN Channel n+1 is in use
              Bit 13: WLAN Channel 14 is in use (2484000 KHz)
              Bits 14, 15: Must be zero

              0x0000 means WLAN is not using any channels. */
);

#endif /* _BTCES_PFAL_H_ */

