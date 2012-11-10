/*
Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
  @file wlan_btc_usr_svc.c

  @brief This module implements the Bluetooth coexistence service

  Bluetooth Coexistence (BTC) aims at minimizing the interference between the
  Bluetooth Radio and the WLAN Radio by employing coexistence schemes. BTC
  Services provides a transport/notification mechanism to deliver the BT events
  from the BT protocol stack to the WLAN protocol stack and WLAN information
  from WLAN stack to Bluetooth stack.

  This implementation is specific to Android and BlueZ Bluetooth Stack.
*/

/*===========================================================================

                       EDIT HISTORY FOR FILE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order. Please
  use ISO format for dates.

  when        who  what, where, why
  ----------  ---  -----------------------------------------------------------
  2011-08-06   ss  To resolve issue BTC event not coming after WLAN OFF and ON,
                   Added a loop to wait till the wlan.driver.status property updates
                   to corect value with 4 seconds time-out.
  2011-06-14   ss  To resolve issue BTC event not coming in SoftAP mode due to the
                   change of the interfase name, Added a way to check is wlan driver
                   already loaded by checking the module name 'wlan' rather than
                   checking through the interface name(wlan0 or softAp.0).
  2011-03-08   rr  Ensure WLAN driver is successful loaded before sending out
                   "Query WLAN" message.
  2010-06-01  tam  Let WLAN driver close before checking for it again
  2010-04-26  tam  Make the use of recv() non-blocking to allow BTC shut down
  2010-04-15  tam  Correct WLAN channel mask passed to BTC-ES
  2010-03-03   pj  Initial Open Source version

===========================================================================*/

/*---------------------------------------------------------------------------
 * Include Files
 *-------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <wlan_nlink_common.h>
#include <wlan_btc_usr_svc.h>

#include "cutils/properties.h"

//Time to wait for WLAN driver to initialize.
//FIXME This value needs to optimized further
#define BTC_SVC_WLAN_SETTLE_TIME 1200000
#define BTC_SVC_SOCKET_CREATE_DELAY (3000)

//Time for WLAN driver to be unloaded after getting BTC_WLAN_IF_DOWN
#define BTC_SVC_WLAN_DOWN_SLEEP_TIME_USEC (200000)
#define BTC_SVC_WLAN_DOWN_WAIT_TIME_SEC (4)

//Time for WLAN driver to be loaded after getting and setting android property correctly.
#define BTC_SVC_WLAN_UP_SLEEP_TIME_USEC (200000)
#define BTC_SVC_WLAN_UP_WAIT_TIME_SEC (4)

#ifdef BTC_DEBUG

#ifdef __BTC_USE_FPRINTF__

#define BTC_INFO(args...) fprintf(stdout, ## args);
#define BTC_ERR(args...)  fprintf(stderr, ## args);
#define BTC_OS_ERR        fprintf(stderr, "err =%d msg = %s\n", errno, strerror(errno))

#else  // __BTC_USE_FPRINTF__

// needed by LOG functions
// LOG_NDEBUG = 1 disables all logging, 0 enables
// Set others to 1 to disable, 0 to enable (LOG_NDDEBUG: debug logs, LOG_NIDEBUG: info logs)
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0
#define LOG_TAG "BTC-SVC"
#include "cutils/log.h"
#define BTC_INFO(...)      LOGI(__VA_ARGS__)
#define BTC_ERR(...)       LOGE(__VA_ARGS__)
#define BTC_OS_ERR         LOGE("err =%d msg = %s", errno, strerror(errno))

#endif  // __BTC_USE_FPRINTF__

#else  // BTC_DEBUG

#define BTC_INFO(args...)
#define BTC_ERR(args...)
#define BTC_OS_ERR

#endif  // BTC_DEBUG

/*---------------------------------------------------------------------------
 * Global Data Definitions
 *-------------------------------------------------------------------------*/

// Event data for WLAN_BTC_BT_EVENT_IND
typedef struct
{
   btces_event_enum   ev;
   btces_event_data_union  u;
} tBtcBtEvent;

typedef enum
{
   BTC_SVC_UNREGISTERED = 0,
   BTC_SVC_REGISTERED
} eBtcSvcState;

