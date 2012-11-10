/* This file is auto-generated.  Do not modify. */
/**
 * @file
 * This file provides access to Alljoyn library version and build information.
 */

/******************************************************************************
 * Copyright 2010-2012, Qualcomm Innovation Center, Inc.
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

#include "alljoyn/version.h"

static const char product[] = "Alljoyn Library";
static const unsigned int architecture = 2;
static const unsigned int apiLevel = 5;
static const unsigned int release = 1;


static const char version[] = "v2.5.1";
static const char build[] = "Alljoyn Library v2.5.1 (Built Fri Aug 03 00:13:14 UTC 2012 on sea-ajnu1010-b by seabuild - Git branch: '(no branch)' tag: 'R02.05.01c1' (+0 changes) commit ref: c8016cee4ba361d2466cfd679d9becf280a7bfbd)";

const char * ajn::GetVersion()
{
    return version;
}

const char * ajn::GetBuildInfo()
{
    return build;
}

uint32_t ajn::GetNumericVersion()
{
    return GenerateVersionValue(architecture, apiLevel, release);
}
