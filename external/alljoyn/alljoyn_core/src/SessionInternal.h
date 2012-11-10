#ifndef _ALLJOYN_SESSIONINTERNAL_H
#define _ALLJOYN_SESSIONINTERNAL_H
/**
 * @file
 * AllJoyn session related data types.
 */

/******************************************************************************
 * Copyright 2009-2010, Qualcomm Innovation Center, Inc.
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
#include <alljoyn/Session.h>

/** DBus signature of SessionOpts structure */
#define SESSIONOPTS_SIG "a{sv}"

namespace ajn {

/**
 * Parse a MsgArg into a SessionOpts
 * @param       msgArg   MsgArg to be parsed.
 * @param[OUT]  opts     SessionOpts output.
 * @return  ER_OK if successful.
 */
QStatus GetSessionOpts(const MsgArg& msgArg, SessionOpts& opts);

/**
 * Write a SessionOpts into a MsgArg
 *
 * @param      opts      SessionOpts to be written to MsgArg.
 * @param[OUT] msgArg    MsgArg output.
 */
void SetSessionOpts(const SessionOpts& opts, MsgArg& msgArg);

}

#endif
