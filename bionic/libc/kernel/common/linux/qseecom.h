/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __QSEECOM_H_
#define __QSEECOM_H_
#include <linux/types.h>
#include <linux/ioctl.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MAX_ION_FD 4
#define MAX_APP_NAME_SIZE 32
struct qseecom_register_listener_req {
 uint32_t listener_id;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int32_t ifd_data_fd;
 uint32_t virt_sb_base;
 uint32_t sb_size;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct qseecom_send_cmd_req {
 void *cmd_req_buf;
 unsigned int cmd_req_len;
 void *resp_buf;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 unsigned int resp_len;
};
struct qseecom_ion_fd_info {
 int32_t fd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint32_t cmd_buf_offset;
};
struct qseecom_send_modfd_cmd_req {
 void *cmd_req_buf;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 unsigned int cmd_req_len;
 void *resp_buf;
 unsigned int resp_len;
 struct qseecom_ion_fd_info ifd_data[MAX_ION_FD];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct qseecom_send_resp_req {
 void *resp_buf;
 unsigned int resp_len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct qseecom_load_img_req {
 uint32_t mdt_len;
 uint32_t img_len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 int32_t ifd_data_fd;
 char img_name[MAX_APP_NAME_SIZE];
 int app_id;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct qseecom_set_sb_mem_param_req {
 int32_t ifd_data_fd;
 uint32_t virt_sb_base;
 uint32_t sb_len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct qseecom_qseos_version_req {
 unsigned int qseos_version;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct qseecom_qseos_app_load_query {
 char app_name[MAX_APP_NAME_SIZE];
};
#define QSEECOM_IOC_MAGIC 0x97
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define QSEECOM_IOCTL_REGISTER_LISTENER_REQ   _IOWR(QSEECOM_IOC_MAGIC, 1, struct qseecom_register_listener_req)
#define QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ   _IO(QSEECOM_IOC_MAGIC, 2)
#define QSEECOM_IOCTL_SEND_CMD_REQ   _IOWR(QSEECOM_IOC_MAGIC, 3, struct qseecom_send_cmd_req)
#define QSEECOM_IOCTL_SEND_MODFD_CMD_REQ   _IOWR(QSEECOM_IOC_MAGIC, 4, struct qseecom_send_modfd_cmd_req)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define QSEECOM_IOCTL_RECEIVE_REQ   _IO(QSEECOM_IOC_MAGIC, 5)
#define QSEECOM_IOCTL_SEND_RESP_REQ   _IO(QSEECOM_IOC_MAGIC, 6)
#define QSEECOM_IOCTL_LOAD_APP_REQ   _IOWR(QSEECOM_IOC_MAGIC, 7, struct qseecom_load_img_req)
#define QSEECOM_IOCTL_SET_MEM_PARAM_REQ   _IOWR(QSEECOM_IOC_MAGIC, 8, struct qseecom_set_sb_mem_param_req)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define QSEECOM_IOCTL_UNLOAD_APP_REQ   _IO(QSEECOM_IOC_MAGIC, 9)
#define QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ   _IOWR(QSEECOM_IOC_MAGIC, 10, struct qseecom_qseos_version_req)
#define QSEECOM_IOCTL_PERF_ENABLE_REQ   _IO(QSEECOM_IOC_MAGIC, 11)
#define QSEECOM_IOCTL_PERF_DISABLE_REQ   _IO(QSEECOM_IOC_MAGIC, 12)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ   _IOWR(QSEECOM_IOC_MAGIC, 13, struct qseecom_load_img_req)
#define QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ   _IO(QSEECOM_IOC_MAGIC, 14)
#define QSEECOM_IOCTL_APP_LOADED_QUERY_REQ   _IOWR(QSEECOM_IOC_MAGIC, 15, struct qseecom_qseos_app_load_query)
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */

