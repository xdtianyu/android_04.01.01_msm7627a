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
  @file wlan_btc_usr_svc.h

  Exports and types for the BTC Service interface. This is shared between
  BTC-ES layer and BTC Services layer.

  This implementation is specific to Android platform and works with BlueZ
  Bluetooth Stack.
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

#ifndef WLAN_BTC_USR_SVC_H__
#define WLAN_BTC_USR_SVC_H__

#include <btces.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 * External Functions
 *-------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *-------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Type Declarations
 *-------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Function Prototypes
 *-------------------------------------------------------------------------*/
int btc_svc_init (btces_funcs *pBtcEsFuncs);
void btc_svc_deinit (void);

#ifdef __cplusplus
}
#endif

#endif //WLAN_BTC_USR_SVC_H__

