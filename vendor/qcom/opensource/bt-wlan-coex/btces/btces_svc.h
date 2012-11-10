/*
Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.

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
#ifndef _BTCES_SVC_H_
#define _BTCES_SVC_H_

/*------------------------------------------------------------------------------
                 BTC-ES SERVICES LAYER
------------------------------------------------------------------------------*/

/**
  @file btces_svc.h

  The BTC-ES Interface for platform independent services to modules in BTC-ES.
  These are not part of the public API.
*/

/*=============================================================================

                       EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order. Please
  use ISO format for dates.

  when        who  what, where, why
  ----------  ---  -----------------------------------------------------------
  2010-03-03   pj  Initial Open Source version

=============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

#include "btces_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/** All native event data structures must be members of this union */
typedef union
{
  btces_bt_addr_struct  addr;           /**< Remote Bluetooth Device Address */
} btces_native_event_data_union;


/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Enumerated types
 * -------------------------------------------------------------------------*/

/**
   Abstract enumeration of native events from all platforms that are not
   provided by HCI traffic analysis
*/
typedef enum
{
  BTCES_NATIVE_EVENT_DEVICE_SWITCHED_ON = 0,  /**< BT is now on; no event data */
  BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF,     /**< BT is now off; no event data */
  BTCES_NATIVE_EVENT_A2DP_STREAM_START,       /**< A2DP Streaming active, to BT Addr in the event data */
  BTCES_NATIVE_EVENT_A2DP_STREAM_STOP,        /**< A2DP Streaming ended, to BT Addr in the event data */
  BTCES_NATIVE_EVENT_MAX                      /**< This value and higher are invalid */
} btces_native_event_enum;


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
FUNCTION:  btces_svc_hci_command_in()
==============================================================*/
/**
    BTC-ES is told of HCI traffic being sent to the Bluetooth SoC.

    Each outgoing HCI command should be reported to BTC-ES with a call to this API,
    one-for-one, in the order they are queued for transmission to the Bluetooth
    Controller SoC.

    If BTC-ES is not running, the function returns and takes no action. Otherwise,
    the command is parsed, BTC-ES state information is updated if needed and a
    BTC-ES event may be reported by way of the callback supplied to btces_register(),
    all before this function returns.

    Invalid use cases such as hci_command_buffer_ptr = NULL or length = 0 will be
    ignored; if length is too small to extract the required parameters from the
    HCI command, the command will be discarded.

    After BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF has been given to btces_svc_native_event_in(),
    no further HCI commands should be reported to this API until power is restored.
    Otherwise, BTC-ES will erroneously think Bluetooth power has been turned back on.
*/

void btces_svc_hci_command_in
(
  uint8         *hci_command_buffer_ptr,
    /**< [in] Pointer a buffer containing an outgoing HCI command and its serialized
      data, before H4/H5 protocol wrapping or in-band sleep protocol bytes are added.
      Therefore, the first two bytes of hci_command_buffer_ptr should contain the
      combined OGF and OCF.
    */

  unsigned int  length
    /**< [in] The length in bytes of the HCI command; according to the Bluetooth
      specification, length should not be more than 255 plus the HCI command header,
      or 258 bytes in all.
    */
);

/*==============================================================
FUNCTION:  btces_svc_hci_event_in()
==============================================================*/
/**
    BTC-ES is told of HCI traffic being received from the Bluetooth SoC.

    Each incoming HCI event should be reported to BTC-ES with a call to this API,
    one-for-one, in the order they are received from the Bluetooth Controller SoC.

    If BTC-ES is not running, the function returns and takes no action. Otherwise,
    the event is parsed, BTC-ES state information is updated if needed and a BTC-ES
    event may be reported by way of the callback supplied to btces_register(), all
    before this function returns.

    Invalid use cases such as hci_event_buffer_ptr = NULL or length = 0 will be
    ignored; if length is too small to extract the required parameters from the
    HCI event, the event will be discarded.

    After BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF has been given to btces_svc_native_event_in(),
    no further HCI events should be reported to this API until power is restored.
    Otherwise, BTC-ES will erroneously think Bluetooth power has been turned back on.
*/

void btces_svc_hci_event_in
(
  uint8         *hci_event_buffer_ptr,
    /**< [in] Pointer a buffer containing an incoming HCI event and its
      serialized data, after H4/H5 protocol unwrapping or in-band sleep protocol
      bytes are removed. Therefore, the first byte of hci_event_buffer_ptr should
      contain the event code.
    */

  unsigned int  length
    /**< [in] The length in bytes of the HCI event; according to the Bluetooth
      specification, length should not be more than 255 plus the HCI event header,
      or 257 bytes in all.
    */
);

/*==============================================================
FUNCTION:  btces_svc_native_event_in()
==============================================================*/
/**
    BTC-ES is told of a platform event.

    Each platform event that occurs should be reported to BTC-ES with a call to
    this API, one-for-one, in the order they occur on the native platform. The
    platform must translate the native value of the event in the platform-independent
    BTC-ES defined value before calling this API.

    If BTC-ES is not running, the function returns and takes no action. Otherwise,
    BTC-ES state information is updated if needed and a BTC-ES event may be
    reported by way of the callback supplied to btces_register(), all before this
    function returns.

    Invalid use cases such as an undefined native_event or a missing, but required
    native_event_data_ptr will be ignored.
*/

void btces_svc_native_event_in
(
  btces_native_event_enum       native_event,
    /**< [in] One of the possible events possibly supported by the hosting platform.
      The event has already been translated from the platform-specific value. The
      initial list contains only the platform events needed, where HCI Traffic
      Analysis does not meet the requirements of the outgoing BTC-ES events.
      Note, the enumerated values do not have to match outgoing BTC-ES events.
    */

  btces_native_event_data_union *native_event_data_ptr
    /**< [in] Pointer a buffer containing the data associated with the native event
      (if any), formatted into one of the structures defined by BTC-ES. BTC-ES will
      examine native_event and then use casting operations on the data pointer to
      extract the associated data. If there is no data associated with a particular
      event, the pointer is ignored.
    */
);

#ifdef __cplusplus
}
#endif

#endif /* _BTCES_SVC_H_ */

