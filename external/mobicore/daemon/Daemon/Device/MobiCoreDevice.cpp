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
#include <pthread.h>
#include "McTypes.h"

#include "DeviceScheduler.h"
#include "DeviceIrqHandler.h"
#include "ExcDevice.h"
#include "Connection.h"
#include "TrustletSession.h"

#include "MobiCoreDevice.h"
#include "Mci/mci.h"
#include "mcLoadFormat.h"


#define LOG_TAG	"McDaemon"
#include "log.h"
#include "public/MobiCoreDevice.h"


//------------------------------------------------------------------------------
MobiCoreDevice::MobiCoreDevice() {
	mcFault = false;
}

//------------------------------------------------------------------------------
MobiCoreDevice::~MobiCoreDevice() {
	delete mcVersionInfo;
	mcVersionInfo = NULL;
}

//------------------------------------------------------------------------------
TrustletSession* MobiCoreDevice::getTrustletSession(
    uint32_t sessionId
) {
	TrustletSession* ts = NULL;

	for (trustletSessionIterator_t session = trustletSessions.begin();
			session != trustletSessions.end();
			++session) {
		TrustletSession* tsTmp = *session;
		if (tsTmp->sessionId == sessionId)
		{
			ts = tsTmp;
			break;
		}
	}
	return ts;
}


//------------------------------------------------------------------------------
Connection * MobiCoreDevice::getSessionConnection(
	uint32_t sessionId,
	notification_t *notification	
) {
	Connection *con = NULL;
	TrustletSession* ts = NULL;

	ts = getTrustletSession(sessionId);
	if (NULL == ts) {
		return NULL;
	}

	con = ts->notificationConnection;
	if(NULL == con) {
		ts->queueNotification(notification);
		return NULL;
	}

	return con;
}


//------------------------------------------------------------------------------
bool MobiCoreDevice::open(
    Connection *connection
) {
	// Link this device to the connection
	connection->connectionData = this;
	return true;
}


//------------------------------------------------------------------------------
/**
 * Close device.
 *
 * Removes all sessions to a connection. Though, clientLib rejects the closeDevice()
 * command if still sessions connected to the device, this is needed to clean up all
 * sessions if client dies.
 */
void MobiCoreDevice::close(
    Connection *connection
) {
	trustletSessionList_t::reverse_iterator interator;
	static CMutex mutex;
	// 1. Iterate through device session to find connection
	// 2. Decide what to do with open Trustlet sessions
	// 3. Remove & delete deviceSession from vector

	// Enter critical section
	mutex.lock();
	for (interator = trustletSessions.rbegin();
		interator != trustletSessions.rend();
		interator++) {
		TrustletSession* ts = *interator;

		if (ts->deviceConnection == connection) {
			closeSession(connection, ts->sessionId);
		}
	}
	// Leave critical section
	mutex.unlock();

	connection->connectionData = NULL;
}


//------------------------------------------------------------------------------
void MobiCoreDevice::start(
    void
) {
	// Call the device specific initialization
	//	initDevice();

	LOG_I("Starting DeviceIrqHandler...");
	// Start the irq handling thread
	DeviceIrqHandler::start();

	if (schedulerAvailable()) {
		LOG_I("Starting DeviceScheduler...");
		// Start the scheduling handling thread
		DeviceScheduler::start();
	} else {
		LOG_I("No DeviceScheduler available.");
	}
}


//------------------------------------------------------------------------------
void MobiCoreDevice::signalMcpNotification(
    void
) {
	mcpSessionNotification.signal();
}