typedef enum
{
  BTC_SUCCESS = 0,
  BTC_FAILURE,
  BTC_WLAN_IF_FOUND,
  BTC_WLAN_IF_DOWN,
}eBtcStatus;

typedef struct
{
   /* BTC Service state */
   eBtcSvcState btcSvcState;

   /* BTC-ES register/deregister function pointers */
   btces_funcs  btcEsFuncs;

   /* Handle to the thread that montiors the WLAN interface */
   pthread_t workerThread;

   /* Handle to the netlink socket */
   int fd;

   /* Whether thread should shutdown */
   int shutdown;

   /* Pipe used to signal shutdown */
   int pd[2];

} tBtcSvcHandle;


//Global handle to the BTC SVC
tBtcSvcHandle *gpBtcSvc = NULL;

#ifndef WIFI_DRIVER_MODULE_NAME
#define WIFI_DRIVER_MODULE_NAME         "wlan"
#endif
static const char MODULE_FILE[]         = "/proc/modules";
static const char DRIVER_MODULE_TAG[]   = WIFI_DRIVER_MODULE_NAME " ";
static const char DRIVER_PROP_NAME[]    = "wlan.driver.status";

/*---------------------------------------------------------------------------
 * Function prototypes
 *-------------------------------------------------------------------------*/

void btc_svc_inject_bt_event (btces_event_enum bt_event,
                              btces_event_data_union *event_data,
                              void *user_data);

static inline eBtcStatus register_btc(tBtcSvcHandle *pBtcSvcHandle)
{
   if(pBtcSvcHandle->btcSvcState != BTC_SVC_REGISTERED) {
      if(pBtcSvcHandle->btcEsFuncs.register_func(btc_svc_inject_bt_event, pBtcSvcHandle))
      {
        BTC_ERR("BTC-SVC: Registration with BTC-ES failed\n");
        return BTC_FAILURE;
      }
      pBtcSvcHandle->btcSvcState = BTC_SVC_REGISTERED;
   }
   return BTC_SUCCESS;
}

static inline eBtcStatus unregister_btc(tBtcSvcHandle *pBtcSvcHandle)
{
   void *pBtc;

   if(pBtcSvcHandle->btcSvcState != BTC_SVC_UNREGISTERED) {
      if(pBtcSvcHandle->btcEsFuncs.deregister_func(&pBtc))
      {
        BTC_ERR("BTC-SVC: Deregistration with BTC-ES failed\n");
        //If Deregistration fails, log an error and continue anyway. Most likely
        //the module is already deregistered.
      }
      pBtcSvcHandle->btcSvcState = BTC_SVC_UNREGISTERED;
   }
   return BTC_SUCCESS;
}

struct udev_event {
    const char *event;
    const char *system;
    const char *fw;
};

#define ACTION_STR      "ACTION="
#define DEVPATH_STR     "DEVPATH="
#define SUBSYS_STR      "SUBSYSTEM="
#define FW_STR          "FIRMWARE="
#define MAJOR_STR       "MAJOR="
#define MINOR_STR       "MINOR="
#define ACTION_STR_LEN      7
#define DEVPATH_STR_LEN     8
#define SUBSYS_STR_LEN      10
#define FW_STR_LEN          9
#define MAJOR_MINOR_STR_LEN 6

static void parse_udev_event_message(const char *message, struct udev_event *udev_event)
{
    while(*message)
    {
        if(!strncmp(message, ACTION_STR, ACTION_STR_LEN))
        {
            message += ACTION_STR_LEN;
            udev_event->event = message;
        }
        else if(!strncmp(message, DEVPATH_STR, DEVPATH_STR_LEN))
        {
            message += DEVPATH_STR_LEN;
        }
        else if(!strncmp(message, SUBSYS_STR, SUBSYS_STR_LEN)) {
            message += SUBSYS_STR_LEN;
            udev_event->system = message;
        }
        else if(!strncmp(message, FW_STR, FW_STR_LEN))
        {
            message += FW_STR_LEN;
            udev_event->fw = message;
        }
        else if(!strncmp(message, MAJOR_STR, MAJOR_MINOR_STR_LEN) ||
                !strncmp(message, MINOR_STR, MAJOR_MINOR_STR_LEN))
        {
            message += MAJOR_MINOR_STR_LEN;
        }

        while(*message++);
    }

    BTC_INFO("BTC-SVC: Uevent { '%s', '%s', '%s' }\n",
       udev_event->event, udev_event->system, udev_event->fw);
}

