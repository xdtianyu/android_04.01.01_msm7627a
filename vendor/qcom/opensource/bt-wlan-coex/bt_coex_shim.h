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
#ifndef _BT_COEX_SHIM_H_
#define _BT_COEX_SHIM_H_

/**
  @file bt_coex_shim.h

  Bluetooth Coexistence Shim

  This file defines the services to start up or shut down the Bluetooth
  Coexistence solution for Qualcomm's WLAN. The services should be called when
  Bluetooth is initialized or shut down.
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

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

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
FUNCTION:  bt_coex_shim_open()
==============================================================*/
/**
  Open the Bluetooth Coexistence solution.

  This function should be called when Bluetooth is initialized. The Coexistence
  solution is based on HCI traffic monitoring and collecting platform events.
  Therefore, this API should be called when HCI communication with the Bluetooth
  SoC is ready; however, the coexistence solution does not need to monitor
  vendor-specific HCI traffic typically associated with controller initialization.

  @return  0: BT Coexistence initialized successfully.
  @return  non-zero: Error (e.g., coexistence is already running, or other error)
*/
int bt_coex_shim_open( void );

/*==============================================================
FUNCTION:  bt_coex_shim_close()
==============================================================*/
/**
  Close the Bluetooth Coexistence solution.

  This API should be called when Bluetooth is shut down. Bluetooth power On / Off
  events are be reported by another means.
*/
void bt_coex_shim_close( void );

#ifdef __cplusplus
}
#endif

#endif /* _BT_COEX_SHIM_H_ */