//------------------------------------------------------------------------------
bool MobiCoreDevice::waitMcpNotification(
    void
) {
	int counter = 5;
	while (1) {
		// In case of fault just return, nothing to do here
		if (mcFault) {
			return false;
		}
		// Wait 10 seconds for notification
		if (mcpSessionNotification.wait(10) == false) {
			// No MCP answer received and mobicore halted, dump mobicore status 
			// then throw exception
			LOG_I("No MCP answer received in 2 seconds.");
			if (getMobicoreStatus() == MC_STATUS_HALT) {
				dumpMobicoreStatus();
				mcFault = true;
				return false;
			} else {
				counter--;
				if (counter < 1) {
					mcFault = true;
					return false;
				} 
			}
		} else {
				break;
		}
	}

	// Check healthiness state of the device
	if (DeviceIrqHandler::isExiting()) {
		LOG_I("waitMcpNotification(): IrqHandler thread died! Joining");
		DeviceIrqHandler::join();
		LOG_I("waitMcpNotification(): Joined");
		LOG_E("IrqHandler thread died!");
		return false;
	}

	if (DeviceScheduler::isExiting()) {
		LOG_I("waitMcpNotification(): Scheduler thread died! Joining");
		DeviceScheduler::join();
		LOG_I("waitMcpNotification(): Joined");
		LOG_E("Scheduler thread died!");
		return false;
	}
	return true;
}


//------------------------------------------------------------------------------
void MobiCoreDevice::openSession(
	Connection                      *deviceConnection,
	loadDataOpenSession_ptr         pLoadDataOpenSession,
	mcDrvCmdOpenSessionPayload_ptr  pCmdOpenSessionPayload,
	mcDrvRspOpenSessionPayload_ptr  pRspOpenSessionPayload
) {

	do {
		// Write MCP open message to buffer
		mcpMessage->cmdOpen.cmdHeader.cmdId = MC_MCP_CMD_OPEN_SESSION;
		mcpMessage->cmdOpen.uuid = pCmdOpenSessionPayload->uuid;
		mcpMessage->cmdOpen.wsmTypeTci = WSM_CONTIGUOUS;
		mcpMessage->cmdOpen.adrTciBuffer = (uint32_t)(pCmdOpenSessionPayload->tci);
		mcpMessage->cmdOpen.ofsTciBuffer = 0;
		mcpMessage->cmdOpen.lenTciBuffer = pCmdOpenSessionPayload->len;

		LOG_I("%s(): tciPhys=%p, len=%d,", __FUNCTION__,
				(addr_t)(pCmdOpenSessionPayload->tci),
				pCmdOpenSessionPayload->len);

		// check if load data is provided
		if(NULL == pLoadDataOpenSession)
		{
			// Preinstalled trustlet shall be loaded
			mcpMessage->cmdOpen.wsmTypeLoadData = WSM_INVALID;
		}
		else
		{
			mcpMessage->cmdOpen.wsmTypeLoadData = WSM_L2;
			mcpMessage->cmdOpen.adrLoadData = (uint32_t)pLoadDataOpenSession->baseAddr;
			mcpMessage->cmdOpen.ofsLoadData = pLoadDataOpenSession->offs;
			mcpMessage->cmdOpen.lenLoadData = pLoadDataOpenSession->len;
			memcpy(&mcpMessage->cmdOpen.tlHeader, pLoadDataOpenSession->tlHeader, sizeof(*pLoadDataOpenSession->tlHeader));
		}

		// Clear the notifications queue. We asume the race condition we have
		// seen in openSession never happens elsewhere
		notifications = std::queue<notification_t>();
		// Notify MC about a new command inside the MCP buffer
		notify(SID_MCP);

		// Wait till response from MC is available
		if(!waitMcpNotification()) {
			break;
		}

		// Check if the command response ID is correct
		if ((MC_MCP_CMD_OPEN_SESSION | FLAG_RESPONSE) != mcpMessage->rspHeader.rspId)
		{
			LOG_E("%s(): CMD_OPEN_SESSION got invalid MCP command response(0x%X)",
				  __FUNCTION__, mcpMessage->rspHeader.rspId);
			break;
		}

		uint32_t mcRet = mcpMessage->rspOpen.rspHeader.result;
		pRspOpenSessionPayload->mcResult = mcRet;

		if(MC_MCP_RET_OK != mcRet)
		{
			LOG_E("%s: CMD_OPEN_SESSION error %d", __FUNCTION__, mcRet);
			break;
		}

		LOG_I("%s: We have %d queued notifications after open session", 
			  __FUNCTION__, notifications.size());
		// Read MC answer from MCP buffer
		TrustletSession *trustletSession = new TrustletSession(
													deviceConnection,
													mcpMessage->rspOpen.sessionId);

		pRspOpenSessionPayload->deviceId = pCmdOpenSessionPayload->deviceId;
		pRspOpenSessionPayload->sessionId = trustletSession->sessionId;
		pRspOpenSessionPayload->deviceSessionId = (uint32_t)trustletSession;
		pRspOpenSessionPayload->sessionMagic = trustletSession->sessionMagic;

		trustletSessions.push_back(trustletSession);
		// We have some queued notifications and we need to send them to them
		// trustlet session
		while (!notifications.empty()) {
			trustletSession->queueNotification(&notifications.front());
			notifications.pop();
		}

	} while(0);
}