//Maximum length of message string in udev event
#define UEVENT_MESSAGE_LENGTH  1024

static eBtcStatus process_udev_event( int fd )
{
    char msg[UEVENT_MESSAGE_LENGTH+1];
    int bytes;
    struct udev_event udev_event;

    /* Read each udev event until no more msgs pending, or until WLAN is found */
    while((bytes = recv(fd, msg, UEVENT_MESSAGE_LENGTH, MSG_DONTWAIT)) > 0) {

        if(bytes > UEVENT_MESSAGE_LENGTH)
            continue;

        msg[bytes] = '\0';

        udev_event.event = "";
        udev_event.system = "";
        udev_event.fw = "";
        parse_udev_event_message(msg, &udev_event);

        if(strcmp(udev_event.system, "firmware") == 0 &&
           strcmp(udev_event.event, "add") == 0 &&
           strstr(udev_event.fw, "qcom_fw.bin") != NULL)
        {
              return BTC_WLAN_IF_FOUND;
        }
   }

   return BTC_FAILURE;
}

static eBtcStatus __select(int fd, tBtcSvcHandle *pBtcSvcHandle)
{
   int max_fd, ret;
   fd_set read_fds;

   max_fd = (fd > pBtcSvcHandle->pd[0]) ? fd : pBtcSvcHandle->pd[0];

   while (1) {
      FD_ZERO(&read_fds);
      FD_SET(fd, &read_fds);
      FD_SET(pBtcSvcHandle->pd[0], &read_fds);
      ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

      if(ret < 0) {
         if(errno == EAGAIN || errno == EINTR) {
            BTC_ERR("BTC-SVC: Interrupted by signal - ignoring\n");
            BTC_OS_ERR;
            continue;
         }
         else {
            BTC_ERR("BTC-SVC: Unhandled error - thread exiting\n");
            BTC_OS_ERR;
            break;
         }
      }

      if(ret == 0) {
         continue;
      }

      if(FD_ISSET(pBtcSvcHandle->pd[0], &read_fds)) {
         BTC_INFO("BTC-SVC: Thread signaled to shutdown\n");
         break;
      }

      if(FD_ISSET(fd, &read_fds))
          return BTC_SUCCESS;
   }

   return BTC_FAILURE;
}

static eBtcStatus monitor_udev_event(tBtcSvcHandle *pBtcSvc)
{
    struct sockaddr_nl addr;
    int sz = 64*1024;
    int fd;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(fd < 0)
        return BTC_FAILURE;

    setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

    if(bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(fd);
        return BTC_FAILURE;
    }

    while (1) {
       if(__select(fd, pBtcSvc) == BTC_SUCCESS) {
          if (process_udev_event(fd) == BTC_WLAN_IF_FOUND) {
             close(fd);
             return BTC_WLAN_IF_FOUND;
          }
          else
             continue;
       }
       else
          break;
    }

   close(fd);
   return BTC_FAILURE;
}

/* Check if WLAN driver is already loaded or not by checking the property status
 * and DRIVER_MODULE_TAG in the MODULE_FILE file.
 */
