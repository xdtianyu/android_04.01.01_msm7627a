/** @addtogroup MCD_IMPL_LIB
 * @{
 * @file
 *
 * MobiCore Driver API.
 *
 * Functions for accessing MobiCore functionality from the normal world.
 * Handles sessions and notifications via MCI buffer.
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
#include <stdbool.h>
#include <list>
#ifndef REDUCED_STLPORT
#include <cassert>
#else
#include "assert.h"
#endif

#include "public/MobiCoreDriverApi.h"

#include "mcDrvModuleApi.h"
#include "Connection.h"
#include "CMutex.h"
#include "Device.h"
#include "mcVersionHelper.h"

#include "Daemon/public/MobiCoreDriverCmd.h"
#include "Daemon/public/mcVersion.h"

#define LOG_TAG	"McClient"
#include "log.h"

MC_CHECK_VERSION(DAEMON, 0, 2);

/** Notification data structure. */
typedef struct {
	uint32_t sessionId; /**< Session ID. */
	int32_t payload; /**< Additional notification information. */
} notification_t;

using namespace std;

list<Device*> devices;

// Forward declarations.
static uint32_t getDaemonVersion(Connection* devCon);

//------------------------------------------------------------------------------
static Device *resolveDeviceId(
    uint32_t deviceId
) {
    Device *ret = NULL;

    // Get Session for sessionId
    for (list<Device*>::iterator  iterator = devices.begin();
         iterator != devices.end();
         ++iterator)
    {
        Device  *device = (*iterator);

        if (device->deviceId == deviceId)
        {
            ret = device;
            break;
        }
    }
    return ret;
}


//------------------------------------------------------------------------------
static void addDevice(
    Device *device
) {
    devices.push_back(device);
}


