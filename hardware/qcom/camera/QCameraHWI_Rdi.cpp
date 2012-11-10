/*
** Copyright (c) 2011-2012 Code Aurora Forum. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*#error uncomment this for compiler test!*/

#define LOG_TAG "QCameraHWI_Rdi"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "QCameraHAL.h"
#include "QCameraHWI.h"
#include <gralloc_priv.h>
#include <genlock.h>

#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

/* QCameraHWI_Raw class implementation goes here*/
/* following code implement the RDI logic of this class*/

namespace android {

// ---------------------------------------------------------------------------
// Rdi Callback
// ---------------------------------------------------------------------------

static void rdi_notify_cb(mm_camera_ch_data_buf_t * frame,
                            void *user_data)
{
  QCameraStream_Rdi *pme = (QCameraStream_Rdi *)user_data;

  if(pme==NULL) {
    ALOGE("%s: X : Incorrect cookie",__func__);
    /*Call buf done*/
    return;
  }

  pme->processRdiFrame(frame);
}

status_t  QCameraStream_Rdi::getBufferRdi( )
{
  int err = 0;
  status_t ret = NO_ERROR;
  int i,  frame_len, y_off, cbcr_off;
  uint8_t num_planes;
  cam_ctrl_dimension_t dim;
  uint32_t planes[VIDEO_MAX_PLANES];

  ALOGI("%s : E ", __FUNCTION__);


  ret = cam_config_get_parm(mCameraId, MM_CAMERA_PARM_DIMENSION, &dim);
  if(ret != NO_ERROR) {
      ALOGE("%s: display format %d is not supported", __func__, dim.prev_format);
    goto end;
  }
  mHalCamCtrl->mRdiMemoryLock.lock();
  mHalCamCtrl->mRdiMemory.buffer_count = kRdiBufferCount;
  if(mHalCamCtrl->isZSLMode()) {
    if(mHalCamCtrl->getZSLQueueDepth() > kRdiBufferCount - 3)
      mHalCamCtrl->mRdiMemory.buffer_count =
      mHalCamCtrl->getZSLQueueDepth() + 3;
  }

  frame_len = mm_camera_get_msm_frame_len(CAMERA_RDI,
                  myMode,
                  dim.rdi0_width,
                  dim.rdi0_height,
                  OUTPUT_TYPE_R,
                  &num_planes, planes);
  y_off = 0;
  cbcr_off = planes[0];

  ALOGE("%s: main image: rotation = %d, yoff = %d, cbcroff = %d, size = %d, width = %d, height = %d",
       __func__, dim.rotation, y_off, cbcr_off, frame_len,
       dim.rdi0_width, dim.rdi0_height);
  if (mHalCamCtrl->initHeapMem(&mHalCamCtrl->mRdiMemory,
     mHalCamCtrl->mRdiMemory.buffer_count,
     frame_len, y_off, cbcr_off, MSM_PMEM_MAINIMG,
     NULL,NULL, num_planes, planes) < 0) {
              ret = NO_MEMORY;
              goto end;
  };


  ALOGI(" %s : X ",__FUNCTION__);
end:
  mHalCamCtrl->mRdiMemoryLock.unlock();

  return NO_ERROR;
}

status_t   QCameraStream_Rdi::freeBufferRdi()
{
  int err = 0;
  status_t ret = NO_ERROR;

  ALOGE(" %s : E ", __FUNCTION__);

  mHalCamCtrl->mRdiMemoryLock.lock();
  for (int cnt = 0; cnt < mHalCamCtrl->mRdiMemory.buffer_count; cnt++) {
      if (cnt < mHalCamCtrl->mRdiMemory.buffer_count) {
          if (NO_ERROR != mHalCamCtrl->sendUnMappingBuf(MSM_V4L2_EXT_CAPTURE_MODE_RDI,
                       cnt, mCameraId, CAM_SOCK_MSG_TYPE_FD_UNMAPPING)) {
              ALOGE("%s: sending data Msg Failed", __func__);
          }
      }
  }
  mHalCamCtrl->releaseHeapMem(&mHalCamCtrl->mRdiMemory);
  memset(&mHalCamCtrl->mRdiMemory, 0, sizeof(mHalCamCtrl->mRdiMemory));
  if (mRdiBuf.def.buf.mp != NULL) {
    delete[] mRdiBuf.def.buf.mp;
    mRdiBuf.def.buf.mp = NULL;
  }

  mHalCamCtrl->mRdiMemoryLock.unlock();
  ALOGI(" %s : X ",__FUNCTION__);
  return NO_ERROR;
}

status_t QCameraStream_Rdi::initRdiBuffers()
{
  status_t ret = NO_ERROR;
  int width = 0;  /* width of channel  */
  int height = 0; /* height of channel */
  uint32_t frame_len = 0; /* frame planner length */
  int buffer_num = 4; /* number of buffers for display */
  const char *pmem_region;
  uint8_t num_planes = 0;
  uint32_t planes[VIDEO_MAX_PLANES];

  cam_ctrl_dimension_t dim;

  ALOGE("%s:BEGIN",__func__);
  mHalCamCtrl->mRdiMemoryLock.lock();
  memset(&mHalCamCtrl->mRdiMemory, 0, sizeof(mHalCamCtrl->mRdiMemory));
  mHalCamCtrl->mRdiMemoryLock.unlock();

/* get rdi size, by qury mm_camera*/
  memset(&dim, 0, sizeof(cam_ctrl_dimension_t));
  ret = cam_config_get_parm(mCameraId, MM_CAMERA_PARM_DIMENSION, &dim);
  if (MM_CAMERA_OK != ret) {
    ALOGE("%s: error - can't get camera dimension!", __func__);
    ALOGE("%s: X", __func__);
    return BAD_VALUE;
  }else {
    width =  dim.rdi0_width;
    height = dim.rdi0_height;
  }

  ret = getBufferRdi( );
  if(ret != NO_ERROR) {
    ALOGE("%s: cannot get memory from heap, ret = %d", __func__, ret);
    return ret;
  }

  /* set 4 buffers for display */
  memset(&mRdiStreamBuf, 0, sizeof(mRdiStreamBuf));
  mHalCamCtrl->mRdiMemoryLock.lock();
  this->mRdiStreamBuf.num = mHalCamCtrl->mRdiMemory.buffer_count;
  this->myMode=myMode; /*Need to assign this in constructor after translating from mask*/

  frame_len = mm_camera_get_msm_frame_len(CAMERA_RDI,
               myMode,
               dim.rdi0_width,
               dim.rdi0_height,
               OUTPUT_TYPE_R,
               &num_planes, planes);

  this->mRdiStreamBuf.frame_len = frame_len;

  memset(&mRdiBuf, 0, sizeof(mRdiBuf));
  mRdiBuf.def.buf.mp = new mm_camera_mp_buf_t[mRdiStreamBuf.num];
  if (!mRdiBuf.def.buf.mp) {
    ALOGE("%s Error allocating memory for mplanar struct ", __func__);
  }
  memset(mRdiBuf.def.buf.mp, 0,
    mRdiStreamBuf.num * sizeof(mm_camera_mp_buf_t));

  /*allocate memory for the buffers*/
  void *vaddr = NULL;
  for(int i = 0; i < mRdiStreamBuf.num; i++){
    if (mHalCamCtrl->mRdiMemory.camera_memory[i] == NULL)
      continue;
    mRdiStreamBuf.frame[i].fd = mHalCamCtrl->mRdiMemory.fd[i];
    mRdiStreamBuf.frame[i].cbcr_off = planes[0];
    mRdiStreamBuf.frame[i].y_off = 0;
    mRdiStreamBuf.frame[i].path = OUTPUT_TYPE_R;
    mRdiStreamBuf.frame[i].buffer =
        (long unsigned int)mHalCamCtrl->mRdiMemory.camera_memory[i]->data;
    mRdiStreamBuf.frame[i].ion_alloc.len = mHalCamCtrl->mRdiMemory.alloc[i].len;

    ALOGE("%s: idx = %d, fd = %d, size = %d, cbcr_offset = %d, y_offset = %d, "
      "vaddr = 0x%x", __func__, i, mRdiStreamBuf.frame[i].fd,
      frame_len,
    mRdiStreamBuf.frame[i].cbcr_off, mRdiStreamBuf.frame[i].y_off,
      (uint32_t)mRdiStreamBuf.frame[i].buffer);

    if (NO_ERROR != mHalCamCtrl->sendMappingBuf(
                      MSM_V4L2_EXT_CAPTURE_MODE_RDI,
                      i,
                      mRdiStreamBuf.frame[i].fd,
                      mHalCamCtrl->mRdiMemory.size,
                      mCameraId, CAM_SOCK_MSG_TYPE_FD_MAPPING)) {
      ALOGE("%s: sending mapping data Msg Failed", __func__);
    }

    mRdiBuf.def.buf.mp[i].frame = mRdiStreamBuf.frame[i];
    mRdiBuf.def.buf.mp[i].frame_offset = mRdiStreamBuf.frame[i].y_off;
    mRdiBuf.def.buf.mp[i].num_planes = num_planes;

    /* Plane 0 needs to be set seperately. Set other planes
     * in a loop. */
    mRdiBuf.def.buf.mp[i].planes[0].length = planes[0];
    mRdiBuf.def.buf.mp[i].planes[0].m.userptr = mRdiStreamBuf.frame[i].fd;
    mRdiBuf.def.buf.mp[i].planes[0].data_offset = 0;
    mRdiBuf.def.buf.mp[i].planes[0].reserved[0] =
      mRdiBuf.def.buf.mp[i].frame_offset;
    for (int j = 1; j < num_planes; j++) {
      mRdiBuf.def.buf.mp[i].planes[j].length = planes[j];
      mRdiBuf.def.buf.mp[i].planes[j].m.userptr =
        mRdiStreamBuf.frame[i].fd;
      mRdiBuf.def.buf.mp[i].planes[j].data_offset = 0;
      mRdiBuf.def.buf.mp[i].planes[j].reserved[0] =
        mRdiBuf.def.buf.mp[i].planes[j-1].reserved[0] +
        mRdiBuf.def.buf.mp[i].planes[j-1].length;
    }

    for (int j = 0; j < num_planes; j++)
      ALOGE("Planes: %d length: %d userptr: %lu offset: %d\n", j,
        mRdiBuf.def.buf.mp[i].planes[j].length,
        mRdiBuf.def.buf.mp[i].planes[j].m.userptr,
        mRdiBuf.def.buf.mp[i].planes[j].reserved[0]);
  }/*end of for loop*/

 /* register the streaming buffers for the channel*/
  mRdiBuf.ch_type = MM_CAMERA_CH_RDI;
  mRdiBuf.def.num = mRdiStreamBuf.num;
  mHalCamCtrl->mRdiMemoryLock.unlock();
  ALOGE("%s:END",__func__);
  return NO_ERROR;

end:
  if (MM_CAMERA_OK == ret ) {
    ALOGV("%s: X - NO_ERROR ", __func__);
    return NO_ERROR;
  }

    ALOGV("%s: out of memory clean up", __func__);
  /* release the allocated memory */

  ALOGV("%s: X - BAD_VALUE ", __func__);
  return BAD_VALUE;
}

void QCameraStream_Rdi::dumpFrameToFile(struct msm_frame* newFrame)
{
  int32_t enabled = 0;
  int frm_num;
  uint32_t  skip_mode;
  char value[PROPERTY_VALUE_MAX];
  char buf[32];
  int w, h;
  static int count = 0;
  cam_ctrl_dimension_t dim;
  int file_fd;
  int rc = 0;
  int len;
  unsigned long addr;
  unsigned long * tmp = (unsigned long *)newFrame->buffer;
  addr = *tmp;
  status_t ret = cam_config_get_parm(mHalCamCtrl->mCameraId,
                 MM_CAMERA_PARM_DIMENSION, &dim);

  w = dim.rdi0_width;
  h = dim.rdi0_height;
  len = (w * h)*3/2;
  count++;
  if(count < 100) {
    snprintf(buf, sizeof(buf), "/data/%d.yuv", count);
    file_fd = open(buf, O_RDWR | O_CREAT, 0777);

    rc = write(file_fd, (const void *)addr, len);
    ALOGE("%s: file='%s', vaddr_old=0x%x, addr_map = 0x%p, len = %d, rc = %d",
          __func__, buf, (uint32_t)newFrame->buffer, (void *)addr, len, rc);
    close(file_fd);
    ALOGE("%s: dump %s, rc = %d, len = %d", __func__, buf, rc, len);
  }
}

status_t QCameraStream_Rdi::processRdiFrame(
  mm_camera_ch_data_buf_t *frame)
{
  ALOGV("%s",__func__);
  int err = 0;
  int msgType = 0;
  int i;
  camera_memory_t *data = NULL;

  Mutex::Autolock lock(mStopCallbackLock);
  if(!mActive) {
    ALOGE("RDI Streaming Stopped. Returning callback");
    return NO_ERROR;
  }
  if(mHalCamCtrl==NULL) {
    ALOGE("%s: X: HAL control object not set",__func__);
    /*Call buf done*/
    return BAD_VALUE;
  }

  mHalCamCtrl->mRdiMemoryLock.lock();
  mNotifyBuffer[frame->def.idx] = *frame;
  mHalCamCtrl->mRdiMemoryLock.unlock();

  mHalCamCtrl->mCallbackLock.lock();
  camera_data_callback pcb = mHalCamCtrl->mDataCb;
  mHalCamCtrl->mCallbackLock.unlock();
  ALOGD("Message enabled = 0x%x", mHalCamCtrl->mMsgEnabled);

  mHalCamCtrl->dumpFrameToFile(frame->def.frame, HAL_DUMP_FRM_RDI);

#ifdef USE_ION
  struct ion_flush_data cache_inv_data;
  int ion_fd;

  cache_inv_data.vaddr = (void *)frame->def.frame->buffer;
  cache_inv_data.fd = frame->def.frame->fd;
  cache_inv_data.handle = frame->def.frame->fd_data.handle;
  cache_inv_data.length = frame->def.frame->ion_alloc.len;
  ion_fd = frame->def.frame->ion_dev_fd;

  if (mHalCamCtrl->cache_ops(ion_fd, &cache_inv_data, ION_IOC_CLEAN_CACHES) < 0)
    ALOGE("%s: Cache clean for RDI buffer %p fd = %d failed", __func__,
      cache_inv_data.vaddr, cache_inv_data.fd);
#endif

  if (pcb != NULL) {
      //Sending rdi callback if corresponding Msgs are enabled
      if(mHalCamCtrl->mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
          msgType |=  CAMERA_MSG_PREVIEW_FRAME;
        data = mHalCamCtrl->mRdiMemory.camera_memory[frame->def.idx];
      } else {
          data = NULL;
      }

      if(msgType) {
          mStopCallbackLock.unlock();
          if(mActive)
            pcb(msgType, data, 0, NULL, mHalCamCtrl->mCallbackCookie);
      }
      ALOGD("end of cb");
  }
  if(MM_CAMERA_OK != cam_evt_buf_done(mCameraId, &mNotifyBuffer[frame->def.idx])) {
          ALOGE("BUF DONE FAILED");
  }
  return NO_ERROR;
}


// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------

QCameraStream_Rdi::
QCameraStream_Rdi(int cameraId, camera_mode_t mode)
  : QCameraStream(cameraId,mode),
    mNumFDRcvd(0)
  {
    mHalCamCtrl = NULL;
    ALOGE("%s: E", __func__);
    ALOGE("%s: X", __func__);
  }
// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------

QCameraStream_Rdi::~QCameraStream_Rdi() {
    ALOGV("%s: E", __func__);
	if(mActive) {
		stop();
	}
	if(mInit) {
		release();
	}
	mInit = false;
	mActive = false;
    ALOGV("%s: X", __func__);

}
// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------

status_t QCameraStream_Rdi::init() {

  status_t ret = NO_ERROR;
  ALOGV("%s: E", __func__);

  ret = QCameraStream::initChannel (mCameraId, MM_CAMERA_CH_RDI_MASK);
  if (NO_ERROR!=ret) {
    ALOGE("%s E: can't init rdi ch\n",__func__);
    return ret;
  }

  /* register a notify into the mmmm_camera_t object*/
  (void) cam_evt_register_buf_notify(mCameraId, MM_CAMERA_CH_RDI,
                                      rdi_notify_cb,
                                      MM_CAMERA_REG_BUF_CB_INFINITE,
                                      0,this);

  buffer_handle_t *buffer_handle = NULL;
  int tmp_stride = 0;
  mInit = true;
  return ret;
}
// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------

status_t QCameraStream_Rdi::start()
{
    ALOGV("%s: E", __func__);
    status_t ret = NO_ERROR;
    uint32_t stream_info;

    Mutex::Autolock lock(mStopCallbackLock);

    /* call start() in parent class to start the monitor thread*/
    //QCameraStream::start ();
    stream_info = mHalCamCtrl->getChannelInterface();

    setFormat(MM_CAMERA_CH_RDI_MASK, (cam_format_t)0);

    initRdiBuffers();
    ret = cam_config_prepare_buf(mCameraId, &mRdiBuf);
    if(ret != MM_CAMERA_OK) {
      ret = BAD_VALUE;
    } else
      ret = NO_ERROR;

	/* For preview, the OP_MODE we set is dependent upon whether we are
       starting camera or camcorder. For snapshot, anyway we disable preview.
       However, for ZSL we need to set OP_MODE to OP_MODE_ZSL and not
       OP_MODE_VIDEO. We'll set that for now in CamCtrl. So in case of
       ZSL we skip setting Mode here */

    if (!(myMode & CAMERA_ZSL_MODE)) {
        ALOGE("Setting OP MODE to MM_CAMERA_OP_MODE_VIDEO");
        mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_VIDEO;
        ret = cam_config_set_parm (mCameraId, MM_CAMERA_PARM_OP_MODE,
                                        &op_mode);
        ALOGE("OP Mode Set");

        if(MM_CAMERA_OK != ret) {
          ALOGE("%s: X :set mode MM_CAMERA_OP_MODE_VIDEO err=%d\n", __func__, ret);
          ret = BAD_VALUE;
          goto error;
        }
    }else {
        ALOGE("Setting OP MODE to MM_CAMERA_OP_MODE_ZSL");
        mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_ZSL;
        ret = cam_config_set_parm (mCameraId, MM_CAMERA_PARM_OP_MODE,
                                        &op_mode);
        if(MM_CAMERA_OK != ret) {
          ALOGE("%s: X :set mode MM_CAMERA_OP_MODE_ZSL err=%d\n", __func__, ret);
          ret = BAD_VALUE;
          goto error;
        }
     }

    /* call mm_camera action start(...)  */

     ALOGE("Starting RDI Stream. ");
     ret = cam_ops_action(mCameraId, TRUE, MM_CAMERA_OPS_RDI, 0);
     if (MM_CAMERA_OK != ret) {
       ALOGE ("%s: rdi streaming start err=%d\n", __func__, ret);
        ret = BAD_VALUE;
        freeBufferRdi();
        goto end;
     }

     ret = NO_ERROR;

     mActive =  true;
     goto end;

error:
   freeBufferRdi();
end:
    ALOGE("%s: X", __func__);
    return ret;
  }


// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------
  void QCameraStream_Rdi::stop() {
    ALOGE("%s: E", __func__);
    int ret=MM_CAMERA_OK;
    uint32_t stream_info;

    if(!mActive) {
      return;
    }
    Mutex::Autolock lock(mStopCallbackLock);
    mActive =  false;
    /* unregister the notify fn from the mmmm_camera_t object*/

    ALOGI("%s: Stop the thread \n", __func__);
    /* call stop() in parent class to stop the monitor thread*/
    stream_info = mHalCamCtrl->getChannelInterface();

    ret = cam_ops_action(mCameraId, FALSE, MM_CAMERA_OPS_RDI, 0);
    if(MM_CAMERA_OK != ret) {
      ALOGE ("%s: camera rdi stop err=%d\n", __func__, ret);
    }
    ret = cam_config_unprepare_buf(mCameraId, MM_CAMERA_CH_RDI);
    if(ret != MM_CAMERA_OK) {
      ALOGE("%s:Unreg rdi buf err=%d\n", __func__, ret);
    }

    /* In case of a clean stop, we need to clean all buffers*/
    /*free camera_memory handles and return buffer back to surface*/
    freeBufferRdi();

    ALOGE("%s: X", __func__);

  }
// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------
  void QCameraStream_Rdi::release() {

    ALOGE("%s : BEGIN",__func__);
    int ret=MM_CAMERA_OK,i;

    if(!mInit)
    {
      ALOGE("%s : Stream not Initalized",__func__);
      return;
    }

    if(mActive) {
      this->stop();
    }

    ret= QCameraStream::deinitChannel(mCameraId, MM_CAMERA_CH_RDI);
    ALOGE(": %s : De init Channel",__func__);
    if(ret != MM_CAMERA_OK) {
      ALOGE("%s:Deinit preview channel failed=%d\n", __func__, ret);
      //ret = BAD_VALUE;
    }

    (void)cam_evt_register_buf_notify(mCameraId, MM_CAMERA_CH_RDI,
                                      NULL,
                                      (mm_camera_register_buf_cb_type_t)NULL,
                                      NULL,
                                      NULL);
    mInit = false;
    ALOGE("%s: END", __func__);

  }

QCameraStream*
QCameraStream_Rdi::createInstance(int cameraId,
                                      camera_mode_t mode)
{
  QCameraStream* pme = new QCameraStream_Rdi(cameraId, mode);
  return pme;
}
// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------

void QCameraStream_Rdi::deleteInstance(QCameraStream *p)
{
  if (p){
    ALOGV("%s: BEGIN", __func__);
    p->release();
    delete p;
    p = NULL;
    ALOGV("%s: END", __func__);
  }
}

// ---------------------------------------------------------------------------
// No code beyone this line
// ---------------------------------------------------------------------------
}; // namespace android
