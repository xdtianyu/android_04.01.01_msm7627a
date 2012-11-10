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
#ifndef _QCOM_BTCES_H_
#define _QCOM_BTCES_H_

/*------------------------------------------------------------------------------
                 BTC-ES Bluetooth Coexistence Events Source
------------------------------------------------------------------------------*/

/**
  @file btces.h

  This file provides the public interface for the Qualcomm Bluetooth Coexistence
  Event Source.  This module plugs into the HCI transport layer and provides
  event synthesis and aggregation for Bluetooth events that relate to WiFi
  coexistence.
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

/**
     @mainpage BTC-ES: Bluetooth Coexistence-Events Source

     BTC-ES exposes a set of APIs allowing a client to subscribe to a stream of
     events announcing Bluetooth activity that is important for proper coexistence
     with WiFi.

*/

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

#define BTCES_INVALID_CONN_HANDLE (0xFFFF)  /**< Invalid connection handle */

/* ACL and Sync connection attempt results */
#define BTCES_CONN_STATUS_FAIL      (0)         /**< Connection failed */
#define BTCES_CONN_STATUS_SUCCESS   (1)         /**< Connection successful */

/** ACL and Sync link types
  These must match the Bluetooth Spec!
*/
#define BTCES_LINK_TYPE_SCO         (0)   /**< SCO Link */
#define BTCES_LINK_TYPE_ACL         (1)   /**< ACL Link */
#define BTCES_LINK_TYPE_ESCO        (2)   /**< eSCO Link */
#define BTCES_LINK_TYPE_MAX         (3)   /**< This value and higher are invalid */

/** ACL link modes
  These must match the Bluetooth Spec!
*/
#define BTCES_MODE_TYPE_ACTIVE      (0)   /**< Active mode */
#define BTCES_MODE_TYPE_HOLD        (1)   /**< Hold mode */
#define BTCES_MODE_TYPE_SNIFF       (2)   /**< Sniff mode */
#define BTCES_MODE_TYPE_PARK        (3)   /**< Park mode */
#define BTCES_MODE_TYPE_MAX         (4)   /**< This value and higher are invalid */


/*----------------------------------------------------------------------------
 * Enumerated types
 * -------------------------------------------------------------------------*/

/** BTC-ES events reported to the client, with associated event_data. */
typedef enum
{
  BTCES_EVENT_DEVICE_SWITCHED_ON,       /**< No event_data */
  BTCES_EVENT_DEVICE_SWITCHED_OFF,      /**< No event_data */
  BTCES_EVENT_INQUIRY_STARTED,          /**< No event_data */
  BTCES_EVENT_INQUIRY_STOPPED,          /**< No event_data */
  BTCES_EVENT_INQUIRY_SCAN_STARTED,     /**< Event not supported */
  BTCES_EVENT_INQUIRY_SCAN_STOPPED,     /**< Event not supported */
  BTCES_EVENT_PAGE_STARTED,             /**< No event_data */
  BTCES_EVENT_PAGE_STOPPED,             /**< No event_data */
  BTCES_EVENT_PAGE_SCAN_STARTED,        /**< Event not supported */
  BTCES_EVENT_PAGE_SCAN_STOPPED,        /**< Event not supported */
  BTCES_EVENT_CREATE_ACL_CONNECTION,    /**< See btces_bt_addr_struct */
  BTCES_EVENT_ACL_CONNECTION_COMPLETE,  /**< See btces_event_data_acl_comp_struct */
  BTCES_EVENT_CREATE_SYNC_CONNECTION,   /**< See btces_bt_addr_struct */
  BTCES_EVENT_SYNC_CONNECTION_COMPLETE, /**< See btces_event_data_sync_comp_up_struct */
  BTCES_EVENT_SYNC_CONNECTION_UPDATED,  /**< See btces_event_data_sync_comp_up_struct */
  BTCES_EVENT_DISCONNECTION_COMPLETE,   /**< See btces_event_data_disc_comp_struct */
  BTCES_EVENT_MODE_CHANGED,             /**< See btces_event_data_mode_struct */
  BTCES_EVENT_A2DP_STREAM_START,        /**< See btces_bt_addr_struct */
  BTCES_EVENT_A2DP_STREAM_STOP,         /**< See btces_bt_addr_struct */
  BTCES_EVENT_MAX                       /**< This value and higher are invalid */
} btces_event_enum;


/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/* Note! Event data structures are organized to avoid structure padding */

