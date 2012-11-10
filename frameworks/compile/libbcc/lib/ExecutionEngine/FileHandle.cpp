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

#include "FileHandle.h"

#include "DebugHelper.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string.h>

namespace bcc {

int FileHandle::open(char const *filename, OpenMode::ModeType mode) {
  static int const open_flags[2] = {
    O_RDONLY,
    O_RDWR | O_CREAT | O_TRUNC,
  };

  static int const lock_flags[2] = { LOCK_SH, LOCK_EX };

#if USE_LOGGER
  static char const *const open_mode_str[2] = { "read", "write" };
#endif

  static size_t const RETRY_MAX = 4;

  static useconds_t const RETRY_USEC = 200000UL;

  for (size_t i = 0; i < RETRY_MAX; ++i) {
    // Try to open the file
    mFD = ::open(filename, open_flags[mode], 0644);

    if (mFD < 0) {
      if (errno == EINTR) {
        // Interrupt occurs while opening the file.  Retry.
        continue;
      }

      ALOGW("Unable to open %s in %s mode.  (reason: %s)\n",
           filename, open_mode_str[mode], strerror(errno));

      return -1;
    }

    // Try to lock the file
    if (flock(mFD, lock_flags[mode] | LOCK_NB) < 0) {
      ALOGW("Unable to acquire the lock immediately, block and wait now ...\n");

      if (flock(mFD, lock_flags[mode]) < 0) {
        ALOGE("Unable to acquire the lock. Retry ...\n");

        ::close(mFD);
        mFD = -1;

        usleep(RETRY_USEC);
        continue;
      }
    }

    // Note: From now on, the object is correctly initialized.  We have to
    // use this->close() to close the file now.

    // Check rather we have locked the correct file or not
    struct stat sfd, sfname;

    if (fstat(mFD, &sfd) == -1 || stat(filename, &sfname) == -1 ||
        sfd.st_dev != sfname.st_dev || sfd.st_ino != sfname.st_ino) {
      // The file we locked is different from the given path.  This may
      // occur when someone changes the file node before we lock the file.
      // Just close the file, and retry after sleeping.

      this->close();
      usleep(RETRY_USEC);
      continue;
    }

    // Good, we have open and lock the file correctly.
    ALOGV("File opened. fd=%d\n", mFD);
    return mFD;
  }

  ALOGW("Unable to open %s in %s mode.\n", filename, open_mode_str[mode]);
  return -1;
}


void FileHandle::close() {
  if (mFD >= 0) {
    flock(mFD, LOCK_UN);
    ::close(mFD);
    ALOGV("File closed. fd=%d\n", mFD);
    mFD = -1;
  }
}


ssize_t FileHandle::read(char *buf, size_t count) {
  if (mFD < 0) {
    return -1;
  }

  while (true) {
    ssize_t nread = ::read(mFD, static_cast<void *>(buf), count);

    if (nread >= 0) {
      return nread;
    }

    if (errno != EAGAIN && errno != EINTR) {
      // If the errno is EAGAIN or EINTR, then we try to read again.
      // Otherwise, consider this is a failure.  And returns zero.
      return -1;
    }
  }

  // Unreachable
  return -1;
}


ssize_t FileHandle::write(char const *buf, size_t count) {
  if (mFD < 0) {
    return -1;
  }

  ssize_t written = 0;

  while (count > 0) {
    ssize_t nwrite = ::write(mFD, static_cast<void const *>(buf), count);

    if (nwrite < 0) {
      if (errno != EAGAIN && errno != EINTR) {
        return written;
      }

      continue;
    }

    written += nwrite;
    count -= (size_t)nwrite;
    buf += (size_t)nwrite;
  }

  return written;
}


off_t FileHandle::seek(off_t offset, int whence) {
  return (mFD < 0) ? -1 : lseek(mFD, offset, whence);
}


void FileHandle::truncate() {
  if (mFD >= 0) {
    if (ftruncate(mFD, 0) != 0) {
      ALOGE("Unable to truncate the file.\n");
    }
  }
}


} // namespace bcc
