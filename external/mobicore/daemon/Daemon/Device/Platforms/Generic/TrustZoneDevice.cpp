/** @addtogroup MCD_MCDIMPL_DAEMON_DEV
 * @{
 * @file
 *
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
#ifndef REDUCED_STLPORT
#include <fstream>
#else
#include <stdio.h>
#endif
#include <inttypes.h>
#include <list>

#include "McTypes.h"
#include "mcDrvModuleApi.h"
#include "Mci/mci.h"
#include "mcVersionHelper.h"

#include "CSemaphore.h"
#include "CMcKMod.h"

#include "MobiCoreDevice.h"
#include "TrustZoneDevice.h"
#include "NotificationQueue.h"

#define LOG_TAG	"McDaemon"
#include "log.h"


#define NQ_NUM_ELEMS      (16)
#define NQ_BUFFER_SIZE    (2 * (sizeof(notificationQueueHeader_t)+  NQ_NUM_ELEMS * sizeof(notification_t)))
#define MCP_BUFFER_SIZE   (sizeof(mcpBuffer_t))
#define MCI_BUFFER_SIZE   (NQ_BUFFER_SIZE + MCP_BUFFER_SIZE)

//------------------------------------------------------------------------------
MC_CHECK_VERSION(MCI,0,2);

//------------------------------------------------------------------------------
__attribute__ ((weak)) MobiCoreDevice* getDeviceInstance(
	void
) {
	return new TrustZoneDevice();
}


//------------------------------------------------------------------------------
TrustZoneDevice::TrustZoneDevice(
    void
) {
    // nothing to do
}


//------------------------------------------------------------------------------
TrustZoneDevice::~TrustZoneDevice(
    void
) {
    delete pMcKMod;
	delete pWsmMcp;
    delete nq;
}


//------------------------------------------------------------------------------
static int loadMobiCoreImage(
	addr_t	virtAddr,
	int32_t size,
	const char*	mobicorePath
) {

    LOG_I("MobiCore path: %s", mobicorePath);

    int ret = 1;

    do {
#ifndef REDUCED_STLPORT
        // Open MobiCore binary for reading only
        fstream fs(mobicorePath, ios_base::in | ios_base::binary);
        if (!fs) {
            LOG_E("MobiCore not found: %s", mobicorePath);
            break;
        }

        // Get the MobiCore file size
        fs.seekg(0, ios::end);
        int32_t fileSize = fs.tellg();
        fs.seekg(0, ios::beg);
        LOG_I("File size: %i", fileSize);
        // Check if file is too big
        if (fileSize > size) {
            LOG_E("MobiCore size exceeds expectations. Size is: %i", fileSize);
            break;
        }

        fs.read((char*)virtAddr, fileSize);

        //Create an visible line with different content at the end
        memset((void*)((uint32_t)virtAddr+fileSize),0xff,4096);

        // Close file
        fs.close();
        ret = 0;
#else
        // Open MobiCore binary for reading only
        FILE *fs = fopen (mobicorePath, "rb");
        if(!fs) {
            LOG_E("MobiCore not found: %s", mobicorePath);
            break;
        }

        // Get the MobiCore file size
        fseek(fs, 0, SEEK_END);
        int32_t fileSize = ftell(fs);
        fseek(fs, 0, SEEK_SET);
        LOG_I("File size: %i", fileSize);
        // Check if file is too big
        if (fileSize > size) {
            LOG_E("MobiCore size exceeds expectations. Size is: %i", fileSize);
            fclose(fs);
            break;
        }

        fread((char*)virtAddr, 1, fileSize, fs);

        //Create an visible line with different content at the end
        memset((void*)((uint32_t)virtAddr+fileSize),0xff,4096);

        // Close file
        fclose(fs);
        ret = 0;
#endif
    } while (false);

    return ret;
}


#define SIZE_DDRAM              (256 * 1024)
#define MOBICORE_BINARY_PATH    "/data/app/mobicore.img"
//------------------------------------------------------------------------------
/**
 * Set up MCI and wait till MC is initialized
 * @return true if mobicore is already initialized
 */
