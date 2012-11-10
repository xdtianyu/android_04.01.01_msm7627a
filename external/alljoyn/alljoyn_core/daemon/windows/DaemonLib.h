/**
 * @file
 * DaemonLib.h
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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
#pragma once

#include <stdio.h>
#include <tchar.h>

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the DAEMONLIBRARY_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// DAEMONLIBRARY_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DAEMONLIBRARY_EXPORTS
#define DAEMONLIBRARY_API __declspec(dllexport)
#else
#define DAEMONLIBRARY_API __declspec(dllimport)
#endif

extern char g_logFilePathName[];
extern bool g_isManaged;

extern "C" DAEMONLIBRARY_API void DaemonMain(wchar_t* cmd);
extern "C" DAEMONLIBRARY_API void SetLogFile(wchar_t* str);
extern "C" DAEMONLIBRARY_API int LoadDaemon(int argc, char** argv);
extern "C" DAEMONLIBRARY_API void UnloadDaemon();