//------------------------------------------------------------------------------
static bool removeDevice(
    uint32_t deviceId
) {
    bool ret = false;

    for (list<Device*>::iterator iterator = devices.begin();
         iterator != devices.end();
         ++iterator)
    {
        Device  *device = (*iterator);

        if (device->deviceId == deviceId)
        {
            devices.erase(iterator);
            delete device;
            ret = true;
            break;
        }
    }
    return ret;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcOpenDevice(
    uint32_t deviceId
) {
    mcResult_t mcResult = MC_DRV_OK;
    static CMutex mutex;
    Connection *devCon = NULL;

    mutex.lock(); // Enter critical section

    do
    {
        Device *device = resolveDeviceId(deviceId);
        if (NULL != device)
        {
            LOG_E("mcOpenDevice(): Device %d already opened", deviceId);
            mcResult = MC_DRV_ERR_INVALID_OPERATION;
            break;
        }

        // Open new connection to device
        devCon = new Connection();
        if (!devCon->connect(SOCK_PATH))
        {
            LOG_E("mcOpenDevice(): Could not connect to %s", SOCK_PATH);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        // Runtime check of Daemon version.
        char* errmsg;
        if (!checkVersionOkDAEMON(getDaemonVersion(devCon), &errmsg)) {
            LOG_E("%s", errmsg);
            mcResult = MC_DRV_ERR_DAEMON_VERSION;
            break;
        }
        LOG_I("%s", errmsg);

        // Forward device open to the daemon and read result
        mcDrvCmdOpenDevice_t mcDrvCmdOpenDevice = {
            // C++ does not support C99 designated initializers
                /* .header = */ {
                    /* .commandId = */ MC_DRV_CMD_OPEN_DEVICE
                },
                /* .payload = */ {
                    /* .deviceId = */ deviceId
                }
            };

        int len = devCon->writeData(
                            &mcDrvCmdOpenDevice,
                            sizeof(mcDrvCmdOpenDevice));
        if (len < 0)
        {
            LOG_E("mcOpenDevice(): CMD_OPEN_DEVICE writeCmd failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        mcDrvResponseHeader_t  rspHeader;
        len = devCon->readData(
                        &rspHeader,
                        sizeof(rspHeader));
        if (len != sizeof(rspHeader))
        {
            LOG_E("mcOpenDevice(): CMD_OPEN_DEVICE readRsp failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }
        if (MC_DRV_RSP_OK != rspHeader.responseId)
        {
            LOG_E("mcOpenDevice(): CMD_OPEN_DEVICE failed, respId=%d", rspHeader.responseId);
            switch(rspHeader.responseId)
            {
            case MC_DRV_RSP_PAYLOAD_LENGTH_ERROR:
            	mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            	break;
            case MC_DRV_INVALID_DEVICE_NAME:
            	mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            	break;
            case MC_DRV_RSP_DEVICE_ALREADY_OPENED:
            default:
            	mcResult = MC_DRV_ERR_INVALID_OPERATION;
            	break;
            }
            break;
        }

        // there is no payload to read

        device = new Device(deviceId, devCon);
        if (!device->open(MC_DRV_MOD_DEVNODE_FULLPATH))
        {
            delete device;
            // devCon is freed in the Device destructor
            devCon = NULL;
            LOG_E("mcOpenDevice(): could not open device file: %s", MC_DRV_MOD_DEVNODE_FULLPATH);
            mcResult = MC_DRV_ERR_INVALID_DEVICE_FILE;
            break;
        }

        addDevice(device);

    } while (false);
    
    if (mcResult != MC_DRV_OK && devCon != NULL)
    {
        delete devCon;
    }

    mutex.unlock(); // Exit critical section

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcCloseDevice(
    uint32_t deviceId
) {
    mcResult_t mcResult = MC_DRV_OK;
    static CMutex mutex;

    mutex.lock(); // Enter critical section
    do
    {
        Device *device = resolveDeviceId(deviceId);
        if (NULL == device)
        {
            LOG_E("mcCloseDevice(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }
        Connection *devCon = device->connection;

        // Return if not all sessions have been closed
        if (device->hasSessions())
        {
            LOG_E("mcCloseDevice(): cannot close with sessions still pending");
            mcResult = MC_DRV_ERR_SESSION_PENDING;
            break;
        }

        mcDrvCmdCloseDevice_t mcDrvCmdCloseDevice = {
            // C++ does not support C99 designated initializers
                /* .header = */ {
                    /* .commandId = */ MC_DRV_CMD_CLOSE_DEVICE
                }
            };
        int len = devCon->writeData(
                    &mcDrvCmdCloseDevice,
                    sizeof(mcDrvCmdCloseDevice));
        // ignore error, but log details
        if (len < 0)
        {
            LOG_E("mcCloseDevice(): CMD_CLOSE_DEVICE writeCmd failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
        }

        mcDrvResponseHeader_t  rspHeader;
        len = devCon->readData(
                        &rspHeader,
                        sizeof(rspHeader));
        if (len != sizeof(rspHeader))
        {
            LOG_E("mcCloseDevice(): CMD_CLOSE_DEVICE readResp failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        if (MC_DRV_RSP_OK != rspHeader.responseId)
        {
            LOG_E("mcCloseDevice(): CMD_CLOSE_DEVICE failed, respId=%d", rspHeader.responseId);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        removeDevice(deviceId);

    } while (false);

    mutex.unlock(); // Exit critical section

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcOpenSession(
    mcSessionHandle_t  *session,
    const mcUuid_t     *uuid,
    uint8_t            *tci,
    uint32_t           len
) {
    mcResult_t mcResult = MC_DRV_OK;
    static CMutex mutex;

    mutex.lock(); // Enter critical section

    do
    {
        if (NULL == session)
        {
            LOG_E("mcOpenSession(): Session is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }
        if (NULL == uuid)
        {
            LOG_E("mcOpenSession(): UUID is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }
        if (NULL == tci)
        {
            LOG_E("mcOpenSession(): TCI is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }
        if (len > MC_MAX_TCI_LEN)
        {
            LOG_E("mcOpenSession(): TCI length is longer than %d", MC_MAX_TCI_LEN);
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        // Get the device associated with the given session
        Device *device = resolveDeviceId(session->deviceId);
        if (NULL == device)
        {
            LOG_E("mcOpenSession(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }
        Connection *devCon = device->connection;

        // Get the physical address of the given TCI
        CWsm_ptr pWsm = device->findContiguousWsm(tci);
        if (NULL == pWsm)
        {
            LOG_E("mcOpenSession(): Could not resolve physical address of TCI");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        if (pWsm->len < len)
        {
            LOG_E("mcOpenSession(): length is more than allocated TCI");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        // Prepare open session command
        mcDrvCmdOpenSession_t cmdOpenSession = {
                // C++ does not support C99 designated initializers
                    /* .header = */ {
                        /* .commandId = */ MC_DRV_CMD_OPEN_SESSION
                    },
                    /* .payload = */ {
                        /* .deviceId = */ session->deviceId,
                        /* .uuid = */ *uuid,
                        /* .tci = */ (uint32_t)pWsm->physAddr,
                        /* .len = */ len
                    }
                };

        // Transmit command data

        int len = devCon->writeData(
                            &cmdOpenSession,
                            sizeof(cmdOpenSession));
        if (sizeof(cmdOpenSession) != len)
        {
            LOG_E("mcOpenSession(): CMD_OPEN_SESSION writeData failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        // Read command response

        // read header first
        mcDrvResponseHeader_t rspHeader;
        len = devCon->readData(
                        &rspHeader,
                        sizeof(rspHeader));
        if (sizeof(rspHeader) != len)
        {
            LOG_E("mcOpenSession(): CMD_OPEN_SESSION readResp failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        if (MC_DRV_RSP_OK != rspHeader.responseId)
        {
            LOG_E("mcOpenSession(): CMD_OPEN_SESSION failed, respId=%d", rspHeader.responseId);
            switch(rspHeader.responseId)
            {
            case MC_DRV_RSP_WRONG_PUBLIC_KEY:
                mcResult = MC_DRV_ERR_WRONG_PUBLIC_KEY;
                break;
            case MC_DRV_RSP_CONTAINER_TYPE_MISMATCH:
                mcResult = MC_DRV_ERR_CONTAINER_TYPE_MISMATCH;
                break;
            case MC_DRV_RSP_CONTAINER_LOCKED:
                mcResult = MC_DRV_ERR_CONTAINER_LOCKED;
                break;
            case MC_DRV_RSP_SP_NO_CHILD:
                mcResult = MC_DRV_ERR_SP_NO_CHILD;
                break;
            case MC_DRV_RSP_TL_NO_CHILD:
                mcResult = MC_DRV_ERR_TL_NO_CHILD;
                break;
            case MC_DRV_RSP_UNWRAP_ROOT_FAILED:
                mcResult = MC_DRV_ERR_UNWRAP_ROOT_FAILED;
                break;
            case MC_DRV_RSP_UNWRAP_SP_FAILED:
                mcResult = MC_DRV_ERR_UNWRAP_SP_FAILED;
                break;
            case MC_DRV_RSP_UNWRAP_TRUSTLET_FAILED:
                mcResult = MC_DRV_ERR_UNWRAP_TRUSTLET_FAILED;
                break;
            case MC_DRV_RSP_TRUSTLET_NOT_FOUND:
            	mcResult = MC_DRV_ERR_INVALID_DEVICE_FILE;
            	break;
            case MC_DRV_RSP_PAYLOAD_LENGTH_ERROR:
            case MC_DRV_RSP_DEVICE_NOT_OPENED:
            case MC_DRV_RSP_FAILED:
            default:
            	mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            	break;
            }
            break;
        }

        // read payload
        mcDrvRspOpenSessionPayload_t rspOpenSessionPayload;
        len = devCon->readData(
                        &rspOpenSessionPayload,
                        sizeof(rspOpenSessionPayload));
        if (sizeof(rspOpenSessionPayload) != len)
        {
            LOG_E("mcOpenSession(): CMD_OPEN_SESSION readPayload failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        // Register session with handle
        session->sessionId = rspOpenSessionPayload.sessionId;

        // Set up second channel for notifications
        Connection *sessionConnection = new Connection();
        if (!sessionConnection->connect(SOCK_PATH))
        {
            LOG_E("mcOpenSession(): Could not connect to %s", SOCK_PATH);
            delete sessionConnection;
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        //TODO CONTINOUE HERE !!!! FIX RW RETURN HANDLING!!!!

        // Write command to use channel for notifications
        mcDrvCmdNqConnect_t cmdNqConnect = {
                // C++ does not support C99 designated initializers
                    /* .header = */ {
                        /* .commandId = */ MC_DRV_CMD_NQ_CONNECT
                    },
                    /* .payload = */ {
                            /* .deviceId =  */ session->deviceId,
                            /* .sessionId = */ session->sessionId,
                            /* .deviceSessionId = */ rspOpenSessionPayload.deviceSessionId,
                            /* .sessionMagic = */ rspOpenSessionPayload.sessionMagic
                    }
                };
        sessionConnection->writeData(
                                &cmdNqConnect,
                                sizeof(cmdNqConnect));


        // Read command response, header first
        len = sessionConnection->readData(
                                    &rspHeader,
                                    sizeof(rspHeader));
        if (sizeof(rspHeader) != len)
        {
            LOG_E("mcOpenSession(): CMD_NQ_CONNECT readRsp failed, ret=%d", len);
            delete sessionConnection;
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        if (MC_DRV_RSP_OK != rspHeader.responseId)
        {
            LOG_E("mcOpenSession(): CMD_NQ_CONNECT failed, respId=%d", rspHeader.responseId);
            delete sessionConnection;
            mcResult = MC_DRV_ERR_NQ_FAILED;
            break;
        }

        // there is no payload.

        // Session has been established, new session object must be created
        device->createNewSession(
                    session->sessionId,
                    sessionConnection);

    } while (false);

    mutex.unlock(); // Exit critical section

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcCloseSession(
    mcSessionHandle_t *session
) {
    mcResult_t mcResult = MC_DRV_OK;
    static CMutex mutex;

    mutex.lock(); // Enter critical section

    do
    {
        if (NULL == session)
        {
            LOG_E("mcCloseSession(): Session is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        Device  *device = resolveDeviceId(session->deviceId);
        if (NULL == device)
        {
            LOG_E("mcCloseSession(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }
        Connection  *devCon = device->connection;

        Session  *nqSession = device->resolveSessionId(session->sessionId);
        if (NULL == nqSession)
        {
            LOG_E("mcCloseSession(): Session not found");
            mcResult = MC_DRV_ERR_UNKNOWN_SESSION;
            break;
        }

        // Write close session command
        mcDrvCmdCloseSession_t cmdCloseSession = {
                // C++ does not support C99 designated initializers
                    /* .header = */ {
                        /* .commandId = */ MC_DRV_CMD_CLOSE_SESSION
                    },
                    /* .payload = */ {
                            /* .sessionId = */ session->sessionId,
                    }
                };
        devCon->writeData(
                            &cmdCloseSession,
                            sizeof(cmdCloseSession));

        // Read command response
        mcDrvResponseHeader_t rspHeader;
        int len = devCon->readData(
                        &rspHeader,
                        sizeof(rspHeader));
        if (sizeof(rspHeader) != len)
        {
            LOG_E("mcCloseSession(): CMD_CLOSE_SESSION readRsp failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        if (MC_DRV_RSP_OK != rspHeader.responseId)
        {
            LOG_E("mcCloseSession(): CMD_CLOSE_SESSION failed, respId=%d", rspHeader.responseId);
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }

        bool r = device->removeSession(session->sessionId);
        assert(r == true);
        mcResult = MC_DRV_OK;

    } while (false);

    mutex.unlock(); // Exit critical section

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcNotify(
    mcSessionHandle_t	*session
) {
    mcResult_t mcResult = MC_DRV_OK;
    
    LOG_I("===%s()===", __func__);

    do
    {
        if (NULL == session)
        {
            LOG_E("mcNotify(): Session is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        Device *device = resolveDeviceId(session->deviceId);
        if (NULL == device)
        {
            LOG_E("mcNotify(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }
        Connection  *devCon = device->connection;

        Session  *nqsession = device->resolveSessionId(session->sessionId);
        if (NULL == nqsession)
        {
            LOG_E("mcNotify(): Session not found");
            mcResult = MC_DRV_ERR_UNKNOWN_SESSION;
            break;
        }

        mcDrvCmdNotify_t cmdNotify = {
                // C++ does not support C99 designated initializers
                    /* .header = */ {
                        /* .commandId = */ MC_DRV_CMD_NOTIFY
                    },
                    /* .payload = */ {
                            /* .sessionId = */ session->sessionId,
                    }
                };

        devCon->writeData(
                    &cmdNotify,
                    sizeof(cmdNotify));

        // Daemon will not return a response

    } while(false);

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcWaitNotification(
    mcSessionHandle_t  *session,
    int32_t            timeout
) {
    mcResult_t mcResult = MC_DRV_OK;
    
    LOG_I("===%s()===", __func__);

    do
    {
        if (NULL == session)
        {
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        Device  *device = resolveDeviceId(session->deviceId);
        if (NULL == device)
        {
            LOG_E("mcWaitNotification(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }

        Session  *nqSession = device->resolveSessionId(session->sessionId);
        if (NULL == nqSession)
        {
            LOG_E("mcWaitNotification(): Session not found");
            mcResult = MC_DRV_ERR_UNKNOWN_SESSION;
            break;
        }

        Connection * nqconnection = nqSession->notificationConnection;
        uint32_t count = 0;

        // Read notification queue till it's empty
        for(;;)
        {
            notification_t notification;
            ssize_t numRead = nqconnection->readData(
                                        &notification,
                                        sizeof(notification_t),
                                        timeout);
            //Exit on timeout in first run
            //Later runs have timeout set to 0. -2 means, there is no more data.
            if (0 == count && -2 == numRead)
            {
                LOG_E("mcWaitNotification(): read timeout");
                mcResult = MC_DRV_ERR_TIMEOUT;
                break;
            }
            // After first notification the queue will be drained, Thus we set
            // no timeout for the following reads
            timeout = 0;

            if (numRead != sizeof(notification_t))
            {
            	if (0 == count)
                {
                	//failure in first read, notify it
                    mcResult = MC_DRV_ERR_NOTIFICATION;
                    LOG_E("mcWaitNotification(): read notification failed, %i bytes received", (int)numRead);
                    break;
                }
            	else
            	{
					// Read of the n-th notification failed/timeout. We don't tell the
					// caller, as we got valid notifications before.
					mcResult = MC_DRV_OK;
					break;
                }
            }

            count++;
            LOG_I("mcWaitNotification(): readNq count=%d, SessionID=%d, Payload=%d",
                   count, notification.sessionId, notification.payload);

            if (0 != notification.payload)
            {
                // Session end point died -> store exit code
                nqSession->setErrorInfo(notification.payload);

                mcResult = MC_DRV_INFO_NOTIFICATION;
                break;
            }
        } // for(;;)

    } while (false);

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcMallocWsm(
    uint32_t	deviceId,
    uint32_t	align,
    uint32_t	len,
    uint8_t		**wsm,
    uint32_t	wsmFlags
) {
    mcResult_t mcResult = MC_DRV_ERR_UNKNOWN;
    static CMutex mutex;

	LOG_I("===%s()===", __func__);

    mutex.lock(); // Enter critical section

    do
    {
        Device *device = resolveDeviceId(deviceId);
        if (NULL == device)
        {
            LOG_E("mcMallocWsm(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }
        if(NULL == wsm)
        {
        	mcResult = MC_DRV_ERR_INVALID_PARAMETER;
        	break;
        }

        CWsm_ptr pWsm =  device->allocateContiguousWsm(len);
        if (NULL == pWsm)
        {
            LOG_E("mcMallocWsm(): Allocation of WSM failed");
            mcResult = MC_DRV_ERR_NO_FREE_MEMORY;
            break;
        }

        *wsm = (uint8_t*)pWsm->virtAddr;
        mcResult = MC_DRV_OK;

    } while (false);

    mutex.unlock(); // Exit critical section

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcFreeWsm(
    uint32_t	deviceId,
    uint8_t		*wsm
) {
    mcResult_t mcResult = MC_DRV_ERR_UNKNOWN;
    Device *device;

    static CMutex mutex;

    LOG_I("===%s()===", __func__);

    mutex.lock(); // Enter critical section

    do {

        // Get the device associated wit the given session
        device = resolveDeviceId(deviceId);
        if (NULL == device)
        {
            LOG_E("mcFreeWsm(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }

        // find WSM object
        CWsm_ptr pWsm = device->findContiguousWsm(wsm);
        if (NULL == pWsm)
        {
            LOG_E("mcFreeWsm(): unknown address");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        // Free the given virtual address
        if (!device->freeContiguousWsm(pWsm))
        {
            LOG_E("mcFreeWsm(): Free of virtual address failed");
            mcResult = MC_DRV_ERR_FREE_MEMORY_FAILED;
            break;
        }
        mcResult = MC_DRV_OK;

    } while (false);

    mutex.unlock(); // Exit critical section

    return mcResult;
}

//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcMap(
    mcSessionHandle_t  *sessionHandle,
    void               *buf,
    uint32_t           bufLen,
    mcBulkMap_t        *mapInfo
) {
    mcResult_t mcResult = MC_DRV_ERR_UNKNOWN;
    static CMutex mutex;

    mutex.lock(); // Enter critical section

    do
    {
        if (NULL == sessionHandle)
        {
            LOG_E("mcMap(): sessionHandle is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }
        if (NULL == mapInfo)
        {
            LOG_E("mcMap(): mapInfo is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }
        if (NULL == buf)
        {
            LOG_E("mcMap(): buf is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        // Determine device the session belongs to
        Device  *device = resolveDeviceId(sessionHandle->deviceId);
        if (NULL == device) {
            LOG_E("mcMap(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }
        Connection *devCon = device->connection;

        // Get session
        Session  *session = device->resolveSessionId(sessionHandle->sessionId);
        if (NULL == session)
        {
            LOG_E("mcMap(): Session not found");
            mcResult = MC_DRV_ERR_UNKNOWN_SESSION;
            break;
        }

		// Workaround Linux memory handling
		if (NULL != buf)
		{
			for (uint32_t i = 0; i < bufLen; i += 4096) {
				volatile uint8_t x = ((uint8_t *) buf)[i]; x = x;
			}
		}

        // Register mapped bulk buffer to Kernel Module and keep mapped bulk buffer in mind
        BulkBufferDescriptor *bulkBuf = session->addBulkBuf(buf, bufLen);
        if (NULL == bulkBuf)
        {
            LOG_E("mcMap(): Error mapping bulk buffer");
            mcResult = MC_DRV_ERR_BULK_MAPPING;
            break;
        }


        // Prepare map command
        mcDrvCmdMapBulkMem_t mcDrvCmdMapBulkMem = {
                // C++ does not support C99 designated initializers
                    /* .header = */ {
                        /* .commandId = */ MC_DRV_CMD_MAP_BULK_BUF
                    },
                    /* .payload = */ {
                        /* .sessionId = */ session->sessionId,
                        /* .pAddrL2 = */ (uint32_t)bulkBuf->physAddrWsmL2,
                        /* .offsetPayload = */ (uint32_t)(bulkBuf->virtAddr) & 0xFFF,
                        /* .lenBulkMem = */ bulkBuf->len
                    }
                };

        // Transmit map command to MobiCore device
        devCon->writeData(
                    &mcDrvCmdMapBulkMem,
                    sizeof(mcDrvCmdMapBulkMem));

        // Read command response
        mcDrvResponseHeader_t rspHeader;
        int len = devCon->readData(
                        &rspHeader,
                        sizeof(rspHeader));
        if (sizeof(rspHeader) != len)
        {
            LOG_E("mcMap(): CMD_MAP_BULK_BUF readRsp failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        if (MC_DRV_RSP_OK != rspHeader.responseId)
        {
            LOG_E("mcMap(): CMD_MAP_BULK_BUF failed, respId=%d", rspHeader.responseId);
            // REV We ignore Daemon Error code because client cannot handle it anyhow.
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;

            // Unregister mapped bulk buffer from Kernel Module and remove mapped
            // bulk buffer from session maintenance
            if (!session->removeBulkBuf(buf))
            {
                // Removing of bulk buffer not possible
                LOG_E("mcMap(): Unregistering of bulk memory from Kernel Module failed");
            }
            break;
        }

        mcDrvRspMapBulkMemPayload_t rspMapBulkMemPayload;
        devCon->readData(
                    &rspMapBulkMemPayload,
                    sizeof(rspMapBulkMemPayload));

        // Set mapping info for Trustlet
        mapInfo->sVirtualAddr = (void *) (rspMapBulkMemPayload.secureVirtualAdr);
        mapInfo->sVirtualLen = bufLen;
        mcResult = MC_DRV_OK;

    } while (false);

    mutex.unlock(); // Exit critical section

    return mcResult;
}

//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcUnmap(
    mcSessionHandle_t  *sessionHandle,
    void               *buf,
    mcBulkMap_t        *mapInfo
) {
    mcResult_t mcResult = MC_DRV_ERR_UNKNOWN;
    static CMutex mutex;

    LOG_I("===%s()===", __func__);

    mutex.lock(); // Enter critical section

    do
    {
        if (NULL == sessionHandle)
        {
            LOG_E("mcUnmap(): sessionHandle is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }
        if (NULL == mapInfo)
        {
            LOG_E("mcUnmap(): mapInfo is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }
        if (NULL == buf)
        {
            LOG_E("mcUnmap(): buf is null");
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        // Determine device the session belongs to
        Device  *device = resolveDeviceId(sessionHandle->deviceId);
        if (NULL == device)
        {
            LOG_E("mcUnmap(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }
        Connection  *devCon = device->connection;

        // Get session
        Session  *session = device->resolveSessionId(sessionHandle->sessionId);
        if (NULL == session)
        {
            LOG_E("mcUnmap(): Session not found");
            mcResult = MC_DRV_ERR_UNKNOWN_SESSION;
            break;
        }

        // Prepare unmap command
        mcDrvCmdUnmapBulkMem_t cmdUnmapBulkMem = {
                // C++ does not support C99 designated initializers
                    /* .header = */ {
                        /* .commandId = */ MC_DRV_CMD_UNMAP_BULK_BUF
                    },
                    /* .payload = */ {
                        /* .sessionId = */ session->sessionId,
                        /* .secureVirtualAdr = */ (uint32_t)(mapInfo->sVirtualAddr),
                        /* .lenBulkMem = mapInfo->sVirtualLen*/
                    }
                };

        devCon->writeData(
                    &cmdUnmapBulkMem,
                    sizeof(cmdUnmapBulkMem));

        // Read command response
        mcDrvResponseHeader_t rspHeader;
        int len = devCon->readData(
                        &rspHeader,
                        sizeof(rspHeader));
        if (sizeof(rspHeader) != len)
        {
            LOG_E("mcUnmap(): CMD_UNMAP_BULK_BUF readRsp failed, ret=%d", len);
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        if (MC_DRV_RSP_OK != rspHeader.responseId)
        {
            LOG_E("mcUnmap(): CMD_UNMAP_BULK_BUF failed, respId=%d", rspHeader.responseId);
            // REV We ignore Daemon Error code because client cannot handle it anyhow.
            mcResult = MC_DRV_ERR_DAEMON_UNREACHABLE;
            break;
        }

        mcDrvRspUnmapBulkMemPayload_t rspUnmapBulkMemPayload;
        devCon->readData(
                    &rspUnmapBulkMemPayload,
                    sizeof(rspUnmapBulkMemPayload));

        // REV axh: what about check the payload?

        // Unregister mapped bulk buffer from Kernel Module and remove mapped
        // bulk buffer from session maintenance
        if (!session->removeBulkBuf(buf))
        {
            // Removing of bulk buffer not possible
            LOG_E("mcUnmap(): Unregistering of bulk memory from Kernel Module failed");
            mcResult = MC_DRV_ERR_BULK_UNMAPPING;
            break;
        }

        mcResult = MC_DRV_OK;

    } while (false);

    mutex.unlock(); // Exit critical section

    return mcResult;
}


//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcGetSessionErrorCode(
    mcSessionHandle_t	*session,
    int32_t				*lastErr
) {
    mcResult_t mcResult = MC_DRV_OK;
    
    LOG_I("===%s()===", __func__);

    do
    {
        if (NULL == session || NULL == lastErr)
        {
            mcResult = MC_DRV_ERR_INVALID_PARAMETER;
            break;
        }

        // Get device
        Device *device = resolveDeviceId(session->deviceId);
        if (NULL == device)
        {
            LOG_E("mcGetSessionErrorCode(): Device not found");
            mcResult = MC_DRV_ERR_UNKNOWN_DEVICE;
            break;
        }

        // Get session
        Session *nqsession = device->resolveSessionId(session->sessionId);
        if (NULL == nqsession)
        {
            LOG_E("mcGetSessionErrorCode(): Session not found");
            mcResult = MC_DRV_ERR_UNKNOWN_SESSION;
            break;
        }

        // get session error code from session
        *lastErr = nqsession->getLastErr();

    } while (false);

    return mcResult;
}

//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcDriverCtrl(
    mcDriverCtrl_t  param,
    uint8_t         *data,
    uint32_t        len
) {
    LOG_W("mcDriverCtrl(): not implemented");
    return MC_DRV_ERR_NOT_IMPLEMENTED;
}

//------------------------------------------------------------------------------
__MC_CLIENT_LIB_API mcResult_t mcGetMobiCoreVersion(
    uint32_t  deviceId,
    mcVersionInfo_t* versionInfo
) {
    mcResult_t mcResult = MC_DRV_OK;

    Device* device = resolveDeviceId(deviceId);
    if (NULL == device) {
        LOG_E("mcGetMobiCoreVersion(): Device not found");
        return MC_DRV_ERR_UNKNOWN_DEVICE;
    }

    if (NULL == versionInfo) {
        return MC_DRV_ERR_INVALID_PARAMETER;
    }

    Connection* devCon = device->connection;

    mcDrvCmdGetMobiCoreVersion_t mcDrvCmdGetMobiCoreVersion = {
        {
            MC_DRV_CMD_GET_MOBICORE_VERSION,
        }
    };
    int len = devCon->writeData(
        &mcDrvCmdGetMobiCoreVersion,
        sizeof(mcDrvCmdGetMobiCoreVersion));

    if (len < 0) {
        LOG_E("mcGetMobiCoreVersion(): MC_DRV_CMD_GET_MOBICORE_VERSION writeCmd failed, ret=%d", len);
        return MC_DRV_ERR_DAEMON_UNREACHABLE;
    }

    // Read GET MOBICORE VERSION response.

    // Read header first.
    mcDrvResponseHeader_t rspHeader;
    len = devCon->readData(&rspHeader, sizeof(rspHeader));
    if (sizeof(rspHeader) != len) {
        LOG_E("mcGetMobiCoreVersion(): MC_DRV_CMD_GET_MOBICORE_VERSION failed to respond, ret=%d", len);
        return MC_DRV_ERR_DAEMON_UNREACHABLE;
    }

    if (MC_DRV_RSP_OK != rspHeader.responseId) {
        LOG_E("mcGetMobiCoreVersion(): MC_DRV_CMD_GET_MOBICORE_VERSION bad response, respId=%d", rspHeader.responseId);
        return MC_DRV_ERR_DAEMON_UNREACHABLE;
    }

    // Read payload.
    mcDrvRspGetMobiCoreVersionPayload_t rspGetMobiCoreVersionPayload;
    len = devCon->readData(&rspGetMobiCoreVersionPayload, sizeof(rspGetMobiCoreVersionPayload));
    if (sizeof(rspGetMobiCoreVersionPayload) != len) {
        LOG_E("mcGetMobiCoreVersion(): MC_DRV_CMD_GET_MOBICORE_VERSION readPayload failed, ret=%d", len);
        return MC_DRV_ERR_DAEMON_UNREACHABLE;
    }

    *versionInfo = rspGetMobiCoreVersionPayload.versionInfo;

    return mcResult;
}


//------------------------------------------------------------------------------
static uint32_t getDaemonVersion(
    Connection* devCon
) {
    assert(devCon != NULL);

    // Send GET VERSION command to daemon.
    mcDrvCmdGetVersion_t cmdGetVersion = {
        {
            MC_DRV_CMD_GET_VERSION,
        },
    };
    int len = devCon->writeData(&cmdGetVersion, sizeof(cmdGetVersion));
    if (sizeof(cmdGetVersion) != len) {
        LOG_E("getDaemonVersion(): MC_DRV_CMD_GET_VERSION failed, ret=%d", len);
        return 0;
    }

    // Read GET VERSION response.

    // Read header first.
    mcDrvResponseHeader_t rspHeader;
    len = devCon->readData(&rspHeader, sizeof(rspHeader));
    if (sizeof(rspHeader) != len) {
        LOG_E("getDaemonVersion(): MC_DRV_CMD_GET_VERSION failed to respond, ret=%d", len);
        return 0;
    }

    if (MC_DRV_RSP_OK != rspHeader.responseId) {
        LOG_E("getDaemonVersion(): MC_DRV_CMD_GET_VERSION bad response, respId=%d", rspHeader.responseId);
        return 0;
    }

    // Read payload.
    mcDrvRspGetVersionPayload_t rspGetVersionPayload;
    len = devCon->readData(&rspGetVersionPayload, sizeof(rspGetVersionPayload));
    if (sizeof(rspGetVersionPayload) != len) {
        LOG_E("getDaemonVersion(): MC_DRV_CMD_GET_VERSION readPayload failed, ret=%d", len);
        return 0;
    }

    return rspGetVersionPayload.version;
}

/** @} */
