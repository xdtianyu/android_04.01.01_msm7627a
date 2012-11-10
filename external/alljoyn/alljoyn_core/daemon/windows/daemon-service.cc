/**
 * @file
 * daemon-service - Wrapper to allow daemon to be built as a DLL
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

#include <qcc/platform.h>

#define DAEMONLIBRARY_EXPORTS
#include "DaemonLib.h"

// Standard Windows DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved
                      )
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// Global buffer to hold log file path when started as a Windows Service
char g_logFilePathName[MAX_PATH];
bool g_isManaged = false;

// forward reference

bool IsWhiteSpace(char c)
{
    if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'))
        return true;
    return false;
}

DAEMONLIBRARY_API void DaemonMain(wchar_t* cmd)
{
    if (!cmd || !*cmd || wcslen(cmd) >= 2000) {         // make sure it fits
        printf("Bad command string\n");
        return;
    }
    char cmdLine[2000];     // on the stack
    char* tempPtrs[20];     // arbitrary limit of 20!

    sprintf_s(cmdLine, 2000, "%S", cmd);
    char* src = cmdLine;

    int i = 0;   // count the arguments
    char workingBuffer[MAX_PATH];     // on the stack
    int cnt = (int)strlen(cmdLine);
    while (cnt > 0) {
        char* dest = workingBuffer;
        while (*src && !IsWhiteSpace(*src)) {
            *dest++ = *src++;
            cnt--;
        }
        *dest = (char)0;         // terminate current arg
        size_t len = strlen(workingBuffer) + 1;
        tempPtrs[i] = new char[len];
        memcpy((void*)tempPtrs[i], (const void*)workingBuffer, len);
        while (*src && IsWhiteSpace(*src)) {       // skip white space
            src++;
            cnt--;
        }
        i++;
    }
    if (!i) {
        printf("Empty command string\n");
        return;
    }
    // now create argc and argv
    char** argv = new char*[i];
    for (int ii = 0; ii < i; ii++)
        argv[ii] = tempPtrs[ii];
    LoadDaemon(i, argv);
    for (int ii = 0; ii < i; ii++)
        delete[] argv[ii];
    delete[] argv;
}

DAEMONLIBRARY_API void SetLogFile(wchar_t* str)
{
    sprintf_s(g_logFilePathName, MAX_PATH, "%S", str);
    g_isManaged = true;
}