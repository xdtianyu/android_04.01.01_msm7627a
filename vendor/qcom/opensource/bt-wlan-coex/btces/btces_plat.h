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
#ifndef _BTCES_PLAT_H_
#define _BTCES_PLAT_H_

/*------------------------------------------------------------------------------
                BTC-ES OS PLATFORM HEADER FOR ANDROID
------------------------------------------------------------------------------*/

/**
  @file btces_plat.h

  The BTC-ES Interface for providing Platform OS abstraction required by BTC-ES
  for its operation. This file is for mapping platform-specific types and macros
  into platform-independent ones. Anything that can be done without creating a
  PFAL function should go here.
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

/* Android debug output setup, must be before log.h inclusion */
#ifdef BTCES_DEBUG

// Comment out LOG_NDEBUG define line to disable all logging
// Set others to 1 to disable, 0 to enable (LOG_NDDEBUG: debug logs, LOG_NIDEBUG: info logs)
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0

#define LOG_TAG "BT_COEX"
#include "cutils/log.h"
#endif

#include <string.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/* Android LOG* functions don't require EOL characters */
#define BTCES_EOL " "    /**< Platform End Of Line sequence; non-empty to avoid a compile warning */

/* Simple way to switch these to standard string.h definitions */
#define std_memcmp  memcmp
#define std_memmove memmove
#define std_memset  memset

#ifndef TRUE
#define TRUE  (1)
#define FALSE (0)
#endif

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

#if defined(BTCES_DEBUG)

/** BTCES_MSG_*() macros accept a printf-style expression. Pass on to Android debug functions. */
#define BTCES_MSG_FATAL(...)  LOG_FATAL(__VA_ARGS__)
#define BTCES_MSG_ERROR(...)  LOGE(__VA_ARGS__)
#define BTCES_MSG_HIGH(...)   LOGE(__VA_ARGS__)
#define BTCES_MSG_MEDIUM(...) LOGE(__VA_ARGS__)
#define BTCES_MSG_LOW(...)    LOGI(__VA_ARGS__)

/** The macro BTCES_ASSERT(condition) logs a generic assertion failure message if
    condition evaluates to zero (i.e., is false). */

#define BTCES_ASSERT(c) LOGE_IF(!(c), "BTCES: Assert Fail: "__FILE__":%d, cause: "#c BTCES_EOL, __LINE__)

#else /* !BTCES_DEBUG */

#define BTCES_MSG_FATAL(...)
#define BTCES_MSG_ERROR(...)
#define BTCES_MSG_HIGH(...)
#define BTCES_MSG_MEDIUM(...)
#define BTCES_MSG_LOW(...)

#define BTCES_ASSERT(c)

#endif /* BTCES_DEBUG */


/*----------------------------------------------------------------------------
 * Constant values
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function declarations
 * -------------------------------------------------------------------------*/

/* DO NOT PLACE FUNCTION DECLARATIONS IN THIS FILE.  USE BTCES_PFAL.H INSTEAD. */

#endif /* _BTCES_PLAT_H_ */