/** Event Data structure used for BTCES_EVENT_ACL_CONNECTION_COMPLETE */
typedef struct
{
  btces_bt_addr_struct  addr;         /**< Remote device address */
  uint16                conn_handle;  /**< Connection handle */
  uint8                 conn_status;  /**< Connection success/fail */
} btces_event_data_acl_comp_struct;

/** Event Data structure used for
   BTCES_EVENT_SYNC_CONNECTION_COMPLETE,
   BTCES_EVENT_SYNC_CONNECTION_UPDATED
*/
typedef struct
{
  btces_bt_addr_struct    addr;         /**< Remote device address */
  uint16                  conn_handle;  /**< Connection handle */
  uint8                   conn_status;  /**< Connection success/fail */
  uint8                   link_type;    /**< SCO or eSCO only */
  uint8                   sco_interval; /**< SCO Instance, or Tsco, in number of slots */
  uint8                   sco_window;   /**< SCO Window, in number of slots */
  uint8                   retrans_win;  /**< eSCO retransmission window, in number of slots */
} btces_event_data_sync_comp_up_struct;

/** Event Data structure used for BTCES_EVENT_DISCONNECTION_COMPLETE */
typedef struct
{
  uint16  conn_handle;  /**< Connection handle */
} btces_event_data_disc_comp_struct;

/** Event Data structure used for BTCES_EVENT_MODE_CHANGED */
typedef struct
{
  uint16  conn_handle;  /**< Connection handle */
  uint8   mode;         /**< Connection mode */
} btces_event_data_mode_struct;


/** All event data structures must be members of this union */
typedef union
{
  btces_bt_addr_struct                  bt_addr;      /**< For events with only a BT Addr in event_data */
  btces_event_data_acl_comp_struct      acl_comp;     /**< BTCES_EVENT_ACL_CONNECTION_COMPLETE events */
  btces_event_data_sync_comp_up_struct  sync_comp_up; /**< BTCES_EVENT_SYNC_CONNECTION_COMPLETE, BTCES_EVENT_SYNC_CONNECTION_UPDATED events */
  btces_event_data_disc_comp_struct     disc_comp;    /**< BTCES_EVENT_DISCONNECTION_COMPLETE events */
  btces_event_data_mode_struct          mode;         /**< BTCES_EVENT_MODE_CHANGED events */
} btces_event_data_union;

/** The callback function used to report a BTC-ES event */

typedef void (btces_cb_type)
(
  btces_event_enum        event,        /**< The event */
  btces_event_data_union  *event_data,  /**< Data associated with the event (if any) */
  void                    *user_data    /**< Same value as when the callback was registered using btces_register() */
);

/* Define pointer types to btces_* APIs */
typedef BTCES_STATUS (*btces_register_ptr)
(
  btces_cb_type *event_cb_ptr,
  void          *user_data
);

typedef BTCES_STATUS (*btces_deregister_ptr)
(
  void  **user_data_ptr
);

typedef BTCES_STATUS (*btces_state_report_ptr)( void );

typedef BTCES_STATUS (*btces_wlan_chan_ptr)
(
  uint16 wlan_channels
);

/* Structure of BTC-ES function pointers (the BTC-ES Interface) */
typedef struct _btces_funcs
{
  btces_register_ptr      register_func;
  btces_deregister_ptr    deregister_func;
  btces_state_report_ptr  state_report_func;
  btces_wlan_chan_ptr     wlan_chan_func;
} btces_funcs;

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
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
FUNCTION:  btces_init
==============================================================*/
/**
  Initialize the BTC-ES module.

  This service is called to initialize BTC-ES. BTC-ES will initialize the platform
  layer, find out the initial power state of Bluetooth, begin doing HCI Traffic
  Analysis and processing platform events. No outgoing events from BTC-ES can be
  reported until a client has registered by calling btces_register(); BTC-ES starts
  up with no client registered.

  If BTC-ES is already running, BTCES_STATUS_ALREADY_INITIALIZED is returned;
  btces_deinit() must be called first if the intent is to re-initialize BTC-ES.
  If BTC-ES initialization fails for any other reason, BTC-ES will remain
  uninitialized.

  @see     btces_deinit

  @return  BTCES_OK: BTC-ES initialized successfully.
  @return  BTCES_STATUS_ALREADY_INITIALIZED: BTC-ES is already running; no change occurred.
*/
BTCES_STATUS btces_init( void );