bool TrustZoneDevice::initDevice(
	const char	*devFile,
	bool		loadMobiCore,
	const char  *mobicoreImage,
	bool		enableScheduler
) throw (ExcDevice) {

    notificationQueue_t* nqStartOut;
	notificationQueue_t* nqStartIn;
	addr_t mciBuffer;

    pMcKMod = new CMcKMod();
    if (!pMcKMod->open(devFile))
    {
        LOG_E("open() kernel module device failed");
        return false;
    }
    if (!pMcKMod->checkKmodVersionOk())
    {
        LOG_E("kernel module version mismatch");
        return false;
    }

    // Start MobiCore from DDRAM
    if (loadMobiCore) {
        // 1. Allocate DDRAM as pseudo IRAM
        mobicoreInDDR = allocateContiguousPersistentWsm(SIZE_DDRAM);
        if (NULL == mobicoreInDDR) {
            LOG_E("Allocation of additional RAM failed");
            return false;
        }
        memset(mobicoreInDDR->virtAddr,0xCC,SIZE_DDRAM);

        int ret = loadMobiCoreImage(mobicoreInDDR->virtAddr, SIZE_DDRAM,
					mobicoreImage);
        if (0 != ret) {
            LOG_E("loading Mobicore file failed: %d", ret);
            return false;
        }

        ret = pMcKMod->fcExecute(
                mobicoreInDDR->physAddr,
                MCP_BUFFER_SIZE);
        if (0 != ret) {
            LOG_E("pMcKMod->fcExecute() failed : %d", ret);
            return false;
        }
    }
    this->schedulerEnabled = enableScheduler;

    // Init MC with NQ and MCP buffer addresses

	// Set up MCI buffer
	if(!getMciInstance(MCI_BUFFER_SIZE, &pWsmMcp, &mciReused)) {
		return false;
	}
	mciBuffer = pWsmMcp->virtAddr;

	if(!checkMciVersion()) {
		return false;
	}

	// Only do a fastcall if MCI has not been reused (MC already initialized)
	if (!mciReused)
	{
		// Wipe memory before first usage
		bzero(mciBuffer, MCI_BUFFER_SIZE);

		// Init MC with NQ and MCP buffer addresses
		int ret = pMcKMod->fcInit(
							pWsmMcp->physAddr,
							0,
							NQ_BUFFER_SIZE,
							NQ_BUFFER_SIZE,
							MCP_BUFFER_SIZE);
		if (0 != ret)
		{
			LOG_E("pMcKMod->fcInit() failed");
			return false;
		}

		// First empty N-SIQ which results in set up of the MCI structure
		if(!nsiq()) {
			return false;
		}

		// Wait until MobiCore state switches to MC_STATUS_INITIALIZED
		// It is assumed that MobiCore always switches state at a certain point in time.
		while(1)
		{
			uint32_t status = getMobicoreStatus();
			
			if (MC_STATUS_INITIALIZED == status)
			{
				break;
			}
			else if (MC_STATUS_NOT_INITIALIZED == status)
			{
				// Switch to MobiCore to give it more CPU time.
				if(!yield()) {
					return false;
				}
			}
			else if (MC_STATUS_HALT == status)
			{
				dumpMobicoreStatus();
				LOG_E("MobiCore halted during init !!!, state is 0x%x", status);
				return false;
			}
			else // MC_STATUS_BAD_INIT or anything else
			{
				LOG_E("MCI buffer init failed, state is 0x%x", status);
				return false;
			}
		}
	}

	nqStartOut = (notificationQueue_t *) mciBuffer;
	nqStartIn = (notificationQueue_t *) ((uint8_t *) nqStartOut
			+ sizeof(notificationQueueHeader_t) + NQ_NUM_ELEMS
			* sizeof(notification_t));

	// Set up the NWd NQ
	nq = new NotificationQueue(nqStartIn, nqStartOut, NQ_NUM_ELEMS);

	mcpBuffer_t *mcpBuf = (mcpBuffer_t*) ((uint8_t *) mciBuffer + NQ_BUFFER_SIZE);

	// Set up the MC flags
	mcFlags = &(mcpBuf->mcFlags);

	// Set up the MCP message
	mcpMessage = &(mcpBuf->mcpMessage);

	// convert virtual address of mapping to physical address for the init.
	LOG_I("MCP: virt=%p, phys=%p, reused=%s",
			pWsmMcp->virtAddr,
			pWsmMcp->physAddr,
			mciReused ? "true" : "false");
	return true;
}