//------------------------------------------------------------------------------
TrustletSession *MobiCoreDevice::registerTrustletConnection(
	Connection                    *connection,
	mcDrvCmdNqConnectPayload_ptr  pCmdNqConnectPayload
) {
	LOG_I("%s(): searching sessionMagic %d and sessionId %d", __FUNCTION__,
		pCmdNqConnectPayload->sessionMagic,
		pCmdNqConnectPayload->sessionId);

	for (trustletSessionIterator_t iterator = trustletSessions.begin();
			iterator != trustletSessions.end();
			++iterator) {
		TrustletSession *ts = *iterator;

		if (ts != (TrustletSession*) (pCmdNqConnectPayload->deviceSessionId)) {
			continue;
		}

		if ( (ts->sessionMagic != pCmdNqConnectPayload->sessionMagic)
				|| (ts->sessionId != pCmdNqConnectPayload->sessionId)) {
			continue;
		}

		LOG_I("%s(): found connection", __FUNCTION__);

		ts->notificationConnection = connection;
		return ts;
	}

	LOG_I("registerTrustletConnection(): search failed");
	return NULL;
}


//------------------------------------------------------------------------------
/**
 * Need connection as well as according session ID, so that a client can not
 * close sessions not belonging to him.
 */
bool MobiCoreDevice::closeSession(
    Connection  *deviceConnection,
	uint32_t    sessionId
) {
	bool ret = true;

	do {
		TrustletSession *ts = NULL;
		trustletSessionIterator_t iterator;

		// Search object to session id
		for (iterator = trustletSessions.begin();
				iterator != trustletSessions.end();
				++iterator)
		{
			TrustletSession  *tsTmp = *iterator;
			if ( (tsTmp->sessionId == sessionId)
					&& (tsTmp->deviceConnection == deviceConnection))
			{
				ts = tsTmp;
				break;
			}
		}
		if (NULL == ts)
		{
			LOG_I("closeSession(): no session found with id=%d",sessionId);
			ret = false;
			break;
		}

		LOG_I("closeSession(): Write MCP close message to buffer and notify, wait");

		// Write MCP close message to buffer
		mcpMessage->cmdClose.cmdHeader.cmdId = MC_MCP_CMD_CLOSE_SESSION;
		mcpMessage->cmdClose.sessionId = sessionId;

		// Notify MC about the availability of a new command inside the MCP buffer
		notify(SID_MCP);

		// Wait till response from MSH is available
		if(!waitMcpNotification()) {
			ret = false;
			break;
		}

		// Check if the command response ID is correct
		if ((MC_MCP_CMD_CLOSE_SESSION | FLAG_RESPONSE) != mcpMessage->rspHeader.rspId) {
			LOG_E("closeSession(): CMD_CLOSE_SESSION got invalid MCP response");
			ret = false;
			break;
		}

		// Read MC answer from MCP buffer
		uint32_t mcRet = mcpMessage->rspOpen.rspHeader.result;

		if( MC_MCP_RET_OK != mcRet) {
			LOG_E("closeSession(): CMD_CLOSE_SESSION error %d",mcRet);
			ret = false;
			break;
		}

		// remove objects
		trustletSessions.erase(iterator);
		delete ts;

	} while(0);

	return ret;
}