static int check_driver_loaded() {
    char driver_status[PROPERTY_VALUE_MAX];
    FILE *proc;
    char line[sizeof(DRIVER_MODULE_TAG)+1];
    int wait;

    /* To Check if property is not set at all */
    if(!property_get(DRIVER_PROP_NAME, driver_status, NULL))
    {
        BTC_INFO("BTC-SVC: Wlan driver not loaded as the Android property is not set\n");
        return BTC_FAILURE;
    }

    /* Wait up to 4 seconds to allow WLAN driver to load and set the property status */
    wait = 0;
    while((wait++ < (BTC_SVC_WLAN_UP_WAIT_TIME_SEC * 1000000 / BTC_SVC_WLAN_UP_SLEEP_TIME_USEC))
          && (!property_get(DRIVER_PROP_NAME, driver_status, NULL)
          || strcmp(driver_status, "loading") == 0)) {

            BTC_INFO("BTC-SVC: WLAN driver is still loading: %d", wait);
            usleep(BTC_SVC_WLAN_UP_SLEEP_TIME_USEC);
    }

    if (!property_get(DRIVER_PROP_NAME, driver_status, NULL)
            || strcmp(driver_status, "ok") != 0) {
        BTC_INFO("Wlan driver not loaded as per Android property");
        return BTC_FAILURE;  /* driver not loaded */
    }
    /*
     * If the property says the driver is loaded, check to
     * make sure that the property setting isn't just left
     * over from a previous manual shutdown or a runtime
     * crash.
     */
    if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
        BTC_ERR("Could not open %s: %s", MODULE_FILE, strerror(errno));
        return BTC_FAILURE;
    }
    while ((fgets(line, sizeof(line), proc)) != NULL) {
        if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
            fclose(proc);
            BTC_INFO("Wlan driver already loaded");
            return BTC_SUCCESS;
        }
    }
    fclose(proc);
    BTC_INFO("Wlan driver not loaded");
    return BTC_FAILURE;
}

/* Check if WLAN device is there or not. This call would basically
 * block this thread from proceeeding further until WLAN module is
 * loaded (insmoded)
 */
static eBtcStatus check_wlan_present(tBtcSvcHandle *pBtcSvcHandle) {

   if (check_driver_loaded() == BTC_SUCCESS) {
        return BTC_SUCCESS;
   }

   if(monitor_udev_event(pBtcSvcHandle) == BTC_WLAN_IF_FOUND)
    {
       // Sleep for sometime. Allow driver to initialize
       usleep(BTC_SVC_WLAN_SETTLE_TIME);
       return BTC_SUCCESS;
    }

   // We would get here only if thread was shutdown while we
   // were detecting WLAN interface
   return BTC_FAILURE;
}

/*--------------------------------------------------------------------------
  \brief btc_svc_inject_bt_event() - Call back function registered with BTC-ES

  \param  bt_event   Type of Bluetooth Event

  \param  event_data Event data associated with the BT event

  \param  user_data  opaque user data (cookie)

  \return none

  \sa
  --------------------------------------------------------------------------*/
void btc_svc_inject_bt_event (btces_event_enum bt_event,
                              btces_event_data_union *event_data,
                              void *user_data)
{
   tBtcSvcHandle *pBtcSvcHandle = (tBtcSvcHandle*)user_data;
   struct sockaddr_nl dest_addr;
   unsigned char buffer[WLAN_NL_MAX_PAYLOAD];
   struct nlmsghdr *nl_header = NULL;
   tAniMsgHdr *msgHdr = NULL;
   tBtcBtEvent *btEvent = NULL;;

   switch (bt_event)
   {
      case BTCES_EVENT_DEVICE_SWITCHED_ON :
      case BTCES_EVENT_DEVICE_SWITCHED_OFF :
      case BTCES_EVENT_INQUIRY_STARTED :
      case BTCES_EVENT_INQUIRY_STOPPED :
      case BTCES_EVENT_PAGE_STARTED :
      case BTCES_EVENT_PAGE_STOPPED :
      case BTCES_EVENT_CREATE_ACL_CONNECTION :   /* btces_bt_addr_struct */
      case BTCES_EVENT_ACL_CONNECTION_COMPLETE : /* btces_event_data_acl_comp_struct */
      case BTCES_EVENT_CREATE_SYNC_CONNECTION :  /* btces_bt_addr_struct */
      case BTCES_EVENT_SYNC_CONNECTION_COMPLETE :/* btces_event_data_sync_comp_up_struct */
      case BTCES_EVENT_SYNC_CONNECTION_UPDATED : /* btces_event_data_sync_comp_up_struct */
      case BTCES_EVENT_DISCONNECTION_COMPLETE :  /* btces_event_data_disc_comp_struct */
      case BTCES_EVENT_MODE_CHANGED :            /* btces_event_data_mode_struct */
      case BTCES_EVENT_A2DP_STREAM_START :       /* btces_bt_addr_struct */
      case BTCES_EVENT_A2DP_STREAM_STOP :        /* btces_bt_addr_struct */
         // Received valid event. Forward the event to WLAN driver via netlink socket
         break;

      default :
         // Not a valid supported event
         BTC_ERR("BTC-SVC: Unknown BT Event %d from BTC-ES\n", bt_event);
         return;
   }

   /* Prepare to send a WLAN_BTC_SIGNAL_EVENT_IND message to the kernel
      nl_pid = 0;   implies message is for Linux Kernel
      nl_groups = 0; implies message is unicast
   */
   memset(&dest_addr, 0, sizeof(dest_addr));
   dest_addr.nl_family = AF_NETLINK;

   nl_header = (struct nlmsghdr *)buffer;
   nl_header->nlmsg_type = WLAN_NL_MSG_BTC;
   nl_header->nlmsg_flags = NLM_F_REQUEST;
   nl_header->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + sizeof(tBtcBtEvent)));
   nl_header->nlmsg_seq = 0;
   nl_header->nlmsg_pid = getpid();

   msgHdr = (tAniMsgHdr*)NLMSG_DATA(nl_header);
   msgHdr->type = WLAN_BTC_BT_EVENT_IND;
   msgHdr->length = sizeof(tBtcBtEvent);
   btEvent = (tBtcBtEvent*)((char*)msgHdr + sizeof(tAniMsgHdr));
   btEvent->ev = bt_event;

   // check for no data before calling copy
   if (NULL != event_data)
   {
      memcpy(&btEvent->u, event_data, sizeof(btEvent->u));
   }