//------------------------------------------------------------------------------
void TrustZoneDevice::initDeviceStep2(
    void
) {
	// not needed
}


//------------------------------------------------------------------------------
bool TrustZoneDevice::yield(
    void
) {
    int32_t ret = pMcKMod->fcYield();
    if (ret != 0) {
        LOG_E("pMcKMod->fcYield() failed: %d", ret);
    }
    return ret == 0;
}


//------------------------------------------------------------------------------
bool TrustZoneDevice::nsiq(
    void
) {
    // There is no need to set the NON-IDLE flag here. Sending an N-SIQ will 
    // make the MobiCore run until it could set itself to a state where it 
    // set the flag itself. IRQs and FIQs are disbaled for this period, so 
    // there is no way the NWd can interrupt here. 

    // not needed: mcFlags->schedule = MC_FLAG_SCHEDULE_NON_IDLE;

    int32_t ret = pMcKMod->fcNSIQ();
    if (ret != 0) {
        LOG_E("pMcKMod->fcNSIQ() failed : %d", ret);
        return false;
    }
    // now we have to wake the scheduler, so MobiCore gets CPU time.
    schedSync.signal();
    return true;
}


//------------------------------------------------------------------------------
void TrustZoneDevice::notify(
    uint32_t sessionId
) {
	do
	{
        // Check if it is MCP session - handle openSession() command
        if (SID_MCP != sessionId)
        {
            // Check if session ID exists to avoid flooding of nq by clients
            TrustletSession* ts = getTrustletSession(sessionId);
            if (NULL == ts)
            {
                LOG_E("notify(): no session with id=%d", sessionId);
                break;
            }
        }

		LOG_I("notify(): Send notification for id=%d", sessionId);
        // Notify MobiCore about new data

        notification_t notification = {
                // C++ does not support C99 designated initializers
                    /* .sessionId = */ sessionId,
                    /* .payload = */ 0
                };

        nq->putNotification(&notification);
        //IMPROVEMENT-2012-03-07-maneaval What happens when/if nsiq fails?
        //In the old days an exception would be thrown but it was uncertain
        //where it was handled, some server(sock or Netlink). In that case
        //the server would just die but never actually signaled to the client
        //any error condition
		nsiq();

	} while(0);
}

//------------------------------------------------------------------------------
uint32_t TrustZoneDevice::getMobicoreStatus(
	void
) {
	uint32_t status;
	//IMPROVEMENT-2012-03-07-maneaval Can fcInfo ever fail? Before it threw an
	//exception but the handler depended on the context.
	pMcKMod->fcInfo(0, &status, NULL);
	
	return status;
}

//------------------------------------------------------------------------------
bool TrustZoneDevice::checkMciVersion(
    void
) {
    int ret;
    uint32_t version = 0;

    ret = pMcKMod->fcInfo(MC_EXT_INFO_ID_MCI_VERSION, NULL, &version);
    if (ret != 0) {
        LOG_E("pMcKMod->fcInfo() failed with %d", ret);
        return false;
    }

    // Run-time check.
    char* errmsg;
    if (!checkVersionOkMCI(version, &errmsg)) {
        LOG_E("%s", errmsg);
        return false;
    }
    LOG_I("%s", errmsg);
    return true;
}

