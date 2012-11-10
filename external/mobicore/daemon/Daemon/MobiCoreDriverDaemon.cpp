/** @addtogroup MCD_MCDIMPL_DAEMON_CONHDLR
 * @{
 * @file
 *
 * Entry of the MobiCore Driver.
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
#include <signal.h>
#include <fcntl.h>
#ifndef REDUCED_STLPORT
#include <fstream>
#include <cassert>
#else
#include <stdio.h>
#endif

#include "MobiCoreDriverCmd.h"
#include "mcVersion.h"
#include "mcVersionHelper.h"
#include "mcDrvModuleApi.h"

#include "MobiCoreDriverDaemon.h"
#include "MobiCoreRegistry.h"
#include "MobiCoreDevice.h"

#include "NetlinkServer.h"

#define LOG_TAG	"McDaemon"
#include "log.h"

#define DRIVER_TCI_LEN 100

#include "Mci/mci.h"

MC_CHECK_VERSION(MCI, 0, 2);
MC_CHECK_VERSION(SO, 2, 0);
MC_CHECK_VERSION(MCLF, 2, 0);
MC_CHECK_VERSION(CONTAINER, 2, 0); 

static void checkMobiCoreVersion(MobiCoreDevice* mobiCoreDevice);

//------------------------------------------------------------------------------
MobiCoreDriverDaemon::MobiCoreDriverDaemon(
	bool enableScheduler,
	bool loadMobicore,
	std::string mobicoreImage,
	unsigned int donateRamSize,
	bool loadDriver,
	std::string driverPath
) {
	mobiCoreDevice = NULL;

	this->enableScheduler = enableScheduler;
	this->loadMobicore = loadMobicore;
	this->mobicoreImage = mobicoreImage;
	this->donateRamSize = donateRamSize;
	this->loadDriver = loadDriver;
	this->driverPath = driverPath;

	for (int i = 0; i < MAX_SERVERS; i++) {
		servers[i] = NULL;
	}
}

//------------------------------------------------------------------------------
MobiCoreDriverDaemon::~MobiCoreDriverDaemon(
    void
) {
	// Unload any device drivers might have been loaded
	driverResourcesList_t::iterator it;
	for(it = driverResources.begin(); it != driverResources.end(); it++) {
		MobicoreDriverResources *res = *it;
		mobiCoreDevice->closeSession(res->conn, res->sessionId);
		mobiCoreDevice->unregisterWsmL2(res->pTciWsm);
	}
	delete mobiCoreDevice;
	for (int i = 0; i < MAX_SERVERS; i++) {
		delete servers[i];
		servers[i] = NULL;
	}
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::run(
	void
) {
	LOG_I("Daemon starting up...");
	LOG_I("Socket interface version is %u.%u", DAEMON_VERSION_MAJOR, DAEMON_VERSION_MINOR);
#ifdef MOBICORE_COMPONENT_BUILD_TAG
	LOG_I("%s", MOBICORE_COMPONENT_BUILD_TAG);
#else
	#warning "MOBICORE_COMPONENT_BUILD_TAG is not defined!"
#endif
	LOG_I("Build timestamp is %s %s", __DATE__, __TIME__);

	int i;

	mobiCoreDevice = getDeviceInstance();

	LOG_I("Daemon scheduler is %s", enableScheduler? "enabled" : "disabled");
	if(!mobiCoreDevice->initDevice(
		MC_DRV_MOD_DEVNODE_FULLPATH,
		loadMobicore,
		mobicoreImage.c_str(),
		enableScheduler)) {
		LOG_E("%s: Failed to initialize MobiCore!", __FUNCTION__);
		return;
	}
	mobiCoreDevice->start();

	checkMobiCoreVersion(mobiCoreDevice);

	if (donateRamSize > 0) {
		// Donate additional RAM to MC
		LOG_I("Donating %u Kbytes to Mobicore", donateRamSize / 1024);
		mobiCoreDevice->donateRam(donateRamSize);
	}

	// Load device driver if requested
	if (loadDriver) {
		loadDeviceDriver(driverPath);
	}

	LOG_I("Servers will be created!");
	// Start listening for incoming TLC connections
	servers[0] = new NetlinkServer(this);
	servers[1] = new Server(this, SOCK_PATH);
	LOG_I("Servers created!");

	// Start all the servers
	for (i = 0; i < MAX_SERVERS; i++) {
		servers[i]->start();
	}

	// then wait for them to exit
	for (i = 0; i < MAX_SERVERS; i++) {
		servers[i]->join();
	}
}


//------------------------------------------------------------------------------
MobiCoreDevice *MobiCoreDriverDaemon::getDevice(
    uint32_t deviceId
) {
	// Always return the trustZoneDevice as it is currently the only one supported
	if(MC_DEVICE_ID_DEFAULT != deviceId)
		return NULL;
	return mobiCoreDevice;
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::dropConnection(
    Connection *connection
) {
	// Check if a Device has already been registered with the connection
	MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);

	if (device != NULL) {
		LOG_I("dropConnection(): closing still open device.");
		// A connection has been found and has to be closed
		device->close(connection);
	}
}


//------------------------------------------------------------------------------
size_t MobiCoreDriverDaemon::writeResult(
    Connection	*connection,
    mcDrvRsp_t  code
) {
	if (0 != code) {
		LOG_E("writeResult(): set error code %d",code);
	}
	return connection->writeData(&code, sizeof(mcDrvRsp_t));
}

//------------------------------------------------------------------------------
bool MobiCoreDriverDaemon::loadDeviceDriver(
	std::string driverPath
) {
	bool ret = false;
	CWsm_ptr pWsm = NULL, pTciWsm = NULL;
	regObject_t *regObj = NULL;
	Connection *conn = NULL;
	uint8_t *tci = NULL;
	mcDrvRspOpenSession_t rspOpenSession;
	
	do
	{
		//mobiCoreDevice
#ifndef REDUCED_STLPORT
		ifstream fs(driverPath.c_str(), ios_base::binary);
		if (!fs) {
			LOG_E("%s: failed: cannot open %s", __func__, driverPath.c_str());
			break;
		}
#else
		FILE *fs = fopen (driverPath.c_str(), "rb");
		if (!fs) {
			LOG_E("%s: failed: cannot open %s", __func__, driverPath.c_str());
			break;
		}
		fclose(fs);
#endif

		LOG_I("%s: loading %s", __func__, driverPath.c_str());
		
		regObj = mcRegistryGetDriverBlob(driverPath.c_str());
		if (regObj == NULL) {
			break;;
		}

		LOG_I("registering L2 in kmod, p=%p, len=%i",
				regObj->value, regObj->len);
		
		// Prepare the interface structure for memory registration, then
		// register virtual memory in kernel module, create L2 table
		// TODO xgal: refactor naming of datatypes and WSM handling
		pWsm = mobiCoreDevice->registerWsmL2(
			(addr_t)(regObj->value), regObj->len, 0);
		if (pWsm == NULL)
		{
			LOG_E("allocating WSM for Trustlet failed");
			break;
		}
		// Initialize information data of open session command
		loadDataOpenSession_t loadDataOpenSession;
		loadDataOpenSession.baseAddr = pWsm->physAddr;
		loadDataOpenSession.offs = ((uint32_t) regObj->value) & 0xFFF;
		loadDataOpenSession.len = regObj->len;
		loadDataOpenSession.tlHeader = (mclfHeader_ptr) regObj->value;
		
		mcDrvCmdOpenSessionPayload_t  openSessionPayload;
		tci = (uint8_t*)malloc(DRIVER_TCI_LEN);
		pTciWsm = mobiCoreDevice->registerWsmL2(
			(addr_t)tci, DRIVER_TCI_LEN, 0);
		if (pTciWsm == NULL)
		{
			LOG_E("allocating WSM TCI for Trustlet failed");
			break;
		}
		openSessionPayload.deviceId = MC_DEVICE_ID_DEFAULT;
		openSessionPayload.tci = (uint32_t)pTciWsm->physAddr;
		openSessionPayload.len = DRIVER_TCI_LEN;

		conn = new Connection();
		mobiCoreDevice->openSession(
			conn,
			&loadDataOpenSession,
			&openSessionPayload,
			&(rspOpenSession.payload));
		
		// Unregister physical memory from kernel module.
		// This will also destroy the WSM object.
		mobiCoreDevice->unregisterWsmL2(pWsm);
		pWsm = NULL;
		
		// Free memory occupied by Trustlet data
		free(regObj);
		regObj = NULL;
		
		if (rspOpenSession.payload.mcResult != MC_MCP_RET_OK)
		{
			LOG_E("%s: rspOpenSession mcResult %d", __func__, 
				  rspOpenSession.payload.mcResult);
			break;
		}
		
		ret = true;
	} while (false);
	// Free all allocated resources
	if (ret == false) {
		LOG_I("%s: Freeing previously allocated resources!", __func__);
		if (pWsm != NULL) {
			if(!mobiCoreDevice->unregisterWsmL2(pWsm)) {
				// At least make sure we don't leak the WSM object
				delete pWsm;
			}
		}
		// No matter if we free NULL objects
		free(regObj);
		
		if (conn != NULL) {
			delete conn;
		}
	} else if (conn != NULL) {
		driverResources.push_back(new MobicoreDriverResources(
			conn, tci, pTciWsm, rspOpenSession.payload.sessionId));
	}
	
	return ret;
}

//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processOpenDevice(
    Connection	*connection
) {
	do
	{
		// Read entire command data
		mcDrvCmdOpenDevicePayload_t cmdOpenDevicePayload;
		uint32_t rlen = connection->readData(
							&(cmdOpenDevicePayload),
							sizeof(cmdOpenDevicePayload));
		if (rlen != sizeof(cmdOpenDevicePayload))
		{
			LOG_E("processOpenDevice(): OpenSession length error: %d", rlen);
			writeResult(connection, MC_DRV_RSP_PAYLOAD_LENGTH_ERROR);
			break;
		}

		// Check if device has been registered to the connection
		MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);
		if (NULL != device)
		{
			LOG_E("processOpenDevice(): device already set");
			writeResult(connection, MC_DRV_RSP_DEVICE_ALREADY_OPENED);
			break;
		}

		LOG_I("processOpenDevice(): deviceId is %d",
				cmdOpenDevicePayload.deviceId);

		// Get device for device ID
		device = getDevice(cmdOpenDevicePayload.deviceId);

		// Check if a device for the given name has been found
		if (NULL == device)
		{
			LOG_E("invalid deviceId");
			writeResult(connection, MC_DRV_INVALID_DEVICE_NAME);
			break;
		}

		// Register device object with connection
		if (false == device->open(connection))
		{
			LOG_E("processOpenDevice(): device->open() failed");
			writeResult(connection, MC_DRV_RSP_FAILED);
			break;
		}

		// Return result code to client lib (no payload)
		writeResult(connection, MC_DRV_RSP_OK);

	} while (false);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processCloseDevice(
    Connection  *connection
) {
	do
	{
		// there is no payload to read

		// Device required
		MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);
		if (NULL == device)
		{
			LOG_E("processCloseDevice(): no device");
			writeResult(connection, MC_DRV_RSP_DEVICE_NOT_OPENED);
			break;
		}

		// No command data will be read
		// Unregister device object with connection
		device->close(connection);

		// there is no payload
		writeResult(connection, MC_DRV_RSP_OK);

	} while (false);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processOpenSession(
    Connection  *connection
) {
	do
	{
		// Read entire command data
		mcDrvCmdOpenSessionPayload_t  cmdOpenSessionPayload;
		uint32_t rlen = connection->readData(
							&cmdOpenSessionPayload,
							sizeof(cmdOpenSessionPayload));
		if (rlen != sizeof(cmdOpenSessionPayload))
		{
			LOG_E("processOpenSession(): OpenSession length error: %d", rlen);
			writeResult(connection, MC_DRV_RSP_PAYLOAD_LENGTH_ERROR);
			break;
		}

		// Device required
		MobiCoreDevice  *device = (MobiCoreDevice *) (connection->connectionData);
		if (NULL == device)
		{
			writeResult(connection, MC_DRV_RSP_DEVICE_NOT_OPENED);
			break;
		}

		// Get service blob from registry
		regObject_t *regObj = mcRegistryGetServiceBlob(
				(mcUuid_t*) &(cmdOpenSessionPayload.uuid));

		// Call preinstalled variant of method
		mcDrvRspOpenSession_t rspOpenSession;
		if (NULL == regObj)
		{
			writeResult(connection, MC_DRV_RSP_TRUSTLET_NOT_FOUND);
			break;
		}
		if (0 == regObj->len)
		{
			free(regObj);
			writeResult(connection, MC_DRV_RSP_TRUSTLET_NOT_FOUND);
			break;
		}
		else
		{
			// Trustlet retrieved from registry

			LOG_I("registering L2 in kmod, p=%p, len=%i",
					regObj->value,
					regObj->len);

			// Prepare the interface structure for memory registration, then
			// register virtual memory in kernel module, create L2 table
			// TODO xgal: refactor naming of datatypes and WSM handling
			CWsm_ptr pWsm = device->registerWsmL2(
				(addr_t)(regObj->value),
				regObj->len,
				0);
			if (NULL == pWsm)
			{
				LOG_E("allocating WSM for Trustlet failed");
				writeResult(connection, MC_DRV_RSP_FAILED);
				break;
			}
			// Initialize information data of open session command
			loadDataOpenSession_t loadDataOpenSession;
			loadDataOpenSession.baseAddr = pWsm->physAddr;
			loadDataOpenSession.offs = ((uint32_t) regObj->value) & 0xFFF;
			loadDataOpenSession.len = regObj->len;
			loadDataOpenSession.tlHeader = (mclfHeader_ptr) regObj->value;
			
			device->openSession(
						connection,
						&loadDataOpenSession,
						&cmdOpenSessionPayload,
						&(rspOpenSession.payload));

			// Unregister physical memory from kernel module.
			// This will also destroy the WSM object.
			if(!device->unregisterWsmL2(pWsm)) {
				writeResult(connection, MC_DRV_RSP_FAILED);
				break;
			}

			// Free memory occupied by Trustlet data
			free(regObj);
		}

		uint32_t mcResult = rspOpenSession.payload.mcResult;

        mcDrvRsp_t responseId = MC_DRV_RSP_FAILED;

        switch (mcResult) 
        {
        case MC_MCP_RET_OK:
            responseId = MC_DRV_RSP_OK;
            break;
        case MC_MCP_RET_ERR_WRONG_PUBLIC_KEY:
            responseId = MC_DRV_RSP_WRONG_PUBLIC_KEY;
            break;
        case MC_MCP_RET_ERR_CONTAINER_TYPE_MISMATCH:
            responseId = MC_DRV_RSP_CONTAINER_TYPE_MISMATCH;
            break;
        case MC_MCP_RET_ERR_CONTAINER_LOCKED:
            responseId = MC_DRV_RSP_CONTAINER_LOCKED;
            break;
        case MC_MCP_RET_ERR_SP_NO_CHILD:
            responseId = MC_DRV_RSP_SP_NO_CHILD;
            break;
        case MC_MCP_RET_ERR_TL_NO_CHILD:
            responseId = MC_DRV_RSP_TL_NO_CHILD;
            break;
        case MC_MCP_RET_ERR_UNWRAP_ROOT_FAILED:
            responseId = MC_DRV_RSP_UNWRAP_ROOT_FAILED;
            break;
        case MC_MCP_RET_ERR_UNWRAP_SP_FAILED:
            responseId = MC_DRV_RSP_UNWRAP_SP_FAILED;
            break;
        case MC_MCP_RET_ERR_UNWRAP_TRUSTLET_FAILED:
            responseId = MC_DRV_RSP_UNWRAP_TRUSTLET_FAILED;
            break;
        }

		if (MC_MCP_RET_OK != mcResult)
		{
			LOG_E("rspOpenSession mcResult %d", mcResult);
			writeResult(connection, responseId);
			break;
		}

		rspOpenSession.header.responseId = responseId;
		connection->writeData(
						&rspOpenSession,
						sizeof(rspOpenSession));

	} while (false);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processCloseSession(
    Connection  *connection
) {
	do
	{
		// Read entire command data
		mcDrvCmdCloseSessionPayload_t  cmdCloseSessionPayload;
		uint32_t rlen = connection->readData(
							&cmdCloseSessionPayload,
							sizeof(cmdCloseSessionPayload));
		if (rlen != sizeof(cmdCloseSessionPayload))
		{
			LOG_E("processCloseSession(): CloseSessionPayload length error: %d",rlen);
			writeResult(connection, MC_DRV_RSP_PAYLOAD_LENGTH_ERROR);
			break;
		}

		// Device required
		MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);
		if (NULL == device)
		{
			LOG_E("processCloseSession(): device is NULL");
			writeResult(connection, MC_DRV_RSP_DEVICE_NOT_OPENED);
			break;
		}

		device->closeSession(
					connection,
					cmdCloseSessionPayload.sessionId);

		// there is no payload
		writeResult(connection, MC_DRV_RSP_OK);

	} while (false);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processNqConnect(
    Connection  *connection
) {
	do
	{
		// Set up the channel for sending SWd notifications to the client
		// MC_DRV_CMD_NQ_CONNECT is only allowed on new connections not
		// associated with a device. If a device is registered to the
		// connection NQ_CONNECT is not allowed.

		// Read entire command data
		mcDrvCmdNqConnectPayload_t  cmdNqConnectPayload;
		size_t rlen = connection->readData(
										&(cmdNqConnectPayload),
										sizeof(cmdNqConnectPayload));
		if (rlen != sizeof(cmdNqConnectPayload))
		{
			LOG_E("processNqConnect(): NqConnect length error: %d",rlen);
			writeResult(connection,MC_DRV_RSP_PAYLOAD_LENGTH_ERROR);
			break;
		}

		// device must be empty
		MobiCoreDevice *device = (MobiCoreDevice *)(connection->connectionData);
		if (NULL != device)
		{
			LOG_E("processNqConnect(): device already set\n");
			writeResult(connection,MC_DRV_RSP_COMMAND_NOT_ALLOWED);
			break;
		}

		// Remove the connection from the list of known client connections
		for (int i = 0; i < MAX_SERVERS; i++) {
			servers[i]->detachConnection(connection);
		}

		device = getDevice(cmdNqConnectPayload.deviceId);
		if (NULL == device)
		{
			//TODO xgal: how about ...NO_SUCH_DEVICE
			LOG_E("processNqConnect(): no device found\n");
			writeResult(connection, MC_DRV_RSP_FAILED);
			break;
		}

		TrustletSession* ts = device->registerTrustletConnection(
								connection,
								&cmdNqConnectPayload);
		if (!ts) {
			LOG_E("processNqConnect(): registerTrustletConnection() failed!");
			writeResult(connection, MC_DRV_RSP_FAILED);
			break;
		}

		writeResult(connection, MC_DRV_RSP_OK);
		ts->processQueuedNotifications();
		

	} while (false);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processNotify(
    Connection  *connection
) {
	do
	{
		// Read entire command data
		mcDrvCmdNotifyPayload_t  cmdNotifyPayload;
		uint32_t rlen = connection->readData(
							&cmdNotifyPayload,
							sizeof(cmdNotifyPayload));
		if (sizeof(cmdNotifyPayload) != rlen)
		{
			LOG_E("processNotify(): NotifyPayload length error: %d", rlen);
			// NOTE: notify fails silently
			// writeResult(connection,MC_DRV_RSP_PAYLOAD_LENGTH_ERROR);
			break;
		}

		// Device required
		MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);
		if (NULL == device)
		{
			LOG_E("processNotify(): device is NULL");
			// NOTE: notify fails silently
			// writeResult(connection,MC_DRV_RSP_DEVICE_NOT_OPENED);
			break;
		}

		// REV axh: we cannot trust the clientLib to give us a valid
		//          sessionId here. Thus we have to check that it belongs to
		//          the clientLib's process.

		device->notify(cmdNotifyPayload.sessionId);
		// NOTE: for notifications there is no response at all
	} while(0);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processMapBulkBuf(
    Connection  *connection
) {
	do
	{
		// Read entire command data
		mcDrvCmdMapBulkMemPayload_t  cmdMapBulkMemPayload;
		uint32_t rlen = connection->readData(
								&cmdMapBulkMemPayload,
								sizeof(cmdMapBulkMemPayload));
		if (rlen != sizeof(cmdMapBulkMemPayload))
		{
			LOG_E("processMapBulkBuf(): MapBulkMemPayload length error: %d", rlen);
			writeResult(connection, MC_DRV_RSP_PAYLOAD_LENGTH_ERROR);
			break;
		}

		// Device required
		MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);
		if (NULL == device)
		{
			LOG_E("processMapBulkBuf(): device is NULL");
			writeResult(connection, MC_DRV_RSP_DEVICE_NOT_OPENED);
			break;
		}

		// Map bulk memory to secure world
		mcDrvRspMapBulkMem_t rspMapBulk;
		device->mapBulk(
					connection,
					&cmdMapBulkMemPayload,
					&(rspMapBulk.payload));

		uint32_t mcResult = rspMapBulk.payload.mcResult;
		if (MC_MCP_RET_OK != mcResult)
		{
			LOG_E("processMapBulkBuf(): rspMapBulk.mcResult=%d", mcResult);
			writeResult(connection, MC_DRV_RSP_FAILED);
			break;
		}

		rspMapBulk.header.responseId = MC_DRV_RSP_OK;
		connection->writeData(&rspMapBulk, sizeof(rspMapBulk));

	} while (false);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processUnmapBulkBuf(
    Connection  *connection
) {
	do
	{
		// Read entire command data
		mcDrvCmdUnmapBulkMemPayload_t cmdUnmapBulkMemPayload;
		uint32_t rlen = connection->readData(
							&cmdUnmapBulkMemPayload,
							sizeof(cmdUnmapBulkMemPayload));
		if (rlen != sizeof(cmdUnmapBulkMemPayload))
		{
			LOG_E("processMapBulkBuf(): UnmapBulkMem length error: %d", rlen);
			writeResult(connection, MC_DRV_RSP_PAYLOAD_LENGTH_ERROR);
			break;
		}

		// Device required
		MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);
		if (NULL == device)
		{
			writeResult(connection, MC_DRV_RSP_DEVICE_NOT_OPENED);
			break;
		}

		// Unmap bulk memory from secure world
		mcDrvRspUnmapBulkMem_t rspUnmpaBulk;

		device->unmapBulk(
					connection,
					&cmdUnmapBulkMemPayload,
					&(rspUnmpaBulk.payload));

		uint32_t mcResult = rspUnmpaBulk.payload.mcResult;
		if (MC_MCP_RET_OK != mcResult)
		{
			LOG_E("processUnmapBulkBuf(): rspUnmpaBulk mcResult %d", mcResult);
			writeResult(connection, MC_DRV_RSP_FAILED);
			break;
		}

		rspUnmpaBulk.header.responseId = MC_DRV_RSP_OK;
		connection->writeData(
						&rspUnmpaBulk,
						sizeof(rspUnmpaBulk));

	} while (false);
}


//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processGetVersion(
    Connection  *connection
) {
	mcDrvRspGetVersion_t rspGetVersion;
	rspGetVersion.payload.version = MC_MAKE_VERSION(DAEMON_VERSION_MAJOR, DAEMON_VERSION_MINOR);

	rspGetVersion.header.responseId = MC_DRV_RSP_OK;
	connection->writeData(
		&rspGetVersion,
		sizeof(rspGetVersion));
}

//------------------------------------------------------------------------------
void MobiCoreDriverDaemon::processGetMobiCoreVersion(
    Connection  *connection
) {
	// Device required
	MobiCoreDevice *device = (MobiCoreDevice *) (connection->connectionData);
	if (NULL == device) {
		writeResult(connection, MC_DRV_RSP_DEVICE_NOT_OPENED);
		return;
	}

	// Get MobiCore version info from secure world.
	mcDrvRspGetMobiCoreVersion_t rspGetMobiCoreVersion;

	device->getMobiCoreVersion(
		&rspGetMobiCoreVersion.payload);

	uint32_t mcResult = rspGetMobiCoreVersion.payload.mcResult;
	if (MC_MCP_RET_OK != mcResult) {
		LOG_E("processGetMobiCoreVersion(): rspGetMobiCoreVersion mcResult %d", mcResult);
		writeResult(connection, MC_DRV_RSP_FAILED);
		return;
	}

	rspGetMobiCoreVersion.header.responseId = MC_DRV_RSP_OK;
	connection->writeData(
		&rspGetMobiCoreVersion,
		sizeof(rspGetMobiCoreVersion));
}


//------------------------------------------------------------------------------
bool MobiCoreDriverDaemon::handleConnection(
    Connection *connection
) {
    bool ret = false;
	static CMutex mutex;
	
	/* In case of RTM fault do not try to signal anything to MobiCore
	 * just answer NO to all incoming connections! */
	if (mobiCoreDevice->getMcFault()) {
		return false;
	}

	mutex.lock();
	do
	{
		// Read header
		mcDrvCommandHeader_t mcDrvCommandHeader;
		uint32_t rlen = connection->readData(
							&(mcDrvCommandHeader),
							sizeof(mcDrvCommandHeader));

		if (0 == rlen)
		{
			LOG_I("handleConnection(): Connection closed.");
			break;
		}
		if (sizeof(mcDrvCommandHeader) != rlen)
		{
			LOG_E("handleConnection(): Header length error: %d", rlen);
			break;
		}
		ret = true;

		switch (mcDrvCommandHeader.commandId)
		{
			//-----------------------------------------
			case MC_DRV_CMD_OPEN_DEVICE:
				processOpenDevice(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_CLOSE_DEVICE:
				processCloseDevice(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_OPEN_SESSION:
				processOpenSession(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_CLOSE_SESSION:
				processCloseSession(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_NQ_CONNECT:
				processNqConnect(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_NOTIFY:
				processNotify(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_MAP_BULK_BUF:
				processMapBulkBuf(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_UNMAP_BULK_BUF:
				processUnmapBulkBuf(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_GET_VERSION:
				processGetVersion(connection);
				break;
			//-----------------------------------------
			case MC_DRV_CMD_GET_MOBICORE_VERSION:
				processGetMobiCoreVersion(connection);
				break;
			//-----------------------------------------

			default:
				LOG_E("handleConnection(): unknown command: %d=0x%x",
						mcDrvCommandHeader.commandId,
						mcDrvCommandHeader.commandId);
				ret = false;
				break;
		}
	} while(0);
	mutex.unlock();

	return ret;
}

//------------------------------------------------------------------------------
/**
 * Print daemon command line options
 */

void printUsage(
	int argc,
	char *args[]
) {
	fprintf(stderr, "usage: %s [-mdsbh]\n", args[0]);
	fprintf(stderr, "Start MobiCore Daemon\n\n");
	fprintf(stderr, "-h\t\tshow this help\n");
	fprintf(stderr, "-b\t\tfork to background\n");
	fprintf(stderr, "-m IMAGE\tload mobicore from IMAGE to DDR\n");
	fprintf(stderr, "-s\t\tdisable daemon scheduler(default enabled)\n");
	fprintf(stderr, "-d SIZE\t\tdonate SIZE bytes to mobicore(disabled on most platforms)\n");
	fprintf(stderr, "-r DRIVER\t\tMobiCore driver to load at start-up\n");
}

//------------------------------------------------------------------------------
/**
 * Signal handler for daemon termination
 * Using this handler instead of the standard libc one ensures the daemon
 * can cleanup everything -> read() on a FD will now return EINTR
 */
void terminateDaemon(
	int signum
) {
	LOG_E("Signal %d received\n", signum);
}

//------------------------------------------------------------------------------
/**
 * Main entry of the MobiCore Driver Daemon.
 */
int main(
    int argc,
    char *args[]
) {
    // Create the MobiCore Driver Singleton
    MobiCoreDriverDaemon *mobiCoreDriverDaemon = NULL;
	// Process signal action
    struct sigaction action;
	
	// Read the Command line options
	extern char *optarg;
	extern int optopt;
	int c, errFlag = 0;
	// Scheduler enabled by default
	int schedulerFlag = 1;
	// Mobicore loading disable by default
	int mobicoreFlag = 0;
	// Autoload driver at start-up
	int driverLoadFlag = 0;
	std::string mobicoreImage, driverPath;
	// Ram donation disabled by default
	int donationSize = 0;
	// By default don't fork
	bool forkDaemon = false;
	while ((c = getopt(argc, args, "m:d:r:sbh")) != -1) {
		switch(c) {
			case 'h': /* Help */
				errFlag++;
				break;
			case 's': /* Disable Scheduler */
				schedulerFlag = 0;
				break;
			case 'd': /* Ram Donation size */
				donationSize = atoi(optarg);
				break;
			case 'm': /* Load mobicore image */
				mobicoreFlag = 1;
				mobicoreImage = optarg;
				break;
			case 'b': /* Fork to background */
				forkDaemon = true;
				break;
			case 'r': /* Load mobicore driver at start-up */
				driverLoadFlag = 1;
				driverPath = optarg;
				break;
			case ':':       /* -d or -m without operand */
				fprintf(stderr, "Option -%c requires an operand\n", optopt);
				errFlag++;
				break;
			case '?':
				fprintf(stderr,
						"Unrecognized option: -%c\n", optopt);
				errFlag++;
		}
	}
	if (errFlag) {
		printUsage(argc, args);
		exit(2);
	}

	// We should fork the daemon to background
	if (forkDaemon == true) {
		int i = fork();
		if (i < 0) {
			exit(1);
		}
		// Parent
		else if (i > 0) {
			exit(0);
		}
	
		// obtain a new process group */
		setsid();
		/* close all descriptors */
		for (i = getdtablesize();i >= 0; --i) {
			close(i);
		}
		// STDIN, STDOUT and STDERR should all point to /dev/null */
		i = open("/dev/null",O_RDWR); 
		dup(i);
		dup(i);
		/* ignore tty signals */
		signal(SIGTSTP,SIG_IGN);
		signal(SIGTTOU,SIG_IGN);
		signal(SIGTTIN,SIG_IGN);
	}

	// Set up the structure to specify the new action.
	action.sa_handler = terminateDaemon;
	sigemptyset (&action.sa_mask);
	action.sa_flags = 0;
	sigaction (SIGINT, &action, NULL);
	sigaction (SIGHUP, &action, NULL);
	sigaction (SIGTERM, &action, NULL);
	signal(SIGPIPE, SIG_IGN);
	
	mobiCoreDriverDaemon = new MobiCoreDriverDaemon(
		/* Scheduler status */
		schedulerFlag,
		/* Mobicore loading to DDR */
		mobicoreFlag,
		mobicoreImage,
		/* Ram Donation */
		donationSize,
		/* Auto Driver loading */
		driverLoadFlag,
		driverPath);

	// Start the driver
	mobiCoreDriverDaemon->run();

	delete mobiCoreDriverDaemon;

	// This should not happen
	LOG_E("Exiting MobiCoreDaemon");

	return EXIT_FAILURE;
}

//------------------------------------------------------------------------------
static void checkMobiCoreVersion(
	MobiCoreDevice* mobiCoreDevice
) {
	bool failed = false;

	// Get MobiCore version info.
	mcDrvRspGetMobiCoreVersionPayload_t versionPayload;
	mobiCoreDevice->getMobiCoreVersion(&versionPayload);

	if (versionPayload.mcResult != MC_MCP_RET_OK) {
		LOG_E("Failed to obtain MobiCore version info. MCP return code: %u", versionPayload.mcResult);
		failed = true;

	} else {
		LOG_I("Product ID is %s", versionPayload.versionInfo.productId);

		// Check MobiCore version info.
		char* msg;
		if (!checkVersionOkMCI(versionPayload.versionInfo.versionMci, &msg)) {
			LOG_E("%s", msg);
			failed = true;
		}
		LOG_I("%s", msg);
		if (!checkVersionOkSO(versionPayload.versionInfo.versionSo, &msg)) {
			LOG_E("%s", msg);
			failed = true;
		}
		LOG_I("%s", msg);
		if (!checkVersionOkMCLF(versionPayload.versionInfo.versionMclf, &msg)) {
			LOG_E("%s", msg);
			failed = true;
		}
		LOG_I("%s", msg);
		if (!checkVersionOkCONTAINER(versionPayload.versionInfo.versionContainer, &msg)) {
			LOG_E("%s", msg);
			failed = true;
		}
		LOG_I("%s", msg);
	}

	if (failed) {
		exit(1);
	}
}

/** @} */