#ifdef BTC_DEBUG
   // print out the event contents
   {
      unsigned int local_index;
      unsigned char *local_byteP;
      unsigned char local_byte;

      BTC_INFO("BTC-SVC: event type (%d)", btEvent->ev);
      if (event_data != NULL)
      {
        BTC_INFO("  event contents:");
        local_byteP = (unsigned char *)&(btEvent->u);
        for (local_index = 0; local_index < sizeof(btEvent->u); local_index++)
        {
          local_byte = *local_byteP++;
          BTC_INFO("  [%02d] 0x%02x (%d)", local_index, local_byte, local_byte);
        }
      }
   }
#endif // BTC_DEBUG

   if(sendto(pBtcSvcHandle->fd, (void *)nl_header, nl_header->nlmsg_len, 0,
      (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_nl)) < 0)
   {
      BTC_ERR("BTC-SVC: Unable to send WLAN_BTC_BT_EVENT_IND to WLAN\n");
      BTC_OS_ERR;
   }
}

/*--------------------------------------------------------------------------
  \brief process_event() - Utility function to detect WLAN Interface

  \param Pointer to BTC Services

  \return Returns Status value

  \sa
  --------------------------------------------------------------------------*/
eBtcStatus process_message(int fd, tBtcSvcHandle *pBtcSvcHandle)
{
   struct sockaddr_nl src_addr;
   socklen_t addr_len = sizeof(struct sockaddr_nl);
   struct nlmsghdr *nh = NULL;
   int slen = 0;
   char buffer[WLAN_NL_MAX_PAYLOAD];
   tAniMsgHdr *msgHdr = NULL;
   tWlanAssocData *assocData = NULL;
   unsigned int len;
   eBtcStatus status = BTC_SUCCESS;

   slen = recvfrom(fd, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr*)&src_addr, &addr_len);
   if(slen < 0)  {
      if(errno != EINTR && errno != EAGAIN) {
         BTC_ERR( "BTC-SVC: Error reading socket\n");
         BTC_OS_ERR;
         //Ignore the error for now and continue listening on the netlink socket
      }
      return status;
   }

   if(slen == 0) {
      return status;
   }

   //To suppress the warning in for loop
   len = (unsigned int) slen;

   //We really do not need a for loop. But this allows clients to send multiple netlink messages
   for (nh = (struct nlmsghdr *) buffer; NLMSG_OK (nh, len); nh = NLMSG_NEXT (nh, len))
   {
      /* The end of multipart message. */
      if (nh->nlmsg_type == NLMSG_DONE)
         return status;

      if (nh->nlmsg_type == NLMSG_ERROR)
         /* Do we need to do some error handling? */
         return status;

      if(nh->nlmsg_pid != 0)
         /* Accept message only from kernel */
         continue;

      if(nh->nlmsg_type != WLAN_NL_MSG_BTC)
         /* Accept only messages from WLAN that are meant for BTC */
         continue;

      msgHdr = NLMSG_DATA(nh);

      /* Continue with parsing payload. */
      switch(msgHdr->type)
      {
         case WLAN_MODULE_UP_IND:
            /* WLAN interface cameup */
            BTC_INFO( "BTC-SVC: WLAN Interface came up!\n");
            /* Register with BTC-ES */
            (void)register_btc(pBtcSvcHandle);
            break;

         case WLAN_MODULE_DOWN_IND:
            /* WLAN interface went down */
            BTC_INFO( "BTC-SVC: WLAN Interface went down!\n");
             /* Make sure we are registered with BTC-ES */
            if(register_btc(pBtcSvcHandle) == BTC_SUCCESS) {
               /* Communicate the WLAN channel mask to BTC-ES (no channels) */
               pBtcSvcHandle->btcEsFuncs.wlan_chan_func(0);
            }
            else
            {
               BTC_ERR("BTC-SVC: Could not pass disassoc info to BTC-ES\n");
            }
            /* Deregister with BTC-ES */
            (void)unregister_btc(pBtcSvcHandle);
            return BTC_WLAN_IF_DOWN;

         case WLAN_STA_ASSOC_DONE_IND:
            /* STA associated to an AP */
            BTC_INFO( "BTC-SVC: STA associated to an AP!\n");
            /* Make sure we are registered with BTC-ES */
            if(!register_btc(pBtcSvcHandle)) {
               /* Communicate the WLAN channel mask to BTC-ES */
               assocData =  (tWlanAssocData *)((char*)msgHdr + sizeof(tAniMsgHdr));
               BTC_INFO( "BTC-SVC: WLAN channel is %d\n", assocData->channel);
               pBtcSvcHandle->btcEsFuncs.wlan_chan_func(1 << (assocData->channel-1));
            }
            else
            {
               BTC_ERR("BTC-SVC: Could not pass assoc info to BTC-ES\n");
            }
            break;

         case WLAN_STA_DISASSOC_DONE_IND:
            /* STA no longer associated to an AP */
            BTC_INFO( "BTC-SVC: STA no longer associated to an AP!\n");
            /* Make sure we are registered with BTC-ES */
            if(!register_btc(pBtcSvcHandle)) {
               /* Communicate the WLAN channel mask to BTC-ES (no channels) */
               pBtcSvcHandle->btcEsFuncs.wlan_chan_func(0);
            }
            else
            {
               BTC_ERR("BTC-SVC: Could not pass disassoc info to BTC-ES\n");
            }
            break;

         case WLAN_BTC_QUERY_STATE_RSP:
            BTC_INFO( "BTC-SVC: Query Rsp rcvd from WLAN!\n");
            /* Make sure we are registered with BTC-ES */
            if(!register_btc(pBtcSvcHandle)) {
               /* Communicate the WLAN channel mask to BTC-ES */
               assocData =  (tWlanAssocData *)((char*)msgHdr + sizeof(tAniMsgHdr));
               BTC_INFO( "BTC-SVC: WLAN channel is %d\n", assocData->channel);
               pBtcSvcHandle->btcEsFuncs.wlan_chan_func(
                  assocData->channel ? 1 << (assocData->channel-1) : 0);
            }
            else
            {
               BTC_ERR("BTC-SVC: Could not pass WLAN state to BTC-ES\n");
            }
            break;

         default:
            BTC_ERR( "BTC-SVC: Unknown netlink message %d\n", msgHdr->type);
            break;
      }
   }

   return status;
}