//------------------------------------------------------------------------------
void MobiCoreDevice::mapBulk(
    Connection                     *deviceConnection,
    mcDrvCmdMapBulkMemPayload_ptr  pCmdMapBulkMemPayload,
    mcDrvRspMapBulkMemPayload_ptr  pRspMapBulkMemPayload
) {
	do
	{

		// Write MCP map message to buffer
		mcpMessage->cmdMap.cmdHeader.cmdId = MC_MCP_CMD_MAP;
		mcpMessage->cmdMap.sessionId = pCmdMapBulkMemPayload->sessionId;
		mcpMessage->cmdMap.wsmType = WSM_L2;
		mcpMessage->cmdMap.adrBuffer = (uint32_t)(pCmdMapBulkMemPayload->pAddrL2);
		mcpMessage->cmdMap.ofsBuffer = pCmdMapBulkMemPayload->offsetPayload;
		mcpMessage->cmdMap.lenBuffer = pCmdMapBulkMemPayload->lenBulkMem;

		// Notify MC about the availability of a new command inside the MCP buffer
		notify(SID_MCP);

		// Wait till response from MC is available
		if(!waitMcpNotification()) {
			break;
		}

		// Check if the command response ID is correct
		if ((MC_MCP_CMD_MAP | FLAG_RESPONSE) != mcpMessage->rspHeader.rspId) {
			LOG_E("mapBulk(): CMD_MAP got invalid MCP response");
			break;
		}

		uint32_t mcRet = mcpMessage->rspMap.rspHeader.result;
		pRspMapBulkMemPayload->mcResult = mcRet;
		pRspMapBulkMemPayload->sessionId = pCmdMapBulkMemPayload->sessionId;

		if(MC_MCP_RET_OK != mcRet) {
			LOG_E("mapBulk(): CMD_MAP error %d",mcRet);
			break;
		}

		pRspMapBulkMemPayload->secureVirtualAdr = mcpMessage->rspMap.secureVirtualAdr;

	} while(0);
}


//------------------------------------------------------------------------------
void MobiCoreDevice::unmapBulk(
    Connection                       *deviceConnection,
    mcDrvCmdUnmapBulkMemPayload_ptr  pCmdUnmapBulkMemPayload,
    mcDrvRspUnmapBulkMemPayload_ptr  pRspUnmapBulkMemPayload
) {
	do {
		// Write MCP unmap command to buffer
		mcpMessage->cmdUnmap.cmdHeader.cmdId = MC_MCP_CMD_UNMAP;
		mcpMessage->cmdUnmap.sessionId = pCmdUnmapBulkMemPayload->sessionId;
		mcpMessage->cmdUnmap.wsmType = WSM_L2;
		mcpMessage->cmdUnmap.secureVirtualAdr = pCmdUnmapBulkMemPayload->secureVirtualAdr;
		mcpMessage->cmdUnmap.lenVirtualBuffer = pCmdUnmapBulkMemPayload->lenBulkMem;

		// Notify MC about the availability of a new command inside the MCP buffer
		notify(SID_MCP);

		// Wait till response from MC is available
		if(!waitMcpNotification()) {
			break;
		}

		// Check if the command response ID is correct
		if ((MC_MCP_CMD_UNMAP | FLAG_RESPONSE) != mcpMessage->rspHeader.rspId)
		{
			LOG_E("unmapBulk(): CMD_OPEN_SESSION got invalid MCP response");
			break;
		}

		uint32_t  mcRet = mcpMessage->rspUnmap.rspHeader.result;
		pRspUnmapBulkMemPayload->mcResult = mcRet;
		pRspUnmapBulkMemPayload->sessionId = pCmdUnmapBulkMemPayload->sessionId;

		if(MC_MCP_RET_OK != mcRet)
		{
			LOG_E("unmapBulk(): MC_MCP_CMD_UNMAP error %d",mcRet);
			break;
		}

	} while(0);
}