//------------------------------------------------------------------------------
void TrustZoneDevice::dumpMobicoreStatus(
	void
) {
	int ret;
	uint32_t status, info;
	// read additional info about exception-point and print
	LOG_E("MobiCore halted !!!");
	ret = pMcKMod->fcInfo(1, &status, &info);		
	LOG_W("MC_HALT: flags               : 0x%8x", info);
	ret = pMcKMod->fcInfo(2, &status, &info);		
	LOG_W("MC_HALT: haltCode            : 0x%8x", info);
	ret = pMcKMod->fcInfo(3, &status, &info);		
	LOG_W("MC_HALT: haltIp              : 0x%8x", info);
	ret = pMcKMod->fcInfo(4, &status, &info);		
	LOG_W("MC_HALT: faultRec.cnt        : 0x%8x", info);
	ret = pMcKMod->fcInfo(5, &status, &info);		
	LOG_W("MC_HALT: faultRec.cause      : 0x%8x", info);
	ret = pMcKMod->fcInfo(6, &status, &info);		
	LOG_W("MC_HALT: faultRec.meta       : 0x%8x", info);
	ret = pMcKMod->fcInfo(7, &status, &info);		
	LOG_W("MC_HALT: faultRec.thread     : 0x%8x", info);
	ret = pMcKMod->fcInfo(8, &status, &info);		
	LOG_W("MC_HALT: faultRec.ip         : 0x%8x", info);
	ret = pMcKMod->fcInfo(9, &status, &info);		
	LOG_W("MC_HALT: faultRec.sp         : 0x%8x", info);
	ret = pMcKMod->fcInfo(10, &status, &info);		
	LOG_W("MC_HALT: faultRec.arch.dfsr  : 0x%8x", info);
	ret = pMcKMod->fcInfo(11, &status, &info);		
	LOG_W("MC_HALT: faultRec.arch.adfsr : 0x%8x", info);
	ret = pMcKMod->fcInfo(12, &status, &info);		
	LOG_W("MC_HALT: faultRec.arch.dfar  : 0x%8x", info);
	ret = pMcKMod->fcInfo(13, &status, &info);		
	LOG_W("MC_HALT: faultRec.arch.ifsr  : 0x%8x", info);
	ret = pMcKMod->fcInfo(14, &status, &info);		
	LOG_W("MC_HALT: faultRec.arch.aifsr : 0x%8x", info);
	ret = pMcKMod->fcInfo(15, &status, &info);		
	LOG_W("MC_HALT: faultRec.arch.ifar  : 0x%8x", info);
	ret = pMcKMod->fcInfo(16, &status, &info);		
	LOG_W("MC_HALT: mcData.flags        : 0x%8x", info);
    ret = pMcKMod->fcInfo(19, &status, &info);
    LOG_W("MC_HALT: mcExcep.partner     : 0x%8x", info);
    ret = pMcKMod->fcInfo(20, &status, &info);
    LOG_W("MC_HALT: mcExcep.peer        : 0x%8x", info);
    ret = pMcKMod->fcInfo(21, &status, &info);
    LOG_W("MC_HALT: mcExcep.message     : 0x%8x", info);
    ret = pMcKMod->fcInfo(22, &status, &info);
    LOG_W("MC_HALT: mcExcep.data        : 0x%8x", info);
}

//------------------------------------------------------------------------------
bool TrustZoneDevice::waitSsiq(
    void
) {
    uint32_t cnt;
    if (!pMcKMod->waitSSIQ(&cnt))
    {
        LOG_E("pMcKMod->SSIQ() failed");
        return false;
    }
    LOG_I("SSIQ Received, COUNTER = %u", cnt);
    return true;
}


//------------------------------------------------------------------------------
bool TrustZoneDevice::getMciInstance(
    uint32_t  len,
    CWsm_ptr  *mci,
    bool	  *reused
) {
    addr_t    virtAddr;
    uint32_t  handle;
    addr_t    physAddr;
    bool	  isMci = true;
    if (0 == len)
    {
        LOG_E("allocateWsm() length is 0");
        return false;
    }

    int ret = pMcKMod->mmap(
                        len,
                        &handle,
                        &virtAddr,
                        &physAddr,
                        &isMci);
    if (0 != ret)
    {
        LOG_E("pMcKMod->mmap() failed: %d", ret);
        return false;
    }
    *mci = new CWsm(virtAddr, len, handle, physAddr);
    // isMci will be set to true if buffer has been reused
    *reused = isMci;
    return true;
}


//------------------------------------------------------------------------------
bool TrustZoneDevice::freeWsm(
    CWsm_ptr  pWsm
) {

    int ret = pMcKMod->free(pWsm->handle);
    if (ret != 0)
    {
        LOG_E("pMcKMod->free() failed: %d", ret);
        return false;
    }
    delete pWsm;
    return true;
}


//------------------------------------------------------------------------------
CWsm_ptr TrustZoneDevice::registerWsmL2(
    addr_t    buffer,
    uint32_t  len,
    uint32_t  pid
) {
    addr_t    physAddr;
    uint32_t  handle;

    int ret = pMcKMod->registerWsmL2(
                        buffer,
                        len,
                        pid,
                        &handle,
                        &physAddr);
    if (ret != 0)
    {
        LOG_E("ipMcKMod->registerWsmL2() failed: %d", ret);
        return NULL;
    }

    return new CWsm(buffer,len,handle,physAddr);
}


