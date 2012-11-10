/** @addtogroup MCD_IMPL_LIB
 * @{
 * @file
 *
 * Client library device management.
 *
 * Device and Trustlet Session management Funtions.
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
#include <stdint.h>
#include <vector>

#include "mcDrvModuleApi.h"

#include "Device.h"

#define LOG_TAG	"McClientLib_Device"
#include "log.h"


//------------------------------------------------------------------------------
Device::Device(
    uint32_t    deviceId,
    Connection  *connection
) {
	this->deviceId = deviceId;
	this->connection = connection;

	pMcKMod = new CMcKMod();
}


//------------------------------------------------------------------------------
Device::~Device(
    void
) {
	/* Delete all session objects. Usually this should not be needed as closeDevice()
	 * requires that all sessions have been closed before.
	 */
	sessionIterator_t  sessionIterator = sessionList.begin();
	while(sessionIterator != sessionList.end())
	{
		delete (*sessionIterator);
		sessionIterator = sessionList.erase(sessionIterator);
	}

	// Free all allocated WSM descriptors
	wsmIterator_t  wsmIterator = wsmL2List.begin();
	while(wsmIterator != wsmL2List.end())
	{
	    CWsm_ptr pWsm = *wsmIterator;

	    // ignore return code
	    pMcKMod->free(pWsm->handle);

	    delete (*wsmIterator);
		wsmIterator = wsmL2List.erase(wsmIterator);
	}
	delete connection;
	delete pMcKMod;
}


//------------------------------------------------------------------------------
bool Device::open(
    const char * deviceName
) {
	return pMcKMod->open(deviceName);
}


//------------------------------------------------------------------------------
void Device::close(
    void
) {
	pMcKMod->close();
}


//------------------------------------------------------------------------------
bool Device::hasSessions(
    void
) {
	return sessionList.size() > 0;
}


//------------------------------------------------------------------------------
void Device::createNewSession(
    uint32_t    sessionId,
    Connection  *connection
) {
	Session *session = new Session(sessionId, pMcKMod, connection);
	sessionList.push_back(session);
}


//------------------------------------------------------------------------------
bool Device::removeSession(
    uint32_t sessionId
) {
	bool ret = false;

	sessionIterator_t interator = sessionList.begin();
	while(interator != sessionList.end())
	{
		if ((*interator)->sessionId == sessionId)
		{
			delete (*interator);
			interator = sessionList.erase(interator);
			ret = true;
			break;
		}
		else
		{
		    interator++;
		}
	}
	return ret;
}


//------------------------------------------------------------------------------
Session *Device::resolveSessionId(
    uint32_t sessionId
) {
	Session  *ret = NULL;

	// Get Session for sessionId
	for ( sessionIterator_t interator = sessionList.begin();
	      interator != sessionList.end();
	      ++interator)
    {
		if ((*interator)->sessionId == sessionId) {
			ret = (*interator);
			break;
		}
	}
	return ret;
}


//------------------------------------------------------------------------------
CWsm_ptr Device::allocateContiguousWsm(
    uint32_t len
) {
    CWsm_ptr  pWsm = NULL;
	do
	{
	    if (0 == len)
	    {
		    break;
	    }
	    
		// Allocate shared memory
		addr_t    virtAddr;
		uint32_t  handle;
		addr_t    physAddr;
		bool      mciReuse = false;
		int ret = pMcKMod->mmap(
							len,
							&handle,
							&virtAddr,
							&physAddr,
							&mciReuse);
		if (0 != ret)
		{
			break;
		}

		// Register (vaddr,paddr) with device
		pWsm = new CWsm(virtAddr,len,handle,physAddr);

		wsmL2List.push_back(pWsm);
		
	} while(0);

	// Return pointer to the allocated memory
	return pWsm;
}


//------------------------------------------------------------------------------
bool Device::freeContiguousWsm(
    CWsm_ptr  pWsm
) {
	bool ret = false;
	wsmIterator_t iterator;

	for (iterator=wsmL2List.begin(); iterator!=wsmL2List.end(); ++iterator)
	{
	    if (pWsm == *iterator)
	    {
	        ret = true;
	        break;
	    }
	}

	if(ret) {
		LOG_I("freeWsm virtAddr=0x%p, handle=%d",
				pWsm->virtAddr,pWsm->handle);

		// ignore return code
		pMcKMod->free(pWsm->handle);

		iterator = wsmL2List.erase(iterator);
		delete pWsm;
	}
	return ret;
}


//------------------------------------------------------------------------------
CWsm_ptr Device::findContiguousWsm(
    addr_t  virtAddr
) {
    CWsm_ptr pWsm = NULL;

	for( wsmIterator_t iterator=wsmL2List.begin();
	     iterator!=wsmL2List.end();
	     ++iterator)
	{
		CWsm_ptr pTmpWsm = *iterator;
	    if (virtAddr == pTmpWsm->virtAddr)
		{
			pWsm = pTmpWsm;
			break;
		}
	}

	return pWsm;
}

/** @} */
