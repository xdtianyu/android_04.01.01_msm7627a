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
#ifndef _BTCES_TYPES_H_
#define _BTCES_TYPES_H_

/*------------------------------------------------------------------------------
                 BTC-ES Bluetooth Coexistence Event Source
------------------------------------------------------------------------------*/

/**
  @file btces_types.h

  This file provides a mapping between platform-specific types and the BTC-ES
  platform-independent types. It also supplies a few definitions common to BTC-ES
  clients and to the hosting platform calling into BTC-ES.
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

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

typedef signed char     int8;
typedef signed short    int16;
typedef signed long     int32;
typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned long   uint32;
typedef int             boolean;


/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Enumerated types
 * -------------------------------------------------------------------------*/
/** BTC-ES API and BTC-ES PFAL API failure codes */
typedef enum
{
  BTCES_OK                            = 0,    /**< The service was successful, alias for BTCES_SUCCESS */
  BTCES_SUCCESS                       = 0,    /**< The service was successful, alias for BTCES_OK */
  BTCES_FAIL                          = 101,  /**< The service was unsuccessful */
  BTCES_STATUS_OUT_OF_MEMORY          = 102,  /**< There was not enough memory to complete the operation. */
  BTCES_STATUS_NOT_IMPLEMENTED        = 103,  /**< The requested operation has not been implemented. */
  BTCES_STATUS_NOT_INITIALIZED        = 104,  /**< The subsystem is not ready to perform operations. */
  BTCES_STATUS_INITIALIZATION_FAILED  = 105,  /**< The request failed due to an initialization problem. */
  BTCES_STATUS_INVALID_PARAMETERS     = 106,  /**< The service was given one or more invalid parameters. */
  BTCES_STATUS_INTERNAL_ERROR         = 107,  /**< The service request found an internal inconsistency. */
  BTCES_STATUS_INVALID_STATE          = 108,  /**< The state of the system does not allow the requested operation. */
  BTCES_STATUS_ALREADY_REGISTERED     = 109,  /**< Registration has already been performed. */
  BTCES_STATUS_NOT_REGISTERED         = 110,  /**< Registration has not yet been performed. */
  BTCES_STATUS_ALREADY_INITIALIZED    = 111,  /**< The subsystem was already initialized, so no action occurred. */
} BTCES_STATUS;


/*----------------------------------------------------------------------------
 * Structure definitions
 * -------------------------------------------------------------------------*/

/**
  Bluetooth Device Address

  The device address is defined as an array of bytes.
  The array is big-endian, meaning that
  - addr[0] contains bits 47-40,
  - addr[1] contains bits 39-32,
  - addr[2] contains bits 31-24,
  - addr[3] contains bits 23-16,
  - addr[4] contains bits 15-8, and
  - addr[5] contains bits 7-0.
*/
typedef struct
{
  uint8 addr[6];        /**< The address is in this byte array. */
} btces_bt_addr_struct;


/*----------------------------------------------------------------------------
 * Macros
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Constant values
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function declarations
 * -------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /*_BTCES_TYPES_H_*/

