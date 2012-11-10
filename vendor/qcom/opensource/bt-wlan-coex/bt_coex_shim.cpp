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
/**
  @file bt_coex_shim.cpp

  Bluetooth Coexistence Shim: BTC-ES to BTC glue for Android+BlueZ

  This file implements the services that bring up or shut down the coexistence
  solution. For the Android+BlueZ, it includes a simple open and close
  implementaion that triggers the rest of the BTCES initialization/teardown.
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

#include "bt_coex_shim.h"
#include "btces.h"
#include "btces_svc.h"
#include "btces_plat.h"

#include <wlan_btc_usr_svc.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/* Comment this out to remove local test code and connect to the "real" BTC */
//#define BTCES_LOCAL_DEBUG


/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Debug functionality section - derived from BM3 implementation
 * -------------------------------------------------------------------------*/


#ifdef BTCES_LOCAL_DEBUG

/* Replace these calls in the shim code below with test code */
#define btc_svc_init _btc_svc_init
#define btc_svc_deinit _btc_svc_deinit

/* For AFH tests */
#define BTCES_WLAN_CHAN_TEST (1 << 6)   /* WLAN Channel 6 only */
static uint16 g_wlan_test_chans = 0;  /* Start with no WLAN channel(s) in use */

static void btc_services_callback
(
  btces_event_enum        event,        /**< The event */
  btces_event_data_union  *event_data,  /**< Data associated with the event (if any) */
  void                    *user_data    /**< Same value as when the callback was registered using btces_register() */
)
{
  switch( event )
  {
    case BTCES_EVENT_DEVICE_SWITCHED_ON:       /**< No event_data */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_DEVICE_SWITCHED_ON)" BTCES_EOL );
      break;
    case BTCES_EVENT_DEVICE_SWITCHED_OFF:      /**< No event_data */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_DEVICE_SWITCHED_OFF)" BTCES_EOL );
      break;
    case BTCES_EVENT_INQUIRY_STARTED:          /**< No event_data */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_INQUIRY_STARTED)" BTCES_EOL );
      break;
    case BTCES_EVENT_INQUIRY_STOPPED:          /**< No event_data */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_INQUIRY_STOPPED)" BTCES_EOL );
      break;
    case BTCES_EVENT_PAGE_STARTED:             /**< No event_data */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_PAGE_STARTED)" BTCES_EOL );
      break;
    case BTCES_EVENT_PAGE_STOPPED:             /**< No event_data */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_PAGE_STOPPED)" BTCES_EOL );
      break;
    case BTCES_EVENT_CREATE_ACL_CONNECTION:    /**< See btces_bt_addr_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_CREATE_ACL_CONNECTION)" BTCES_EOL );
      break;
    case BTCES_EVENT_ACL_CONNECTION_COMPLETE:  /**< See btces_event_data_acl_comp_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_ACL_CONNECTION_COMPLETE)" BTCES_EOL );
      break;
    case BTCES_EVENT_CREATE_SYNC_CONNECTION:   /**< See btces_bt_addr_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_CREATE_SYNC_CONNECTION)" BTCES_EOL );
      break;
    case BTCES_EVENT_SYNC_CONNECTION_COMPLETE: /**< See btces_event_data_sync_comp_up_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_SYNC_CONNECTION_COMPLETE)" BTCES_EOL );
      break;
    case BTCES_EVENT_SYNC_CONNECTION_UPDATED:  /**< See btces_event_data_sync_comp_up_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_SYNC_CONNECTION_UPDATED)" BTCES_EOL );
      break;
    case BTCES_EVENT_DISCONNECTION_COMPLETE:   /**< See btces_event_data_disc_comp_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_DISCONNECTION_COMPLETE)" BTCES_EOL );

      /* Whenever a disconnect event is announced, toggle WLAN channel activity for AFH */
      BTCES_MSG_LOW("setting WLAN Channel to 0x%04X" BTCES_EOL, g_wlan_test_chans);
      btces_wlan_chan( g_wlan_test_chans );
      g_wlan_test_chans = ((g_wlan_test_chans) ? 0 : BTCES_WLAN_CHAN_TEST);
      break;
    case BTCES_EVENT_MODE_CHANGED:             /**< See btces_event_data_mode_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_MODE_CHANGED)" BTCES_EOL );
      break;
    case BTCES_EVENT_A2DP_STREAM_START:        /**< See btces_bt_addr_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_A2DP_STREAM_START)" BTCES_EOL );
      break;
    case BTCES_EVENT_A2DP_STREAM_STOP:         /**< See btces_bt_addr_struct */
      BTCES_MSG_LOW("btc_services_callback(BTCES_EVENT_A2DP_STREAM_STOP)" BTCES_EOL );
      break;
    default:
      BTCES_MSG_LOW("btc_services_callback(unknown event!!)" BTCES_EOL );
      break;
  }
} /* btc_services_callback */

/*==============================================================
FUNCTION:  _btc_svc_init
==============================================================*/
static int _btc_svc_init( btces_funcs *funcs_ptr )
{
  /* Register with BTC-ES */
  BTCES_STATUS status = funcs_ptr->register_func( btc_services_callback, NULL);

  BTCES_MSG_LOW( "BTC Services test code: btces_register() returned %d" BTCES_EOL, status );

  return (int) status;
} /* btc_svc_init */

/*==============================================================
FUNCTION:  _btc_svc_deinit
==============================================================*/
static void _btc_svc_deinit
(
  void
)
{
  /* De-register with BTC-ES */
  BTCES_STATUS status = btces_deregister( NULL );

  BTCES_MSG_LOW( "BTC Services test code: btces_deregister() returned %d" BTCES_EOL, status );
}

#endif /* BTCES_LOCAL_DEBUG */


/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/

/*==============================================================
FUNCTION:  bt_coex_shim_open()
==============================================================*/
/** BT Coexistence is started. See bt_coex_shim.h for details. */

int bt_coex_shim_open( void )
{
  BTCES_STATUS  ret_val;
  int btc_ret_val;

  btces_funcs funcs_data =
  {
    btces_register,
    btces_deregister,
    btces_state_report,
    btces_wlan_chan
  }; /**< A structure of the BTC-ES APIs, it will be copied by BTC */

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ret_val = btces_init();

  if ( ret_val == BTCES_OK )
  {
    /* Initialize BTC, passing in the set of BTC-ES APIs */
    btc_ret_val = btc_svc_init( &funcs_data );

    if ( btc_ret_val )
    {
      BTCES_MSG_ERROR( "bt_coex_shim_open(): btc_svc_init() Failed: %d" BTCES_EOL, btc_ret_val );

      /* Shut BTC-ES back down, don't care about the return value */
      (void) btces_deinit();
      ret_val = BTCES_FAIL;
    }
  }

  return ( (ret_val == BTCES_OK) ? 0 : -1 );
}

/*==============================================================
FUNCTION:  bt_coex_shim_close()
==============================================================*/
/** BT Coexistence is closed. See bt_coex_shim.h for details. */

void bt_coex_shim_close( void )
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Shut down BTC, it will de-register itself from BTC-ES */
  btc_svc_deinit();

  /* Shut down BTC-ES, don't care about the return value */
  (void) btces_deinit();
}

