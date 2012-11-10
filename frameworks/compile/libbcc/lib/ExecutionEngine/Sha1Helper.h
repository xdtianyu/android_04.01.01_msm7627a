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

#ifndef BCC_SHA1HELPER_H
#define BCC_SHA1HELPER_H

#include "Config.h"

#include <stddef.h>

namespace bcc {
  extern unsigned char sha1LibBCC_SHA1[20];
  extern char const *pathLibBCC_SHA1;

  extern unsigned char sha1LibRS[20];
  extern char const *pathLibRS;

  void calcSHA1(unsigned char *result, char const *data, size_t size);

  void calcFileSHA1(unsigned char *result, char const *filename);

  // Read binary representation of sha1 from filename.
  void readSHA1(unsigned char *result, int resultsize, char const *filename);
}

#endif // BCC_SHA1HELPER_H
