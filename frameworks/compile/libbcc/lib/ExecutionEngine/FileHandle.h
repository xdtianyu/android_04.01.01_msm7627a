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

#ifndef BCC_FILEHANDLE_H
#define BCC_FILEHANDLE_H

#include <sys/types.h>

#include <stddef.h>
#include <stdint.h>

namespace bcc {
  namespace OpenMode {
    enum ModeType {
      Read = 0,
      Write = 1,
    };
  }

  class FileHandle {
  private:
    int mFD;

  public:
    FileHandle() : mFD(-1) {
    }

    ~FileHandle() {
      if (mFD >= 0) {
        close();
      }
    }

    int open(char const *filename, OpenMode::ModeType mode);

    void close();

    int getFD() {
      // Note: This function is designed not being qualified by const.
      // Because once the file descriptor is given, the user can do every
      // thing on file descriptor.

      return mFD;
    }

    off_t seek(off_t offset, int whence);

    ssize_t read(char *buf, size_t count);

    ssize_t write(char const *buf, size_t count);

    void truncate();

  };

} // namespace bcc

#endif // BCC_FILEHANDLE_H
