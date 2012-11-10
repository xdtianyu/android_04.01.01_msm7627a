/** @addtogroup MCD_IMPL_LIB
 * @{
 * @file
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

#include "Session.h"

#define LOG_TAG	"McClient"
#include "log.h"


//------------------------------------------------------------------------------
Session::Session(
    uint32_t    sessionId,
    CMcKMod     *mcKMod,
    Connection  *connection
) {
    this->sessionId = sessionId;
    this->mcKMod = mcKMod;
    this->notificationConnection = connection;

    sessionInfo.lastErr = SESSION_ERR_NO;
    sessionInfo.state = SESSION_STATE_INITIAL;
}


//------------------------------------------------------------------------------
Session::~Session(
    void
) {
    BulkBufferDescriptor  *pBlkBufDescr;

    // Unmap still mapped buffers
    for ( bulkBufferDescrIterator_t iterator = bulkBufferDescriptors.begin();
          iterator != bulkBufferDescriptors.end();
          ++iterator)
    {
        pBlkBufDescr = *iterator;

        LOG_I("removeBulkBuf - Physical Address of L2 Table = 0x%X, handle= %d",
                (unsigned int)pBlkBufDescr->physAddrWsmL2,
                pBlkBufDescr->handle);

        // ignore any error, as we cannot do anything in this case.
        int ret = mcKMod->unregisterWsmL2(pBlkBufDescr->handle);
        if (0 != ret)
        {
            LOG_E("removeBulkBuf(): mcKModUnregisterWsmL2 failed: %d",ret);
        }

        //iterator = bulkBufferDescriptors.erase(iterator);
        delete(pBlkBufDescr);
    }

    // Finally delete notification connection
    delete notificationConnection;
}


//------------------------------------------------------------------------------
void Session::setErrorInfo(
    int32_t err
) {
    sessionInfo.lastErr = err;
}


//------------------------------------------------------------------------------
int32_t Session::getLastErr(
    void
) {
    return sessionInfo.lastErr;
}


//------------------------------------------------------------------------------
BulkBufferDescriptor* Session::addBulkBuf(
    addr_t		buf,
    uint32_t	len
) {
    BulkBufferDescriptor* blkBufDescr = NULL;

    // Search bulk buffer descriptors for existing vAddr
    // At the moment a virtual address can only be added one time
    for ( bulkBufferDescrIterator_t iterator = bulkBufferDescriptors.begin();
          iterator != bulkBufferDescriptors.end();
          ++iterator
    ) {
        if ((*iterator)->virtAddr == buf)
        {
            return NULL;
        }
    }

    do
    {
        // Prepare the interface structure for memory registration in Kernel Module
        addr_t    pPhysWsmL2;
        uint32_t  handle;

        int ret = mcKMod->registerWsmL2(
                    buf,
                    len,
                    0,
                    &handle,
                    &pPhysWsmL2);

        if (0 != ret) {
            LOG_E("mcKModRegisterWsmL2 failed, ret=%d",ret);
            break;
        }

        LOG_I("addBulkBuf - Physical Address of L2 Table = 0x%X, handle=%d",
                (unsigned int)pPhysWsmL2,
                handle);

        // Create new descriptor
        blkBufDescr = new BulkBufferDescriptor(
                            buf,
                            len,
                            handle,
                            pPhysWsmL2);

        // Add to vector of descriptors
        bulkBufferDescriptors.push_back(blkBufDescr);
    } while(0);

    return blkBufDescr;
}


//------------------------------------------------------------------------------
bool Session::removeBulkBuf(
	addr_t	virtAddr
) {
    bool ret = true;
    BulkBufferDescriptor  *pBlkBufDescr = NULL;

    LOG_I("removeBulkBuf(): Virtual Address = 0x%X", (unsigned int) virtAddr);

    // Search and remove bulk buffer descriptor
    for ( bulkBufferDescrIterator_t iterator = bulkBufferDescriptors.begin();
          iterator != bulkBufferDescriptors.end();
          ++iterator
    ) {

        if ((*iterator)->virtAddr == virtAddr)
        {
            pBlkBufDescr = *iterator;
            iterator = bulkBufferDescriptors.erase(iterator);
            break;
        }
    }

    if (NULL == pBlkBufDescr)
    {
        LOG_E("removeBulkBuf - Virtual Address not found");
        ret = false;
    }
    else
    {
        LOG_I("removeBulkBuf(): WsmL2 phys=0x%X, handle=%d",
        		(unsigned int)pBlkBufDescr->physAddrWsmL2, pBlkBufDescr->handle);

        // ignore any error, as we cannot do anything
        int ret = mcKMod->unregisterWsmL2(pBlkBufDescr->handle);
        if (0 != ret)
        {
            LOG_E("removeBulkBuf(): mcKModUnregisterWsmL2 failed: %d",ret);
        }

        delete (pBlkBufDescr);
    }

    return ret;
}

/** @} */
