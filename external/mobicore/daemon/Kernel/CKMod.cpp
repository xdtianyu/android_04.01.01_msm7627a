/** @addtogroup MCD_MCDIMPL_DAEMON_KERNEL
 * @{
 * @file
 *
 * Kernel Module Interface.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009 - 2012 -->
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <cstdlib>

#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "CKMod.h"

#define LOG_TAG	"McDaemon"
#include "log.h"


//------------------------------------------------------------------------------
CKMod::CKMod(
    void
) {
	fdKMod = ERROR_KMOD_NOT_OPEN;
}


//------------------------------------------------------------------------------
CKMod::~CKMod(
    void
) {
	close();
}


//------------------------------------------------------------------------------
bool CKMod::isOpen(
    void
) {
	return (ERROR_KMOD_NOT_OPEN == fdKMod) ? false : true;
}


//------------------------------------------------------------------------------
bool CKMod::open(
    const char *deviceName
) {
	bool ret = true;

	do
	{
		if (isOpen())
		{
			LOG_W("already open");
			ret = false;
			break;
		}

		// open return -1 on error, "errno" is set with details
		int openRet = ::open(deviceName, O_RDWR);
		if (-1 == openRet)
		{
			LOG_E("open failed with errno: %d", errno);
			ret = false;
			break;
		}

		fdKMod = openRet;

	} while(0);

	return ret;
}


//------------------------------------------------------------------------------
void CKMod::close(
    void
) {
	if (isOpen())
	{
		if (0 != ::close(fdKMod))
		{
			LOG_E("close failed with errno: %d", errno);
		}
		else
		{
			fdKMod = ERROR_KMOD_NOT_OPEN;
		}
	}
	else
	{
		LOG_W("not open");
	}
}

/** @} */