//------------------------------------------------------------------------------
void MobiCoreDevice::donateRam(
	const uint32_t	donationSize
) {
	// Donate additional RAM to the MobiCore
	CWsm_ptr ram = allocateContiguousPersistentWsm(donationSize);
	if (NULL == ram) {
		LOG_E("Allocation of additional RAM failed");
		return;
	}
	ramType_t ramType = RAM_GENERIC;
	addr_t adrBuffer = ram->physAddr;
	const uint32_t numPages = donationSize / (4 * 1024);


	LOG_I("donateRam(): adrBuffer=%p, numPages=%d, ramType=%d",
			adrBuffer,
			numPages,
			ramType);

	do {
		// Write MCP open message to buffer
		mcpMessage->cmdDonateRam.cmdHeader.cmdId = MC_MCP_CMD_DONATE_RAM;
		mcpMessage->cmdDonateRam.adrBuffer = (uint32_t) adrBuffer;
		mcpMessage->cmdDonateRam.numPages = numPages;
		mcpMessage->cmdDonateRam.ramType = ramType;

		// Notify MC about a new command inside the MCP buffer
		notify(SID_MCP);

		// Wait till response from MC is available
		if(!waitMcpNotification()) {
			break;
		}

		// Check if the command response ID is correct
		if ((MC_MCP_CMD_DONATE_RAM | FLAG_RESPONSE) != mcpMessage->rspHeader.rspId)
		{
			LOG_E("donateRam(): CMD_DONATE_RAM got invalid MCP response - rspId is: %d",
					mcpMessage->rspHeader.rspId);
			break;
		}

		uint32_t mcRet = mcpMessage->rspDonateRam.rspHeader.result;
		if(MC_MCP_RET_OK != mcRet)
		{
			LOG_E("donateRam(): CMD_DONATE_RAM error %d", mcRet);
			break;
		}

		LOG_I("donateRam() succeeded.");

	} while(0);
}

//------------------------------------------------------------------------------
void MobiCoreDevice::getMobiCoreVersion(
    mcDrvRspGetMobiCoreVersionPayload_ptr pRspGetMobiCoreVersionPayload
) {
	// If MobiCore version info already fetched.
	if (mcVersionInfo != NULL) {
		pRspGetMobiCoreVersionPayload->mcResult = MC_MCP_RET_OK;
		pRspGetMobiCoreVersionPayload->versionInfo = *mcVersionInfo;
	// Otherwise, fetch it via MCP.
	} else {
		pRspGetMobiCoreVersionPayload->mcResult = MC_MCP_RET_ERR_UNKNOWN;

		// Write MCP unmap command to buffer
		mcpMessage->cmdGetMobiCoreVersion.cmdHeader.cmdId = MC_MCP_CMD_GET_MOBICORE_VERSION;

		// Notify MC about the availability of a new command inside the MCP buffer
		notify(SID_MCP);

		// Wait till response from MC is available
		if(!waitMcpNotification()) {
			return;
		}

		// Check if the command response ID is correct
		if ((MC_MCP_CMD_GET_MOBICORE_VERSION | FLAG_RESPONSE) != mcpMessage->rspHeader.rspId) {
			LOG_E("getMobiCoreVersion(): MC_MCP_CMD_GET_MOBICORE_VERSION got invalid MCP response");
			return;
		}

		uint32_t  mcRet = mcpMessage->rspGetMobiCoreVersion.rspHeader.result;
		pRspGetMobiCoreVersionPayload->mcResult = mcRet;

		if(MC_MCP_RET_OK != mcRet) {
			LOG_E("getMobiCoreVersion(): MC_MCP_CMD_GET_MOBICORE_VERSION error %d",mcRet);
			return;
		}

		pRspGetMobiCoreVersionPayload->versionInfo = mcpMessage->rspGetMobiCoreVersion.versionInfo;

		// Store MobiCore info for future reference.
		mcVersionInfo = new mcVersionInfo_t();
		*mcVersionInfo = pRspGetMobiCoreVersionPayload->versionInfo;
	}
}

//------------------------------------------------------------------------------
void MobiCoreDevice::queueUnknownNotification(
	notification_t notification
) {
	notifications.push(notification);
}

/** @} */