//------------------------------------------------------------------------------
CWsm_ptr TrustZoneDevice::allocateContiguousPersistentWsm(
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
	    int ret = pMcKMod->mapPersistent(
	                     len,
	                     &handle,
	                     &virtAddr,
	                     &physAddr);
    	if (0 != ret)
    	{
    		break;
    	}

    	// Register (vaddr,paddr) with device
        pWsm = new CWsm(virtAddr,len,handle,physAddr);

    } while(0);

	// Return pointer to the allocated memory
	return pWsm;
}


//------------------------------------------------------------------------------
bool TrustZoneDevice::unregisterWsmL2(
    CWsm_ptr  pWsm
) {
    int ret = pMcKMod->unregisterWsmL2(pWsm->handle);
    if (ret != 0) {
        LOG_E("pMcKMod->unregisterWsmL2 failed: %d", ret);
        //IMPROVEMENT-2012-03-07 maneaval Make sure we don't leak objects
        return false;
    }
    delete pWsm;
    return true;
}

// REV add unregister (used after OPEN_SESSION)

//------------------------------------------------------------------------------
bool TrustZoneDevice::schedulerAvailable(
	void
){
    return schedulerEnabled;
}

//------------------------------------------------------------------------------
//TODO Schedulerthread to be switched off if MC is idle. Will be woken up when
//     driver is called again.
void TrustZoneDevice::schedule(
    void
) {
    uint32_t timeslice = SCHEDULING_FREQ;
	// loop forever
	for (;;)
	{
		// Scheduling decision
		if (MC_FLAG_SCHEDULE_IDLE == mcFlags->schedule)
		{
			// MobiCore is IDLE

			// Prevent unnecessary consumption of CPU cycles -> Wait until S-SIQ received
			schedSync.wait();

		} else {
			// MobiCore is not IDLE (anymore)

			// Check timeslice
			if (0 == timeslice)
			{
				// Slice expired, so force MC internal scheduling decision
				timeslice = SCHEDULING_FREQ;
				if(!nsiq()) {
					break;
				}
			} else {
				// Slice not used up, simply hand over control to the MC
				timeslice--;
				if(!yield()) {
					break;
				}
			}
		}
	}
}
//------------------------------------------------------------------------------
void TrustZoneDevice::handleIrq(
	void
	) {
	LOG_I("Starting NQ IRQ handler...");
	for (;;)
	{
		LOG_I("NQ empty now");
		if(!waitSsiq()) {
			LOG_E("Waiting for SSIQ failed");
			break;
		}
		LOG_I("S-SIQ received");

		// Save all the
		for (;;)
		{
			notification_t *notification = nq->getNotification();
			if (NULL == notification) {
				break;
			}
			LOG_I("Received notification, sessionId=%d, payload=%d",
			notification->sessionId, notification->payload);
			
			// check if the notification belongs to the MCP session
			if (notification->sessionId == SID_MCP) {
				// Signal main thread of the driver to continue after MCP
				// command has been processed by the MC
				signalMcpNotification();
			}
			else
			{
				// Get the NQ connection for the session ID
				Connection *connection = getSessionConnection(notification->sessionId, notification);
				if (connection == NULL) {
					/* Couldn't find the session for this notifications
					 * In practice this only means one thing: there is
					 * a race condition between RTM and the Daemon and
					 * RTM won. But we shouldn't drop the notification
					 * right away we should just queue it in the device
					 */
					LOG_W("Notification for unknown session ID");
					queueUnknownNotification(*notification);
				}
				else
				{
					LOG_I("Write notification!");
					// Forward session ID and additional payload of
					// notification to the TLC/Application layer
					connection->writeData((void *)notification,
								sizeof(notification_t));
				}
			}
		}

		// Wake up scheduler
		schedSync.signal();
	}
	LOG_E("S-SIQ exception");
	// Tell main thread that "something happened"
	// MSH thread MUST not block!
	DeviceIrqHandler::setExiting();
	signalMcpNotification();
}
/** @} */
