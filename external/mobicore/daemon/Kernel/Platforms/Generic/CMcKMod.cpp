/** @addtogroup MCD_MCDIMPL_DAEMON_KERNEL
 * @{
 * @file
 *
 * MobiCore Driver Kernel Module Interface.
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

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <inttypes.h>
#include <cstring>

#include "McTypes.h"
#include "mcDrvModuleApi.h"
#include "mcVersionHelper.h"

#include "CMcKMod.h"

#define LOG_TAG	"McDaemon"
#include "log.h"

//------------------------------------------------------------------------------
MC_CHECK_VERSION(MCDRVMODULEAPI,0,1);

// TODO: rename this to mapWsm
//------------------------------------------------------------------------------
int CMcKMod::mmap(
	uint32_t	len,
	uint32_t	*pHandle,
	addr_t		*pVirtAddr,
	addr_t		*pPhysAddr,
	bool		*pMciReuse
) {
	int ret = 0;
	do
	{
		LOG_I("mmap(): len=%d, mci_reuse=%x", len, *pMciReuse);

		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
	        ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		// TODO: add type parameter to distinguish between non-freeing TCI, MCI and others
		addr_t virtAddr = ::mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED,
				fdKMod, *pMciReuse ? MC_DRV_KMOD_MMAP_MCI
						: MC_DRV_KMOD_MMAP_WSM);
		if (MAP_FAILED == virtAddr)
		{
			LOG_E("mmap() failed with errno: %d", errno);
			ret = ERROR_MAPPING_FAILED;
			break;
		}

		// mapping response data is in the buffer
		struct mc_mmap_resp *pMmapResp = (struct mc_mmap_resp *) virtAddr;

		*pMciReuse = pMmapResp->is_reused;

		LOG_I("mmap(): virtAddr=%p, handle=%d, phys_addr=%p, is_reused=%s",
				virtAddr, pMmapResp->handle, (addr_t) (pMmapResp->phys_addr),
				pMmapResp->is_reused ? "true" : "false");

		if (NULL != pVirtAddr)
		{
			*pVirtAddr = virtAddr;
		}

		if (NULL != pHandle)
		{
			*pHandle = pMmapResp->handle;
		}

		if (NULL != pPhysAddr)
		{
			*pPhysAddr = (addr_t) (pMmapResp->phys_addr);
		}

		// clean memory
		memset(pMmapResp, 0, sizeof(*pMmapResp));

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::mapPersistent(
	uint32_t	len,
	uint32_t	*pHandle,
	addr_t		*pVirtAddr,
	addr_t		*pPhysAddr
) {
	int ret = 0;
	do
	{
		LOG_I("mapPersistent(): len=%d", len);

		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		addr_t virtAddr = ::mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED,
				fdKMod, MC_DRV_KMOD_MMAP_PERSISTENTWSM);

		if (MAP_FAILED == virtAddr)
		{
			LOG_E("mmap() failed with errno: %d", errno);
			ret = ERROR_MAPPING_FAILED;
			break;
		}

		// mapping response data is in the buffer
		struct mc_mmap_resp *pMmapResp = (struct mc_mmap_resp *) virtAddr;

		LOG_I("mapPersistent(): virtAddr=%p, handle=%d, phys_addr=%p, is_reused=%s",
				virtAddr, pMmapResp->handle,
				(addr_t) (pMmapResp->phys_addr),
				pMmapResp->is_reused ? "true" : "false");

		if (NULL != pVirtAddr)
		{
			*pVirtAddr = virtAddr;
		}

		if (NULL != pHandle)
		{
			*pHandle = pMmapResp->handle;
		}

		if (NULL != pPhysAddr)
		{
			*pPhysAddr = (addr_t) (pMmapResp->phys_addr);
		}

		// clean memory
		memset(pMmapResp, 0, sizeof(*pMmapResp));

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::read(
	addr_t		buffer,
	uint32_t	len
) {
	int ret = 0;

	do
	{
		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		ret = ::read(fdKMod, buffer, len);
		if(-1 == ret)
		{
			LOG_E("read() failed with errno: %d", errno);
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
bool CMcKMod::waitSSIQ(
	uint32_t *pCnt
) {
	int ret = true;

	do
	{
		uint32_t cnt;
		int ret = read(&cnt, sizeof(cnt));
		if (sizeof(cnt) != ret)
		{
			ret = false;
		}

		if (NULL != pCnt)
		{
			*pCnt = cnt;
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::fcInit(
	addr_t		mciBuffer,
	uint32_t	nqOffset,
	uint32_t	nqLength,
	uint32_t	mcpOffset,
	uint32_t	mcpLength
) {
	int ret = 0;

	do
	{
		if (!isOpen())
		{
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		// Init MC with NQ and MCP buffer addresses
		union mc_ioctl_init_params fcInitParams = {
		// C++ does not support C99 designated initializers
				/* .in = */{
				/* .base = */(uint32_t) mciBuffer,
				/* .nq_offset = */nqOffset,
				/* .nq_length = */nqLength,
				/* .mcp_offset = */mcpOffset,
				/* .mcp_length = */mcpLength } };
		ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_FC_INIT, &fcInitParams);
		if (ret != 0)
		{
			LOG_E("IOCTL_FC_INIT failed with ret = %d and errno = %d", ret, errno);
			break;
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::fcInfo(
	uint32_t	extInfoId,
	uint32_t	*pState,
	uint32_t	*pExtInfo
) {
	int ret = 0;

	do
	{
		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		// Init MC with NQ and MCP buffer addresses
		union mc_ioctl_info_params fcInfoParams = {
		// C++ does not support C99 designated initializers
				/* .in = */{
				/* .ext_info_id = */extInfoId } };
		ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_FC_INFO, &fcInfoParams);
		if (ret != 0)
		{
			LOG_E("IOCTL_FC_INFO failed with ret = %d and errno = %d", ret, errno);
			break;
		}

		if (NULL != pState)
		{
			*pState = fcInfoParams.out.state;
		}

		if (NULL != pExtInfo)
		{
			*pExtInfo = fcInfoParams.out.ext_info;
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::fcYield(
	void
) {
	int ret = 0;

	do
	{
		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_FC_YIELD, NULL);
		if (ret != 0)
		{
			LOG_E("IOCTL_FC_YIELD failed with ret = %d and errno = %d", ret, errno);
			break;
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::fcNSIQ(
	void
) {
	int ret = 0;

	do
	{
		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_FC_NSIQ, NULL);
		if (ret != 0)
		{
			LOG_E("IOCTL_FC_NSIQ failed with ret = %d and errno = %d", ret, errno);
			break;
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::free(
	uint32_t handle
) {
	int ret = 0;

	do
	{
		LOG_I("free(): handle=%d", handle);

		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		union mc_ioctl_free_params freeParams = {
		// C++ does not support c99 designated initializers
				/* .in = */{
				/* .handle = */(uint32_t) handle } };

		ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_FREE, &freeParams);
		if (0 != ret)
		{
			LOG_E("IOCTL_FREE failed with ret = %d and errno = %d", ret, errno);
			break;
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::registerWsmL2(
	addr_t		buffer,
	uint32_t	len,
	uint32_t	pid,
	uint32_t	*pHandle,
	addr_t		*pPhysWsmL2
) {
	int ret = 0;

	do
	{
		LOG_I("registerWsmL2(): buffer=%p, len=%d, pid=%d", buffer, len, pid);

		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		union mc_ioctl_app_reg_wsm_l2_params params = {
		// C++ does not support C99 designated initializers
				/* .in = */{
				/* .buffer = */(uint32_t) buffer,
				/* .len = */len,
				/* .pid = */pid } };

		ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_APP_REGISTER_WSM_L2, &params);
		if (0 != ret)
		{
			LOG_E("IOCTL_APP_REGISTER_WSM_L2 failed with ret = %d and errno = %d", ret, errno);
			break;
		}

		LOG_I("WSM L2 phys=%x, handle=%d", params.out.phys_wsm_l2_table,
				params.out.handle);

		if (NULL != pHandle)
		{
			*pHandle = params.out.handle;
		}

		if (NULL != pPhysWsmL2)
		{
			*pPhysWsmL2 = (addr_t) params.out.phys_wsm_l2_table;
		}

	} while (0);

	return ret;
}


//------------------------------------------------------------------------------
int CMcKMod::unregisterWsmL2(
	uint32_t handle
) {
	int ret = 0;

	do
	{
		LOG_I("unregisterWsmL2(): handle=%d", handle);

		if (!isOpen())
		{
	        LOG_E("no connection to kmod");
			ret = ERROR_KMOD_NOT_OPEN;
			break;
		}

		struct mc_ioctl_app_unreg_wsm_l2_params params = {
		// C++ does not support c99 designated initializers
				/* .in = */{
				/* .handle = */handle } };

		int ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_APP_UNREGISTER_WSM_L2, &params);
		if (0 != ret)
		{
			LOG_E("IOCTL_APP_UNREGISTER_WSM_L2 failed with ret = %d and errno = %d", ret, errno);
			break;
		}

	} while (0);

	return ret;
}

//------------------------------------------------------------------------------
int CMcKMod::fcExecute(
    addr_t    startAddr,
    uint32_t  areaLength
) {
    int ret = 0;
    union mc_ioctl_fc_execute_params params = {
        /*.in =*/ {
            /*.phys_start_addr = */ (uint32_t)startAddr,
            /*.length = */ areaLength
        }
    };
    do
    {
        if (!isOpen())
        {
            LOG_E("no connection to kmod");
            break;
        }

        ret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_FC_EXECUTE, &params);
        if (ret != 0)
        {
            LOG_E("IOCTL_FC_EXECUTE failed with ret = %d and errno = %d", ret, errno);
            break;
        }

    } while(0);

    return ret;
}
//------------------------------------------------------------------------------
bool CMcKMod::checkKmodVersionOk(
    void
) {
    bool ret = false;

    do
    {
        if (!isOpen())
        {
            LOG_E("no connection to kmod");
            break;
        }

        struct mc_ioctl_get_version_params params;

        int ioret = ioctl(fdKMod, MC_DRV_KMOD_IOCTL_GET_VERSION, &params);
        if (0 != ioret)
        {
            LOG_E("IOCTL_GET_VERSION failed with ret = %d and errno = %d", ret, errno);
            break;
        }

        // Run-time check.
        char* errmsg;
        if (!checkVersionOkMCDRVMODULEAPI(params.out.kernel_module_version, &errmsg)) {
            LOG_E("%s", errmsg);
            break;
        }
        LOG_I("%s", errmsg);

        ret = true;

    } while (0);

    return ret;
}

/** @} */
