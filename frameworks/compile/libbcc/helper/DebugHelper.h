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

#ifndef BCC_DEBUGHELPER_H
#define BCC_DEBUGHELPER_H

#include "Config.h"

#if USE_LOGGER

#define LOG_TAG "bcc"
#include <cutils/log.h>

#else // !USE_LOGGER

#undef ALOGV
#undef ALOGI
#undef ALOGD
#undef ALOGW
#undef ALOGE
#undef LOGA

#define ALOGV(...)
#define ALOGI(...)
#define ALOGD(...)
#define ALOGW(...)
#define ALOGE(...)
#define LOGA(...)

#endif


#if !USE_FUNC_LOGGER

#define BCC_FUNC_LOGGER()

#else // USE_FUNC_LOGGER

#if defined(__cplusplus)
namespace bcc {
  class FuncLogger {
  private:
    char const *mFuncName;

  public:
    FuncLogger(char const *name) : mFuncName(name) {
      ALOGD("---> BEGIN: libbcc [ %s ]\n", name);
    }

    ~FuncLogger() {
      ALOGD("---> END: libbcc [ %s ]\n", mFuncName);
    }
  };
} // namespace bcc

#define BCC_FUNC_LOGGER() bcc::FuncLogger XX__FuncLogger(__func__)
#endif

#endif

#endif // BCC_DEBUGHELPER_H
