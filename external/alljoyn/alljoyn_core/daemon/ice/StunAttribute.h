#ifndef _STUNATTRIBUTE_H
#define _STUNATTRIBUTE_H
/**
 * @file
 *
 * This file is a convenient wrapper for including all the STUN Message
 * Attribute header files..
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

#ifndef __cplusplus
#error Only include StunAttribute.h in C++ code.
#endif

#include <StunAttributeAlternateServer.h>
#include <StunAttributeBase.h>
#include <StunAttributeChannelNumber.h>
#include <StunAttributeData.h>
#include <StunAttributeDontFragment.h>
#include <StunAttributeErrorCode.h>
#include <StunAttributeEvenPort.h>
#include <StunAttributeFingerprint.h>
#include <StunAttributeIceControlled.h>
#include <StunAttributeIceControlling.h>
#include <StunAttributeLifetime.h>
#include <StunAttributeMappedAddress.h>
#include <StunAttributeMessageIntegrity.h>
#include <StunAttributePriority.h>
#include <StunAttributeRequestedTransport.h>
#include <StunAttributeReservationToken.h>
#include <StunAttributeSoftware.h>
#include <StunAttributeUnknownAttributes.h>
#include <StunAttributeUseCandidate.h>
#include <StunAttributeUsername.h>
#include <StunAttributeXorMappedAddress.h>
#include <StunAttributeXorPeerAddress.h>
#include <StunAttributeXorRelayedAddress.h>

#endif
