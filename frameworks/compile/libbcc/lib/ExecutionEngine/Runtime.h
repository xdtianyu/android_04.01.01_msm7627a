/*
 * Copyright 2010, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BCC_RUNTIME_H
#define BCC_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

void *FindRuntimeFunction(const char *Name);
void VerifyRuntimesTable();

#ifdef __cplusplus
};
#endif

#endif  /* BCC_RUNTIME_H */