/*--------------------------------------------------------------------------
  \brief thread_function() - Thread function that monitors the WLAN Interface

  \param Pointer for Global BTC Services Data passed as VOID pointer

  \return Returns Status value

  \sa
  --------------------------------------------------------------------------*/
static void* thread_function(void* arg)
{
   tBtcSvcHandle *pBtcSvcHandle = (tBtcSvcHandle*)arg;
   int fd, len, ret, wait;
   struct sockaddr_nl src_addr, dest_addr;
   unsigned char buffer[WLAN_NL_MAX_PAYLOAD];
   struct nlmsghdr *nl_header = NULL;
   tAniMsgHdr *msgHdr = NULL;
   char driver_status[PROPERTY_VALUE_MAX];

check_wlan:
   // Check if WLAN device is there or not. Following function would
   // block this thread until WLAN module is loaded or the thread is
   // signaled to shutdown
   if( check_wlan_present(pBtcSvcHandle) != BTC_SUCCESS)
      return NULL;

   BTC_INFO( "BTC-SVC: WLAN net device detected\n");

   // Create a netlink socket
   fd  = socket(AF_NETLINK, SOCK_RAW, WLAN_NLINK_PROTO_FAMILY);
   if(fd < 0) {
      BTC_ERR( "BTC-SVC: Cannot open Netlink socket\n");
      BTC_OS_ERR;
      /* This Implies:
       * - Either WLAN module failed to create socket or
       * - Delay between the creation of netlink Socket in the WLAN or
       * - WLAN loaded/unloaded at bootup.
       */
      usleep(BTC_SVC_SOCKET_CREATE_DELAY);
      goto check_wlan;
   }

   //Prepare to bind the socket
   memset(&src_addr, 0, sizeof(src_addr));
   src_addr.nl_family = AF_NETLINK; /* Domain */
   src_addr.nl_pid = getpid();

   //Subscriptions: WLAN can choose to mcast or ucast
   src_addr.nl_groups = WLAN_NLINK_MCAST_GRP_ID;

   // Bind assigns a name to the socket.
   ret = bind(fd, (struct sockaddr*)&src_addr, sizeof(src_addr));
   if (ret < 0) {
      BTC_ERR( "BTC-SVC: Cannot bind Netlink socket\n");
      BTC_OS_ERR;
      close(fd);
      //Implies WLAN module is not present. Go back to polling for WLAN
      goto check_wlan;
   }

   // Get the socket name, address and port number of the src socket
   len = sizeof(struct sockaddr_nl);
   if (getsockname(fd, (struct sockaddr *)&src_addr, &len) < 0) {
      BTC_ERR( "BTC-SVC: getsockname failed\n");
      BTC_OS_ERR;
      close(fd);
      return NULL;
   }

   /* Prepare to send a "Query WLAN" message to the kernel
      nl_pid = 0;   implies message is for Linux Kernel
      nl_groups = 0; implies message is unicast
   */
   memset(&dest_addr, 0, sizeof(dest_addr));
   dest_addr.nl_family = AF_NETLINK;

   nl_header = (struct nlmsghdr *)buffer;
   nl_header->nlmsg_type = WLAN_NL_MSG_BTC;
   nl_header->nlmsg_flags = NLM_F_REQUEST;
   nl_header->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr)));
   nl_header->nlmsg_seq = 0;
   nl_header->nlmsg_pid = src_addr.nl_pid;

   msgHdr = (tAniMsgHdr*)NLMSG_DATA(nl_header);
   msgHdr->type = WLAN_BTC_QUERY_STATE_REQ;
   msgHdr->length = 0;

   if(sendto(fd, (void *)nl_header, nl_header->nlmsg_len, 0,
      (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_nl)) < 0)
   {
      BTC_ERR("BTC-SVC: Unable to send WLAN_BTC_QUERY_STATE_REQ msg\n");
      BTC_OS_ERR;
      //Implies WLAN module is not present. Go back to polling for WLAN
      close(fd);
      goto check_wlan;
   }

   pBtcSvcHandle->fd = fd;

   while (1) {
      if(__select(fd, pBtcSvcHandle) == BTC_SUCCESS) {
         if (process_message(fd, pBtcSvcHandle) == BTC_WLAN_IF_DOWN) {
            //Implies WLAN device went down. Go back to polling for WLAN, but
            //allow the driver to close, so we don't re-open the socket too soon
            close(fd);

            //Wait up to 4 seconds
            wait = 0;
            while(wait++ < (BTC_SVC_WLAN_DOWN_WAIT_TIME_SEC * 1000000 / BTC_SVC_WLAN_DOWN_SLEEP_TIME_USEC)) {
              if (!property_get("wlan.driver.status", driver_status, NULL)) {
                BTC_ERR("BTC-SVC: Couldn't get driver status!\n");
                break;
              }
              if (!strcmp(driver_status, "unloaded")) {
                BTC_INFO("BTC-SVC: WLAN driver unloaded: %d", wait);
                break;
              }
              BTC_INFO("BTC-SVC: Waiting: %d", wait);
              usleep(BTC_SVC_WLAN_DOWN_SLEEP_TIME_USEC);
            }
            goto check_wlan;
         }
         else
            continue;
      }
      else
         break;
   }

   // Thread is being terminated.
   BTC_INFO( "BTC-SVC: Thread terminating\n");
   close(fd);
   return NULL;
}