/*==============================================================
FUNCTION:  btces_deinit
==============================================================*/
/**
  BTC-ES is told to un-initialize, and all HCI and Native event analysis is stopped.
  It does not matter if a client is still registered to receive BTC-ES output events.

  BTC-ES is uninitialized; all dynamically allocated resources are
  freed and all event reporting is halted.

  @see     btces_init

  @return  BTCES_OK: BTC-ES uninitialized successfully.
  @return  BTCES_STATUS_NOT_INITIALIZED: BTC-ES is not running; btces_init() must be called first.
*/
BTCES_STATUS btces_deinit( void );

/*==============================================================
FUNCTION:  btces_register
==============================================================*/
/**
  BTC-ES is given a callback function to report events, and BTC-ES uses it
  immediately to report events representing the current Bluetooth state.

  This service registers a callback to be used by BTC-ES to report events, and
  triggers a sequence of events to occur representing the current Bluetooth state.

  When BTC-ES reports an event, it calls the service pointed to by event_cb_ptr,
  supplying a pointer to an event structure containing the event and the associated
  data, and the original value of user_data. The receiver of this event must copy
  the data before returning.

  @see     btces_deregister

  @return  BTCES_OK: The registration occurred successfully.
  @return  BTCES_STATUS_INVALID_PARAMETERS: event_cb_ptr was NULL.
  @return  BTCES_STATUS_ALREADY_REGISTERED: BTC-ES already has a registered event callback.
  @return  BTCES_STATUS_NOT_INITIALIZED: BTC-ES is not running; btces_init() must be called first.
*/
BTCES_STATUS btces_register
(
  btces_cb_type *event_cb_ptr,
  /**< [in] Pointer to a service to be called by BTC-ES to report an event.
            The pointer cannot be NULL. */

  void *user_data
  /** [in] Opaque user-supplied data. This same value will always be passed to the
           callback service indicated by event_cb_ptr. */
);

/*==============================================================
FUNCTION:  btces_deregister
==============================================================*/
/**
  BTC-ES is told to stop reporting events.

  This service de-registers the callback that was given to btces_register(),
  and so BTC-ES will stop reporting events until that service is called again.

  @see     btces_register

  @return  BTCES_OK: The callback deregistration occurred successfully.
  @return  BTCES_STATUS_NOT_REGISTERED: BTC-ES does not have a registered event callback.
  @return  BTCES_STATUS_NOT_INITIALIZED: BTC-ES is not running; btces_init() must be called first.
*/
BTCES_STATUS btces_deregister
(
  void  **user_data_ptr
  /**< [out] Optional pointer to opaque user-supplied data that was given to
             btces_register(). If the pointer is NULL, the value is not output.
             If this service returns an error, the output value must be ignored. */
);

/*==============================================================
FUNCTION:  btces_state_report
==============================================================*/
/**
  BTC-ES outputs a series of events representing the current state of Bluetooth.

  This service causes BTC-ES to insert a series of events into the outgoing event
  stream. The event for the current Bluetooth power will occur first, and if Bluetooth
  is ON, then several more events can be generated.

  @return  BTCES_OK: The series of events was scheduled successfully.
  @return  BTCES_STATUS_NOT_REGISTERED: BTC-ES does not have a registered event callback, so no events were delivered.
  @return  BTCES_STATUS_NOT_INITIALIZED: BTC-ES is not running; btces_init() must be called first.
*/
BTCES_STATUS btces_state_report( void );

/*==============================================================
FUNCTION:  btces_wlan_chan
==============================================================*/
/**
  BTC-ES is told what channel(s) WLAN is currently using for AFH consideration.

  This service causes BTC-ES to save the set of channels to a static global;
  whenever BTC-ES detects Bluetooth in the "On" state, the channels are passed to
  the Bluetooth stack via btces_pfal_wlan_chan(). The channels are stored
  statically in case BTC-ES is not running.

  The initial state of the static global is 0x0000 (no channels used by WLAN),
  which is also equivalent to the internal state of the SoC after processing the
  HCI_Reset command (AFH Channel Map = "all Bluetooth channels available"). Thus,
  BTC-ES can detect whether the channel data has actually changed, and so can
  avoid unneeded calls to btces_pfal_wlan_chan().

  If WLAN is turned off by the user, this API must be called with 0x0000.

  @return BTCES_OK: The set of channels WLAN is using was accepted.
  @return BTCES_STATUS_INVALID_PARAMETERS: Invalid bit positions in wlan_channels were set to 1, so it was discarded.

  Note that the error BTCES_STATUS_NOT_INITIALIZED will not occur, as this
  service does not require BTC-ES to be initialized.
*/
BTCES_STATUS btces_wlan_chan
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

#ifdef __cplusplus
}
#endif

#endif /*_QCOM_BTCES_H_*/