/*--------------------------------------------------------------------------
  \brief btc_svc_init() - This function will be called by BTC ES

  \param pBtcEsFuncs  Function pointers for Register/Deregister functions

  \return Returns Status value (0 = sucess, -1 = failure)

  \sa

  --------------------------------------------------------------------------*/
int btc_svc_init (btces_funcs *pBtcEsFuncs)
{
   if(gpBtcSvc != NULL) {
      BTC_ERR( "BTC-SVC: Trying to init BTC SVC twice\n");
      return -1;
   }

   // Allocate the BTC Svc object
   gpBtcSvc = (tBtcSvcHandle*)malloc(sizeof(tBtcSvcHandle));

   if(gpBtcSvc == NULL) {
      BTC_ERR("BTC-SVC: No memory for BTC Svc object\n");
      return -1;
   }

   // Initialize the BTC Svc object
   memset(gpBtcSvc, 0, sizeof(*gpBtcSvc));

   // Store function pointers to talk to BTC-ES
   gpBtcSvc->btcEsFuncs.register_func = pBtcEsFuncs->register_func;
   gpBtcSvc->btcEsFuncs.deregister_func = pBtcEsFuncs->deregister_func;
   gpBtcSvc->btcEsFuncs.state_report_func = pBtcEsFuncs->state_report_func;
   gpBtcSvc->btcEsFuncs.wlan_chan_func = pBtcEsFuncs->wlan_chan_func;

   gpBtcSvc->btcSvcState = BTC_SVC_UNREGISTERED;

   // Create a pipe that can be used to signal thread shutdown
   if(pipe(gpBtcSvc->pd)) {
      BTC_ERR( "BTC-SVC: Cannot open pipe\n");
      BTC_OS_ERR;
      free(gpBtcSvc);
      gpBtcSvc = NULL;
      return -1;
   }

   //Spawn a thread which will monitor the WLAN Interface. This thread
   //will be responsible for detecting when WLAN interface comes up. Once
   //WLAN interface is up, this thread will register the btc_svc_inject_bt_event
   //callback function with BTC-ES layer via register_func supplied by BTC-ES
   if(pthread_create(&gpBtcSvc->workerThread, NULL, thread_function, gpBtcSvc))
   {
      //log an error
      BTC_ERR( "BTC-SVC: pthread_create failed\n");
      close(gpBtcSvc->pd[0]);
      close(gpBtcSvc->pd[1]);
      free(gpBtcSvc);
      gpBtcSvc = NULL;
      return -1;
   }

   return 0;
}

/*--------------------------------------------------------------------------
  \brief btc_svc_deinit() - This function de-inits the BTC Service layer

  \return none

  \sa
  --------------------------------------------------------------------------*/
void btc_svc_deinit (void)
{
   char shutdown = 0x0;
   void *value = NULL;

   if(gpBtcSvc != NULL)
   {
      // Stop the worker BTC-SVC thread
      write(gpBtcSvc->pd[1], &shutdown, sizeof(shutdown));
      pthread_join(gpBtcSvc->workerThread, &value);

      //close the pipes opened for communicating to worker BTC-SVC thread
      close(gpBtcSvc->pd[0]);
      close(gpBtcSvc->pd[1]);

      //Deallocate memory of BTC-SVC
      free(gpBtcSvc);
      gpBtcSvc = NULL;
   }
}

