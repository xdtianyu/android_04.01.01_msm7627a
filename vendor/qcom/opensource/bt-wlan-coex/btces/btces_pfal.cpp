/*
Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.

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
  @file btces_pfal.cpp

  This file implements BTC-ES PFAL API for the Android/BlueZ platform.

  In addition to the publicly advertised PFAL APIs (via btces_pfal.h), the
  platform adaptation layer also implements the glue code required to interface
  with the BlueZ stack and derive HCI/Native events, including the worker thread
  utilized for this purpose.
  The PFAL implementation also provides the main function since on the
  Android+BlueZ platform, the BTCES model is that of an executable, running as a
  daemon.
  The main() function in this file is responsible for triggering the BT Coex shim
  for BlueZ, much as BM3 initialization logic would directly invoke the shim API
  to initiate the rest of BTCES functionality.
*/

/*=============================================================================

                       EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order. Please
  use ISO format for dates.

  when        who  what, where, why
  ----------  ---  -----------------------------------------------------------
  2011-06-06   ss  Resolve issues due to addition of bluetooth management
                   interface in kernel 2.6.38 and delay in intializing the
                   default adapter in BlueZ.
  2011-04-01   ag  Fixing Compilation Issue with new DBUS api's on Honeycomb
  2011-03-01   rr  Resolving compilation errors for stricter compilation rules.
  2010-02-28   rr  Added support for sending the one last event notification
                   to BTCES in the signal handler if it is missed in the
                   worker thread.
  2010-09-14   tm  Correct handling of AdapterAdded/AdapterRemoved; code cleanup
  2010-08-11   tm  AFH guard band is now 11 MHz on either side of WLAN channel
  2010-03-03   pj  Initial Open Source version

=============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

/* Standard Linux headers */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* BlueZ related includes */
#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"

/* Dbus related includes */
#include "dbus/dbus.h"

#include "btces_plat.h"
#include "btces_pfal.h"
#include "bt_coex_shim.h"
#include "btces_svc.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/* Cookie for sanity checks */
#define BTCES_COOKIE 0x10DECADE

/* BTCES Daemon name as a string */
#define BTCES_DAEMON_NAME "btwlancoex"

/* Comment out if Channel Assessment mode is to be left alone when WLAN is active */
#define TURN_OFF_CA_IF_WLAN

/* Reference : ANSI/IEEE Std 802.11 1999, Section 15.4.6.2 and
   Bluetooth Specification v2.0+EDR Vol 2, Part A, Section 2
   Bluetooth channel spacing is 1 MHZ
*/
#define WLAN_80211_RF_CH_1_MHZ       (2412) /**< WLAN Ch1 = 2412 MHz */
#define WLAN_80211_RF_CH_SPACING_MHZ (5)    /**< WLAN Channels 1-13 are spaced @ 5 MHz */
#define WLAN_80211_RF_CH_14_MHZ      (2484) /**< WLAN Ch14 = 2484 MHz */
#define BT_RF_CHANNEL_0_MHZ          (2402) /**< Bluetooth Ch0 = 2402 MHz */
#define BT_N_MIN  (20)  /**< Nmin: Minimum number of BT channels, this is a spec value */

/** Define the platform specific guard band: Exclude BT channels within this many
    channels (MHz) of the WLAN frequency. This number must be 29 or less, or else
    a single WLAN channel will exclude too many Bluetooth frequencies (79-29*2-1 = BT_N_MIN)
*/
#define BT_DC_AFH_CH_EXCLUDE  (11)  /* Recommended by QCOM WLAN team */

/* State values of the initial Channel Assessment mode */
#define CA_MODE_OFF     (0x00)
#define CA_MODE_ON      (0x01)
#define CA_MODE_UNKNOWN (0xFF)

/* Conversion from milliseconds to nanoseconds */
#define BTCES_MS_TO_NS(x) ((x) * 1000000)

/* Derive seconds component from milliseconds */
#define BTCES_SEC_FROM_MS(x) ( (((x) - ((x) % 1000))/1000) )

/* Derive anything < 1 sec from milliseconds and convert to nanoseconds */
#define BTCES_NS_FROM_MS(x) ( ((x) % 1000) * 1000000 )

/* Timeout for dbus queries (5 seconds - arbitrary choice) */
#define BTCES_DBUS_TIMEOUT  5000

//Maximum Time to wait for BT driver to initialize.
#define BTCES_BT_SETTLE_TIME_SEC 2
#define BTCES_BT_UP_SLEEP_TIME_USEC 100000


/* Timeout for HCI lib operations (5 seconds - arbitrary choice) */
#define BTCES_HCI_LIB_TIMEOUT 5000

/* Maximum size of default adapter string */
#define BTCES_MAX_ADAPTER_SIZE 128

/* Maximum size of an object path */
#define BTCES_MAX_OBJ_PATH_SIZE 128

/* BT addr size in bytes */
#define BTCES_BT_ADDR_SIZE 6

/* Hexadecimal base definition for strtol */
#define BTCES_BASE_HEX 16

/* Define a max macro */
#define BTCES_MAX(x,y) ( ((x) > (y)) ? (x) : (y) )

/* Macro for the bluez path */
#define BTCES_BLUEZ_PATH "/org/bluez/"
#define BTCES_BLUEZ_PATH_LEN 11

/* Macro for the dev string */
#define BTCES_DEV_STR "dev"
#define BTCES_DEV_STR_LEN 3

/* Macro for the hci string */
#define BTCES_HCI_STR "hci"
#define BTCES_HCI_STR_LEN 3

/* Macro to define size of the pipe file descriptor - should always be 2 */
#define BTCES_PIPE_FD_SIZE 2

/* Macro to define the max number of dbus signals of interest (currently 6) */
#define BTCES_MAX_DBUS_SIGNALS 6

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/**
   This structure is used to define the initial values for main() to store
   user config.
   We use this to neatly share data between main() and the real pfal code
*/
typedef struct
{
  /* See btces_pfal_data_struct which is the real pfal info */
  boolean       read_ca_from_bluez;

  /* See btces_pfal_data_struct which is the real pfal info */
  boolean       turn_off_ca_if_wlan;

  /* See btces_pfal_data_struct which is the real pfal info */
  uint8                                 initial_ca_mode;
} btces_pfal_user_data_struct;

/**
   This enumeration lists the possible dbus information that can be queried
   (used to improve refactoring)
 */
typedef enum
{
  BTCES_PFAL_DBUS_DEFAULT_ADAPTER_INFO    = 0,

  BTCES_PFAL_DBUS_MAX_INFO
} btces_pfal_dbus_info_enum;

/**
   This enumeration lists the possible dbus response types
 */
typedef enum
{
  BTCES_PFAL_DBUS_RSP_NONE_TYPE      = 0,
  BTCES_PFAL_DBUS_RSP_STRING_TYPE    = 1,
  BTCES_PFAL_DBUS_RSP_BOOLEAN_TYPE   = 2,
  BTCES_PFAL_DBUS_RSP_ARRAY_TYPE     = 3,
  BTCES_PFAL_DBUS_RSP_OBJ_PATH_TYPE  = 4,

  BTCES_PFAL_DBUS_RSP_MAX_TYPE
} btces_pfal_dbus_rsp_type_enum;

/** This type represents the data structure used to manipulate timer handling in
    PFAL
*/
typedef struct
{
  unsigned int               cookie;
  btces_pfal_timer_cb_type  *client_callback;
  void                      *client_user_data;
  timer_t                    timer;
  struct sigevent            event;
  struct itimerspec          timer_spec;
} btces_pfal_timer_struct;

/** This type represents the data structure used to propagate watch info
    over the pipe
*/
typedef struct
{
  int              new_fd;
  unsigned int     flags;
  DBusWatch       *watch_ptr;
} btces_pfal_watch_info_struct;


/** This type represents the handler for a particular dbus signal
*/
typedef void (* btces_pfal_dbus_signal_handler_type)(DBusMessage *);


/** This type represents the data structure used to maintain a
    reference of interface and signal name
*/
typedef struct
{
  const char                                   *interface;
  const char                                   *signal_name;
  const btces_pfal_dbus_signal_handler_type     signal_handler;
} btces_pfal_dbus_signal_struct;

/**
   This type represents the structure for all the pfal worker thread related
   information.
 */
typedef struct
{
  pthread_t        thread_handle;  /* Worker thread for HCI/DBus operations */

  boolean          close_worker_thread; /* Should we close the worker thread? */

  /** Pipe for determining whether to close worker
      thread.
      NOTE: close_pipe_fd[0] is always read from
      worker thread and close_pipe_fd[1] is always
      written from main thread
  */
  int              close_pipe_fd[BTCES_PIPE_FD_SIZE];

  /** Pipe for dbus watch operations to propagate
      watch fd to worker thread
      NOTE: watch_pipe_fd[0] is always read from
      worker thread and watch_pipe_fd[1] is always
      written from watch callbacks
  */
  int              watch_pipe_fd[BTCES_PIPE_FD_SIZE];

  /* File descriptor for dbus operations */
  int              dbus_fd;

  /* File descriptor for hci operations */
  int              hci_fd;

  /* File descriptor set for read operations */
  fd_set           read_set;

  /* Dbus Connection Handle */
  DBusConnection  *conn_ptr;

  /* Dbus watch handle */
  DBusWatch       *watch_ptr;

  /* Dbus default adapter string */
  char             default_adapter[BTCES_MAX_ADAPTER_SIZE];

  /* HCI read buffer (max value from HCI) */
  char             hci_socket_buf[HCI_MAX_FRAME_SIZE];

  /* Device descriptor for AFH/CA operations (read from WLAN thread) */
  int              hci_lib_dd;
} btces_pfal_worker_thread_data_struct;


/**
   This type represents the structure for all the pfal related
   information in one single control block.
 */
typedef struct
{
  /* Has PFAL been initialized? */
  boolean                               initialized;

  /** This variable tracks the initial state of Channel Assessment mode in the SoC,
   as well as whether we actually know what it was to start with. If the intitial
   state is unknown right before sending the first AFH channel map when WLAN goes
   active, we will read it then.
  */
  uint8                                 initial_ca_mode;

  /**
    This variable is provided from cmd line and determines whether daemon should
    read the current ca mode from bluez or assume a value
    If this is FALSE, current ca mode is not read from bluez
  */
  boolean                               read_ca_from_bluez;

  /**
    We allow a run-time determination of whether we want to turn off ca on bluez
    If this is FALSE, CA is not touched
  */
  boolean                               turn_off_ca_if_wlan;

  btces_pfal_worker_thread_data_struct  worker_thread_info;

  /* Handle to mutex offered to PFAL clients */
  pthread_mutex_t                       client_token;

} btces_pfal_data_struct;



/*----------------------------------------------------------------------------
 * Forward Declarations
 * -------------------------------------------------------------------------*/
void *btces_pfal_worker_thread(void *unused);

static void btces_pfal_timer_notify_callback(union sigval);

static BTCES_STATUS btces_pfal_init_control_block(void);

static dbus_bool_t btces_pfal_dbus_add_watch_callback(DBusWatch *watch_ptr, void *user_data);

static void btces_pfal_dbus_remove_watch_callback(DBusWatch *watch_ptr, void *user_data);

static void btces_pfal_dbus_toggle_watch_callback(DBusWatch *watch_ptr, void *user_data);

static int btces_pfal_get_dev_id_from_path(const char *path_ptr);

static BTCES_STATUS btces_pfal_hci_open(void);

static BTCES_STATUS btces_pfal_hci_close(void);

static BTCES_STATUS btces_pfal_update_afh_map(uint8 *afh_mask_ptr);

static BTCES_STATUS btces_pfal_update_ca_mode(boolean turn_off_ca);

/* Native event handlers for the table */
static void btces_pfal_dbus_adapter_added_sig_handler(DBusMessage *msg_ptr);
static void btces_pfal_dbus_adapter_removed_sig_handler(DBusMessage *msg_ptr);
static void btces_pfal_dbus_adapter_property_changed_sig_handler(DBusMessage *msg_ptr);
static void btces_pfal_dbus_audio_sink_playing_sig_handler(DBusMessage *msg_ptr);
static void btces_pfal_dbus_audio_sink_stopped_sig_handler(DBusMessage *msg_ptr);

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/* For data read in the main() function */
static btces_pfal_user_data_struct   g_btces_pfal_user_data;

/* The real PFAL global data */
static btces_pfal_data_struct  g_btces_pfal_data;

static const btces_pfal_dbus_signal_struct g_btces_dbus_sig_table[BTCES_MAX_DBUS_SIGNALS] =
{
  {"org.bluez.Manager",
    "AdapterAdded",
    btces_pfal_dbus_adapter_added_sig_handler,
  },
  {"org.bluez.Manager",
    "AdapterRemoved",
    btces_pfal_dbus_adapter_removed_sig_handler,
  },
  {"org.bluez.Adapter",
    "PropertyChanged",
    btces_pfal_dbus_adapter_property_changed_sig_handler,
  },
  {"org.bluez.AudioSink",
    "Playing",
    btces_pfal_dbus_audio_sink_playing_sig_handler,
  },
  {"org.bluez.AudioSink",
    "Stopped",
    btces_pfal_dbus_audio_sink_stopped_sig_handler,
  },
  {"org.bluez.AudioSink",
    "Disconnected",
    btces_pfal_dbus_audio_sink_stopped_sig_handler, /* same hdlr as Stopped */
  }
};

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*==============================================================
FUNCTION:  btces_pfal_init_control_block
==============================================================*/
static BTCES_STATUS btces_pfal_init_control_block
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  memset(&g_btces_pfal_data,
         0,
         sizeof(g_btces_pfal_data));

  g_btces_pfal_data.initialized = FALSE;
  g_btces_pfal_data.initial_ca_mode = CA_MODE_UNKNOWN;
  g_btces_pfal_data.read_ca_from_bluez = FALSE;
  g_btces_pfal_data.turn_off_ca_if_wlan = FALSE;
  g_btces_pfal_data.worker_thread_info.close_worker_thread = FALSE;
  g_btces_pfal_data.worker_thread_info.thread_handle = 0;
  g_btces_pfal_data.worker_thread_info.dbus_fd = -1;
  g_btces_pfal_data.worker_thread_info.hci_fd = -1;
  g_btces_pfal_data.worker_thread_info.hci_lib_dd = -1;
  FD_ZERO(&g_btces_pfal_data.worker_thread_info.read_set);
  g_btces_pfal_data.worker_thread_info.conn_ptr = NULL;
  g_btces_pfal_data.worker_thread_info.watch_ptr = NULL;

  memset(&g_btces_pfal_data.worker_thread_info.default_adapter[0],
         0,
         sizeof(g_btces_pfal_data.worker_thread_info.default_adapter));
  memset(&g_btces_pfal_data.worker_thread_info.hci_socket_buf[0],
         0,
         sizeof(g_btces_pfal_data.worker_thread_info.hci_socket_buf));

  /* Set pipe fd values to -1 to make sure they are invalid */
  memset(&g_btces_pfal_data.worker_thread_info.close_pipe_fd[0],
         -1,
         sizeof(g_btces_pfal_data.worker_thread_info.close_pipe_fd));
  memset(&g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0],
         -1,
         sizeof(g_btces_pfal_data.worker_thread_info.watch_pipe_fd));

  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_configure_ca_support
==============================================================*/
static BTCES_STATUS btces_pfal_configure_ca_support
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /**
     The logic for determining CA behavior is:

     -- If "turn off CA if WLAN" is FALSE, other values do not matter

     -- If "turn off CA if WLAN" CA mode is turned on:

     If CA mode needs to be read, then "initial CA mode" is set to UNKNOWN
     (it is derived at run time from BlueZ)

     If CA mode should not be read, we set "initial CA mode" to user
     preference to preserve what it should be reset to after WLAN is done.
     In this case during WLAN chan updates, this "initial CA mode" is
     never updated.

     Note: If the user sets CA mode not to be read from BlueZ and fails to
     provide an initial value, then we default to ON for "initial CA mode"
  */
  if(FALSE == g_btces_pfal_user_data.turn_off_ca_if_wlan)
  {
    BTCES_MSG_MEDIUM("btces_pfal_configure_ca_support(): Do not turn off CA" BTCES_EOL);
    /* Reset to default values just in case */
    g_btces_pfal_data.initial_ca_mode = CA_MODE_UNKNOWN;
    g_btces_pfal_data.turn_off_ca_if_wlan = FALSE;
    g_btces_pfal_data.read_ca_from_bluez = FALSE;
  }
  else
  {
    BTCES_MSG_MEDIUM("btces_pfal_configure_ca_support(): Turn off CA if WLAN" BTCES_EOL);

    g_btces_pfal_data.turn_off_ca_if_wlan = TRUE;

    if(TRUE == g_btces_pfal_user_data.read_ca_from_bluez)
    {
      BTCES_MSG_MEDIUM("btces_pfal_configure_ca_support(): Read CA mode from BlueZ" BTCES_EOL);
      g_btces_pfal_data.read_ca_from_bluez = TRUE;
      g_btces_pfal_data.initial_ca_mode = CA_MODE_UNKNOWN;
    }
    else
    {
      BTCES_MSG_MEDIUM("btces_pfal_configure_ca_support(): Do not read CA mode from BlueZ" BTCES_EOL);

      /* Apply init ca mode from user or if not default to on */
      g_btces_pfal_data.read_ca_from_bluez = FALSE;

      if(CA_MODE_OFF == g_btces_pfal_user_data.initial_ca_mode)
      {
        BTCES_MSG_MEDIUM("btces_pfal_configure_ca_support(): Init CA mode off" BTCES_EOL);
        g_btces_pfal_data.initial_ca_mode = CA_MODE_OFF;
      }
      else
      {
        BTCES_MSG_MEDIUM("btces_pfal_configure_ca_support(): Init CA mode on" BTCES_EOL);
        g_btces_pfal_data.initial_ca_mode = CA_MODE_ON;
      }
    }
  }

  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_init_worker_thread
==============================================================*/
static BTCES_STATUS btces_pfal_init_worker_thread
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* For now, only the close pipe needs to be initialized */
  if(0 > pipe(g_btces_pfal_data.worker_thread_info.close_pipe_fd))
  {
    BTCES_MSG_ERROR("btces_pfal_init_worker_thread(): pipe create failure" BTCES_EOL);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  /* Add the read close_pipe_fd to the file set for select() operation */
  FD_SET(g_btces_pfal_data.worker_thread_info.close_pipe_fd[0],
         &g_btces_pfal_data.worker_thread_info.read_set);

  BTCES_MSG_MEDIUM("btces_pfal_init_worker_thread(): init success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_add_watch_callback
==============================================================*/
static dbus_bool_t btces_pfal_dbus_add_watch_callback
(
  DBusWatch *watch_ptr,
  void      *user_data
)
{
  btces_pfal_watch_info_struct   watch_info;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(NULL == user_data || user_data)
  {
    BTCES_MSG_LOW("btces_pfal_dbus_add_watch_callback(): user_data currently unused" BTCES_EOL);
  }

  if(NULL == watch_ptr)
  {
    BTCES_MSG_LOW("btces_pfal_dbus_add_watch_callback(): watch_ptr invalid" BTCES_EOL);
    return FALSE;
  }

  /* Check whether the watch has been enabled - if so, continue */
  if(FALSE == dbus_watch_get_enabled(watch_ptr))
  {
    /** Need to return TRUE per dbus watch API since it is not a failure -
       just not of our interest due to being disabled
    */
    BTCES_MSG_MEDIUM("btces_pfal_dbus_add_watch_callback(): watch not enabled" BTCES_EOL);
    return TRUE;
  }

  /* Check whether flags match - if watch is readable, continue */
  watch_info.flags = dbus_watch_get_flags(watch_ptr);
  if(!(DBUS_WATCH_READABLE & watch_info.flags))
  {
    /** Need to return TRUE per dbus watch API since it is not a failure -
       just not of our interest due to being non-readable
    */
    BTCES_MSG_MEDIUM("btces_pfal_dbus_add_watch_callback(): watch not readable. Flags: %d" BTCES_EOL,
                     watch_info.flags);
    return TRUE;
  }

  /* Get the fd to be used for the dbus select operations */
  watch_info.new_fd = dbus_watch_get_unix_fd(watch_ptr);

  /* Populate the watch ptr for later use */
  watch_info.watch_ptr = watch_ptr;

  /* Notify worker thread of new fd */
  (void) write(g_btces_pfal_data.worker_thread_info.watch_pipe_fd[1],
               (void *)&watch_info,
               sizeof(watch_info));


  BTCES_MSG_HIGH("btces_pfal_dbus_add_watch_callback(): watch enabled for fd: %d, watch: %p" BTCES_EOL,
                 watch_info.new_fd,
                 (void *)watch_ptr);
  return TRUE;
}


/*==============================================================
FUNCTION:  btces_pfal_dbus_remove_watch_callback
==============================================================*/
static void btces_pfal_dbus_remove_watch_callback
(
  DBusWatch *watch_ptr,
  void      *user_data
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(NULL == user_data || user_data)
  {
    BTCES_MSG_LOW("btces_pfal_dbus_remove_watch_callback(): user_data currently unused" BTCES_EOL);
  }

  if(NULL != watch_ptr)
  {
    /* We ignore this since watch remains on for as long as BlueZ/coex are on */

    BTCES_MSG_HIGH("btces_pfal_dbus_remove_watch_callback(): watch disabled: %p" BTCES_EOL,
                   (void *)watch_ptr);
  }
}


/*==============================================================
FUNCTION:  btces_pfal_dbus_toggle_watch_callback
==============================================================*/
static void btces_pfal_dbus_toggle_watch_callback
(
  DBusWatch *watch_ptr,
  void      *user_data
)
{
  dbus_bool_t                    enabled = FALSE;
  btces_pfal_watch_info_struct   watch_info;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(NULL == user_data || user_data)
  {
    BTCES_MSG_LOW("btces_pfal_dbus_toggle_watch_callback(): user_data currently unused" BTCES_EOL);
  }

  if(NULL == watch_ptr)
  {
    BTCES_MSG_LOW("btces_pfal_dbus_toggle_watch_callback(): watch_ptr invalid" BTCES_EOL);
    return;
  }

  /** Check whether flags match - if watch is readable,
      continue. This assumes that to disable a previously
      readable watch, the toggle function would be called with
      the flag as readable and the status would be disabled.
      This also assumes that we will have only one active
      readable watch at a time, which should be the case.
  */
  watch_info.flags = dbus_watch_get_flags(watch_ptr);
  if(!(DBUS_WATCH_READABLE & watch_info.flags))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_toggle_watch_callback(): watch: %p not readable. Flags: %d" BTCES_EOL,
                    watch_ptr,
                    watch_info.flags);
    return;
  }

  /* Check whether the watch has been enabled */
  enabled = dbus_watch_get_enabled(watch_ptr);

  BTCES_MSG_HIGH("btces_pfal_dbus_toggle_watch_callback(): watch status: %d" BTCES_EOL,
                 enabled);

  /* Get the fd to be used for the dbus select operations; if feasible */
  if(TRUE == enabled)
  {
    watch_info.new_fd = dbus_watch_get_unix_fd(watch_ptr);
  }
  else
  {
    /* Propagate to our thread the invalid fd to avoid the select */
    watch_info.new_fd = -1;
  }

  /* Store the watch ptr as the final field */
  watch_info.watch_ptr = watch_ptr;

  /* Notify worker thread of new fd */
  (void) write(g_btces_pfal_data.worker_thread_info.watch_pipe_fd[1],
               (void *)&watch_info,
               sizeof(watch_info));

  BTCES_MSG_HIGH("btces_pfal_dbus_toggle_watch_callback(): watch toggled for fd: %d, watch: %p" BTCES_EOL,
                 watch_info.new_fd,
                 (void *)watch_ptr);
}


/*==============================================================
FUNCTION:  btces_pfal_dbus_open
==============================================================*/
static BTCES_STATUS btces_pfal_dbus_open
(
  void
)
{
  DBusError bus_error;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(NULL != g_btces_pfal_data.worker_thread_info.conn_ptr)
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_open(): already have a bus!" BTCES_EOL);
    return BTCES_STATUS_ALREADY_INITIALIZED;
  }

  dbus_error_init(&bus_error);

  /* Connect to the system bus (bluez is offered on the system bus only) */
  g_btces_pfal_data.worker_thread_info.conn_ptr = dbus_bus_get(DBUS_BUS_SYSTEM,
                                                               &bus_error);

  if((dbus_error_is_set(&bus_error)) ||
     (NULL == g_btces_pfal_data.worker_thread_info.conn_ptr))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_open(): could not get system bus!" BTCES_EOL);

    dbus_error_free(&bus_error);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  dbus_error_free(&bus_error);

  /* Enable the watch pipe */
  if(0 > pipe(g_btces_pfal_data.worker_thread_info.watch_pipe_fd))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_open(): pipe create failure" BTCES_EOL);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  /* Add the read watch_pipe_fd to the file set for select() operation */
  FD_SET(g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0],
         &g_btces_pfal_data.worker_thread_info.read_set);

  /* Set up dbus fd framework */
  if(FALSE == dbus_connection_set_watch_functions(g_btces_pfal_data.worker_thread_info.conn_ptr,
                                                  &btces_pfal_dbus_add_watch_callback,
                                                  &btces_pfal_dbus_remove_watch_callback,
                                                  &btces_pfal_dbus_toggle_watch_callback,
                                                  NULL,
                                                  NULL))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_open(): could not set up watch!" BTCES_EOL);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_open(): bus get success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_enable_events
==============================================================*/
static BTCES_STATUS btces_pfal_dbus_enable_events
(
  void
)
{
  DBusError bus_error;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  dbus_error_init(&bus_error);

  /* No need to check conn_ptr (dbus will fail and we catch that directly) */

  /* Enable the Manager interface signals */
  dbus_bus_add_match(g_btces_pfal_data.worker_thread_info.conn_ptr,
                     "type='signal',interface='org.bluez.Manager'",
                     &bus_error);

  dbus_connection_flush(g_btces_pfal_data.worker_thread_info.conn_ptr);

  if( dbus_error_is_set(&bus_error) )
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_enable_events(): could not enable Manager signals!" BTCES_EOL);

    dbus_error_free(&bus_error);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_enable_events(): Manager signals enabled" BTCES_EOL);

  /* Enable the Adapter interface signals */
  dbus_bus_add_match(g_btces_pfal_data.worker_thread_info.conn_ptr,
                     "type='signal',interface='org.bluez.Adapter'",
                     &bus_error);

  dbus_connection_flush(g_btces_pfal_data.worker_thread_info.conn_ptr);

  if( dbus_error_is_set(&bus_error) )
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_enable_events(): could not enable Adapter signals!" BTCES_EOL);

    dbus_error_free(&bus_error);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_enable_events(): Adapter signals enabled" BTCES_EOL);

  /* Add A2DP (sink) signals here */
  dbus_bus_add_match(g_btces_pfal_data.worker_thread_info.conn_ptr,
                     "type='signal',interface='org.bluez.AudioSink'",
                     &bus_error);

  dbus_connection_flush(g_btces_pfal_data.worker_thread_info.conn_ptr);

  if(dbus_error_is_set(&bus_error))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_enable_events(): could not enable Sink signals!" BTCES_EOL);

    dbus_error_free(&bus_error);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_enable_events(): Sink signals enabled" BTCES_EOL);

  dbus_error_free(&bus_error);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_enable_events(): setup success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_get_info
==============================================================*/
static BTCES_STATUS btces_pfal_dbus_get_info
(
  btces_pfal_dbus_info_enum         info_type,
  void                             *info_ptr,      /* in and out */
  int                              *info_size_ptr  /* in and out */
)
{
  btces_pfal_dbus_rsp_type_enum   rsp_type = BTCES_PFAL_DBUS_RSP_NONE_TYPE;
  DBusError                       bus_error;
  DBusMessage                    *bus_req_ptr = NULL;
  DBusMessage                    *bus_rsp_ptr = NULL;
  char                           *bus_rsp_str_ptr;
  unsigned int wait;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* No need to check conn_ptr (dbus will fail and we catch that directly) */

  switch(info_type)
  {
    case BTCES_PFAL_DBUS_DEFAULT_ADAPTER_INFO:
      bus_req_ptr = dbus_message_new_method_call("org.bluez",
                                                 "/",
                                                 "org.bluez.Manager",
                                                 "DefaultAdapter");

      rsp_type = BTCES_PFAL_DBUS_RSP_OBJ_PATH_TYPE;
      break;

    default:
      BTCES_MSG_ERROR("btces_pfal_dbus_get_info(): unsupported type!" BTCES_EOL);
  }

  if(NULL == bus_req_ptr)
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_get_info(): could not create msg!" BTCES_EOL);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  dbus_error_init(&bus_error);

  //Wait up to BTCES_BT_SETTLE_TIME_SEC seconds
  wait = 0;
  while(wait++ < (BTCES_BT_SETTLE_TIME_SEC* 1000000 / BTCES_BT_UP_SLEEP_TIME_USEC)) {

    if(dbus_error_is_set(&bus_error)) {
      /* Free the dbus_error if it's already set before sneding it to dbus module for setting the error */
      dbus_error_free(&bus_error);
    }

    /* Send the request and wait for finish */
    bus_rsp_ptr = dbus_connection_send_with_reply_and_block(g_btces_pfal_data.worker_thread_info.conn_ptr,
                                                            bus_req_ptr,
                                                            BTCES_DBUS_TIMEOUT,
                                                            &bus_error);
    if(bus_rsp_ptr != NULL) {
      BTCES_MSG_MEDIUM("btces_pfal_dbus_get_info(): dbus_connection_send_with_reply_and_block Success %d"
                       BTCES_EOL, wait);
      break;
    }

    if(dbus_error_is_set(&bus_error)) {
      /* Verify if BTCES_DBUS_TIMEOUT occurred by checking the bus_error and exit the loop on same,
         it means 'org.bluez' is found and there is time out issue in getting the default adapter */
      if(strcmp(bus_error.name, DBUS_ERROR_NO_REPLY) == 0) {
        BTCES_MSG_MEDIUM("btces_pfal_dbus_get_info(): dbus_connection_send_with_reply_and_block: DBUS_ERROR_NO_REPLY" BTCES_EOL);
        break;
      }
    }

    BTCES_MSG_MEDIUM("btces_pfal_dbus_get_info(): Waiting: %d micro seconds(Max waiting time: %d Seconds)"
                     BTCES_EOL, (wait*BTCES_BT_UP_SLEEP_TIME_USEC),BTCES_BT_SETTLE_TIME_SEC);
    usleep(BTCES_BT_UP_SLEEP_TIME_USEC);
  }


  /* Free the request (it cannot be NULL) */
  dbus_message_unref(bus_req_ptr);

  if((dbus_error_is_set(&bus_error)) ||
     (NULL == bus_rsp_ptr))
  {

    BTCES_MSG_ERROR("Error name: %s", bus_error.name);
    BTCES_MSG_ERROR("Error message: %s", bus_error.message);

    BTCES_MSG_ERROR("btces_pfal_dbus_get_info(): could not get rsp!" BTCES_EOL);

    if(NULL != bus_rsp_ptr)
    {
      dbus_message_unref(bus_rsp_ptr);
    }

    dbus_error_free(&bus_error);
    return BTCES_FAIL;
  }

  /* Sanity check */
  BTCES_ASSERT(NULL != info_ptr);

  switch(rsp_type)
  {
    case BTCES_PFAL_DBUS_RSP_STRING_TYPE:
      /* Fall through: the rsp type is separated below b/w str & obj path */
    case BTCES_PFAL_DBUS_RSP_OBJ_PATH_TYPE:

      bus_rsp_str_ptr = NULL;

      /* Get the obj path or string argument from the rsp */
      (void) dbus_message_get_args(bus_rsp_ptr,
                                   &bus_error,
                                   ((BTCES_PFAL_DBUS_RSP_OBJ_PATH_TYPE == rsp_type) ?
                                    DBUS_TYPE_OBJECT_PATH : DBUS_TYPE_STRING),
                                   &bus_rsp_str_ptr,
                                   DBUS_TYPE_INVALID);

      if((dbus_error_is_set(&bus_error)) ||
         (NULL == bus_rsp_str_ptr))
      {
        BTCES_MSG_ERROR("btces_pfal_dbus_get_info(): could not get rsp str!" BTCES_EOL);
        BTCES_MSG_ERROR("Error name: %s", bus_error.name);
        BTCES_MSG_ERROR("Error message: %s", bus_error.message);

        dbus_message_unref(bus_rsp_ptr);
        dbus_error_free(&bus_error);
        return BTCES_FAIL;
      }

      BTCES_ASSERT(NULL != info_size_ptr);

      memcpy((char *)info_ptr,
             bus_rsp_str_ptr,
             *info_size_ptr);

      /* No need to update info_size_ptr */

      break;

    default:
      BTCES_MSG_ERROR("btces_pfal_dbus_get_info(): unsupported type!" BTCES_EOL);
  }

  /* Free the response (it cannot be NULL) */
  dbus_message_unref(bus_rsp_ptr);

  dbus_error_free(&bus_error);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_get_info(): Success" BTCES_EOL);
  return BTCES_SUCCESS;
}


/*==============================================================
FUNCTION:  btces_pfal_dbus_get_default_adapter
==============================================================*/
static boolean btces_pfal_dbus_get_default_adapter
(
  void
)
{
  int buffer_size = sizeof(g_btces_pfal_data.worker_thread_info.default_adapter);
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(BTCES_SUCCESS != btces_pfal_dbus_get_info(BTCES_PFAL_DBUS_DEFAULT_ADAPTER_INFO,
                                               &g_btces_pfal_data.worker_thread_info.default_adapter[0],
                                               &buffer_size))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_get_default_adapter(): could not get adapter" BTCES_EOL);
    return FALSE;
  }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_get_default_adapter(): adapter %s retrieved" BTCES_EOL,
                   g_btces_pfal_data.worker_thread_info.default_adapter);
  return TRUE;
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_get_dev_address_from_msg
==============================================================*/
static boolean btces_pfal_dbus_get_dev_address_from_msg
(
  uint8         *addr_ptr,
  DBusMessage   *msg_ptr
)
{
  const char    *obj_path_ptr = NULL;
  int            addr_cnt;
  char          *end_ptr = NULL;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if((NULL == addr_ptr) ||
     (NULL == msg_ptr))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_get_dev_address_from_msg(): NULL input" BTCES_EOL);
    return FALSE;
  }

  obj_path_ptr = dbus_message_get_path(msg_ptr);

  if(NULL == obj_path_ptr)
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_get_dev_address_from_msg(): NULL obj path" BTCES_EOL);
    return FALSE;
  }

  /** Convoluted, but necessary since the BlueZ dbus interface has
     become rather limited (BlueZ dbus Wiki -
     http://wiki.bluez.org/ - advertises much more than eclair
     implementation) So we try to get the object path of the
     message and derive the BT addr from there instead As a sanity
     check, we do try to consider cases where an invalid message
     (where dev obj path does not exist) is passed in Assume max
     size of the object_path is BTCES_MAX_OBJ_PATH_SIZE Verify the
     `well-known' /org/bluez/ first from the path. Then try to
     parse the path and retrieve dev_XX_XX_XX_XX_XX_XX, where
     XX_XX_XX_XX_XX_XX is the addr of interest
  */
  if(NULL == (end_ptr = strstr(obj_path_ptr,
                               BTCES_BLUEZ_PATH)))
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_get_dev_address_from_msg(): invalid obj path!" BTCES_EOL);
    return FALSE;
  }

  end_ptr += BTCES_BLUEZ_PATH_LEN;

  /* end_ptr is now past /org/bluez/ in obj_path_ptr */

  if(NULL == (end_ptr = strstr(end_ptr,
                               BTCES_DEV_STR)))
    {
    BTCES_MSG_ERROR("btces_pfal_dbus_get_dev_address_from_msg(): no dev in string!" BTCES_EOL);
    return FALSE;
  }

  end_ptr += BTCES_DEV_STR_LEN;

  /* end_ptr is now past "dev" in obj_path_ptr */

  /* Found a match for "dev" - derive address from here */
      for(addr_cnt = 0; addr_cnt < BTCES_BT_ADDR_SIZE; addr_cnt++)
      {
        /**
       Essentially the obj path at end_ptr should now look like:
       _XX_XX_XX_XX_XX_XX
        We then loop and pass XX_ to strtol,
        and skip these 3 characters for each iteration over addr_cnt. The
        last XX should be the end of the string. Assume strtol will treat the
        trailing _ as the end of the byte and adjust end_ptr accordingly for
        each new iteration to allow end_ptr to point to the next XX_ instead
        of _XX
           Base 16 since address is in hex (atoi does not work)
         */
        addr_ptr[addr_cnt] = (uint8) strtol(++end_ptr,
                                            &end_ptr,
                                            BTCES_BASE_HEX);
      }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_get_dev_address_from_msg(): msg obj path: %s" BTCES_EOL,
                   obj_path_ptr);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_get_dev_address_from_msg(): address retrieved: %x %x %x %x %x %x" BTCES_EOL,
                   addr_ptr[0], addr_ptr[1], addr_ptr[2], addr_ptr[3],
                   addr_ptr[4], addr_ptr[5]);
  return TRUE;
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_close
==============================================================*/
static BTCES_STATUS btces_pfal_dbus_close
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* If close_pipe fd are valid, close gracefully */
  if(0 <= g_btces_pfal_data.worker_thread_info.close_pipe_fd[0])
  {
    (void) close(g_btces_pfal_data.worker_thread_info.close_pipe_fd[0]);
    g_btces_pfal_data.worker_thread_info.close_pipe_fd[0] = -1;
  }
  if(0 <= g_btces_pfal_data.worker_thread_info.close_pipe_fd[1])
  {
    (void) close(g_btces_pfal_data.worker_thread_info.close_pipe_fd[1]);
    g_btces_pfal_data.worker_thread_info.close_pipe_fd[1] = -1;
  }
  /* If watch_pipe fd are valid, close gracefully */
  if(0 <= g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0])
  {
    (void) close(g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0]);
    g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0] = -1;
  }
  if(0 <= g_btces_pfal_data.worker_thread_info.watch_pipe_fd[1])
  {
    (void) close(g_btces_pfal_data.worker_thread_info.watch_pipe_fd[1]);
    g_btces_pfal_data.worker_thread_info.watch_pipe_fd[1] = -1;
  }

  /* If dbus fd is valid, close it gracefully */
  if(0 <= g_btces_pfal_data.worker_thread_info.dbus_fd)
  {
    (void) close(g_btces_pfal_data.worker_thread_info.dbus_fd);
    g_btces_pfal_data.worker_thread_info.dbus_fd = -1;
  }

  /* We do not disable the events we previously enabled - conn is going down */

  /* Reset the watch ptr */
  g_btces_pfal_data.worker_thread_info.watch_ptr = NULL;

  /* Unref the dbus connection (is this required?) */
  if(NULL != g_btces_pfal_data.worker_thread_info.conn_ptr)
  {
    dbus_connection_unref(g_btces_pfal_data.worker_thread_info.conn_ptr);
    g_btces_pfal_data.worker_thread_info.conn_ptr = NULL;
  }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_close(): DBUS close success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_process_dbus_event
==============================================================*/
static BTCES_STATUS btces_pfal_process_dbus_event
(
  void
)
{
  DBusMessage     *msg_ptr = NULL;
  int              signal_count;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /* Need to always handle the watch to let the fd know; false is not fatal */
  if(FALSE == dbus_watch_handle(g_btces_pfal_data.worker_thread_info.watch_ptr,
                                DBUS_WATCH_READABLE))
  {
    BTCES_MSG_ERROR("btces_pfal_process_dbus_event(): handle watch is FALSE" BTCES_EOL);
  }

  /* If we get in here, we have a message to pop- empty the queue */
  while(NULL != (msg_ptr = dbus_connection_pop_message(g_btces_pfal_data.worker_thread_info.conn_ptr)))
  {
    /* Process the message */
    BTCES_MSG_MEDIUM("btces_pfal_process_dbus_event(): popped msg = %s" BTCES_EOL, dbus_message_get_member(msg_ptr));

    for(signal_count = 0; signal_count < BTCES_MAX_DBUS_SIGNALS; signal_count++)
    {
      if(TRUE == dbus_message_is_signal(msg_ptr,
                                        g_btces_dbus_sig_table[signal_count].interface,
                                        g_btces_dbus_sig_table[signal_count].signal_name))
      {
        BTCES_MSG_MEDIUM("btces_pfal_process_dbus_event(): found signal match" BTCES_EOL);

        BTCES_ASSERT(NULL != g_btces_dbus_sig_table[signal_count].signal_handler);

        g_btces_dbus_sig_table[signal_count].signal_handler(msg_ptr);

        /* No need to continue loop */
        break;
      }
    }

    /* Free the dbus message */
    dbus_message_unref(msg_ptr);
  }

  BTCES_MSG_MEDIUM("btces_pfal_process_dbus_event(): done" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_adapter_added_sig_handler
==============================================================*/
static void btces_pfal_dbus_adapter_added_sig_handler
(
  DBusMessage *msg_ptr
)
{
  DBusError    bus_error;
  const char  *obj_path_ptr;
  int          dev_id;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_MSG_MEDIUM("btces_pfal_dbus: AdapterAdded" BTCES_EOL);

  if(NULL == msg_ptr)
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_added_sig_handler(): NULL msg!" BTCES_EOL);
  }

  dbus_error_init(&bus_error);

  if (!dbus_message_get_args(msg_ptr, &bus_error,
                             DBUS_TYPE_OBJECT_PATH, &obj_path_ptr,
                             DBUS_TYPE_INVALID))
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_added_sig_handler(): dbus_message_get_args() failed!" BTCES_EOL);
    dbus_error_free(&bus_error);
    return;
  }

  dbus_error_free(&bus_error);

  /* Get the device ID from the added adapter's path */
  dev_id = btces_pfal_get_dev_id_from_path(obj_path_ptr);

  if(0 > dev_id)
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_adapter_added_sig_handler(): could not get added dev_id" BTCES_EOL);
    return;
  }

  /* If previous HCI socket exists, see if the dev_id is the same as the added adapter */
  if(0 <= g_btces_pfal_data.worker_thread_info.hci_fd)
  {
    if(dev_id != btces_pfal_get_dev_id_from_path(&g_btces_pfal_data.worker_thread_info.default_adapter[0]))
    {
      BTCES_MSG_LOW("btces_pfal_dbus_adapter_added_sig_handler(): Added adapter differs from existing default adapter; done" BTCES_EOL);
      return;
    }

    BTCES_MSG_ERROR("btces_pfal_dbus_adapter_added_sig_handler(): default adapter re-added, turning off HCI!" BTCES_EOL);
    (void) btces_pfal_hci_close();
  }

  /* Since an adapter was added, try to get the default adapter */
  if(TRUE == btces_pfal_dbus_get_default_adapter())
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_added_sig_handler(): turning on HCI!" BTCES_EOL);

    if(BTCES_SUCCESS != btces_pfal_hci_open())
    {
      BTCES_MSG_ERROR("btces_pfal_dbus_adapter_added_sig_handler(): cannot open HCI!!" BTCES_EOL);
    }
  }
  else
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_adapter_added_sig_handler(): no default adapter!!" BTCES_EOL);
  }

  /* Notify the btces core about the native event */
  btces_svc_native_event_in(BTCES_NATIVE_EVENT_DEVICE_SWITCHED_ON,
                            NULL);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_added_sig_handler(): adapter add success" BTCES_EOL);
}


/*==============================================================
FUNCTION:  btces_pfal_dbus_adapter_removed_sig_handler
==============================================================*/
static void btces_pfal_dbus_adapter_removed_sig_handler
(
  DBusMessage *msg_ptr
)
{
  DBusError    bus_error;
  const char  *obj_path_ptr;
  int          dev_id;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_MSG_MEDIUM("btces_pfal_dbus(): AdapterRemoved" BTCES_EOL);

  if(NULL == msg_ptr)
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_removed_sig_handler(): NULL msg!" BTCES_EOL);
    return;
  }

  dbus_error_init(&bus_error);

  if (!dbus_message_get_args(msg_ptr, &bus_error,
                             DBUS_TYPE_OBJECT_PATH, &obj_path_ptr,
                             DBUS_TYPE_INVALID))
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_removed_sig_handler(): dbus_message_get_args() failed!" BTCES_EOL);
    dbus_error_free(&bus_error);
    return;
  }

  dbus_error_free(&bus_error);

  /* Get the device ID from the removed adapter's path */
  dev_id = btces_pfal_get_dev_id_from_path(obj_path_ptr);

  if(0 > dev_id)
  {
    BTCES_MSG_ERROR("btces_pfal_dbus_adapter_removed_sig_handler(): could not get removed dev_id" BTCES_EOL);
    return;
  }

  if(0 > g_btces_pfal_data.worker_thread_info.hci_fd)
  {
    BTCES_MSG_LOW("btces_pfal_dbus_adapter_removed_sig_handler(): Default HCI adapter not open; done" BTCES_EOL);
    return;
  }

  if(dev_id != btces_pfal_get_dev_id_from_path(&g_btces_pfal_data.worker_thread_info.default_adapter[0]))
  {
    BTCES_MSG_LOW("btces_pfal_dbus_adapter_removed_sig_handler(): Default adapter not removed; done" BTCES_EOL);
    return;
  }

  /* Remove the adapter info we currently have */
  memset(&g_btces_pfal_data.worker_thread_info.default_adapter[0],
         0,
         sizeof(g_btces_pfal_data.worker_thread_info.default_adapter));

  /* Notify the btces core about the native event */
  btces_svc_native_event_in(BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF,
                            NULL);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_removed_sig_handler(): adapter remove success" BTCES_EOL);

  (void) bt_coex_shim_close();
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_adapter_property_changed_sig_handler
==============================================================*/
static void btces_pfal_dbus_adapter_property_changed_sig_handler
(
  DBusMessage *msg_ptr
)
{
  DBusMessageIter iter, sub_iter;
  const char  *property;
  int powered;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_MSG_MEDIUM("btces_pfal_dbus: Adapter PropertyChanged" BTCES_EOL);

  if(NULL == msg_ptr)
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): NULL msg!" BTCES_EOL);
    return;
  }

  if(!dbus_message_iter_init(msg_ptr, &iter))
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): message has no args" BTCES_EOL);
  }

  if(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): unexpected signature in PropertyChanged signal" BTCES_EOL);
    return;
  }
  dbus_message_iter_get_basic(&iter, &property);

  /* Only process the Powered value change signals */
  if(property == NULL || strcmp(property, "Powered"))
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): event ignored" BTCES_EOL);
    return;
  }

  if(!dbus_message_iter_next(&iter))
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): unexpected signature in PropertyChanged signal" BTCES_EOL);
    return;
  }
  dbus_message_iter_recurse(&iter, &sub_iter);
  if(dbus_message_iter_get_arg_type(&sub_iter) != DBUS_TYPE_BOOLEAN)
  {
    BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): unexpected signature in PropertyChanged signal");
    return;
  }

  dbus_message_iter_get_basic(&sub_iter, &powered);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): powered %d" BTCES_EOL, powered);

  /* Notify the btces core about the native event */
  btces_svc_native_event_in((powered?BTCES_NATIVE_EVENT_DEVICE_SWITCHED_ON:BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF),NULL);

  if(!powered)
  {
    /* Start shutting down the BTC module */
    (void) bt_coex_shim_close();
  }

  BTCES_MSG_MEDIUM("btces_pfal_dbus_adapter_property_changed_sig_handler(): done processing" BTCES_EOL);

}

/*==============================================================
FUNCTION:  btces_pfal_dbus_audio_sink_playing_sig_handler
==============================================================*/
static void btces_pfal_dbus_audio_sink_playing_sig_handler
(
  DBusMessage *msg_ptr
)
{
  btces_native_event_data_union    event_data;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_MSG_MEDIUM("btces_pfal_dbus(): AudioSink Playing" BTCES_EOL);

  if(NULL == msg_ptr)
  {
    /* Any functions using msg_ptr will deal with a null ptr so don't bail */
    BTCES_MSG_MEDIUM("btces_pfal_dbus_audio_sink_playing_sig_handler(): NULL msg!" BTCES_EOL);
  }

  /* Retrieve the remote device address */
  if(FALSE == btces_pfal_dbus_get_dev_address_from_msg(&event_data.addr.addr[0],
                                                       msg_ptr))
  {
    /** Set the address to 0 to avoid confusion, but still propagate
        event. Note that this will simply end up being discarded by
        BTC-ES which cannot find an ACL connection match for this
        address
    */
    memset(&event_data.addr,
           0,
           sizeof(event_data.addr));
    BTCES_MSG_ERROR("btces_pfal_dbus_audio_sink_playing_sig_handler(): no remote addr!" BTCES_EOL);
  }

  /* Notify the btces core about the native event */
  btces_svc_native_event_in(BTCES_NATIVE_EVENT_A2DP_STREAM_START,
                            &event_data);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_audio_sink_playing_sig_handler(): audio sink playing success" BTCES_EOL);
}

/*==============================================================
FUNCTION:  btces_pfal_dbus_audio_sink_stopped_sig_handler
==============================================================*/
static void btces_pfal_dbus_audio_sink_stopped_sig_handler
(
  DBusMessage *msg_ptr
)
{
  btces_native_event_data_union    event_data;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_MSG_MEDIUM("btces_pfal_dbus(): AudioSink Stopped/Disconnected" BTCES_EOL);

  if(NULL == msg_ptr)
  {
    /* Any functions using msg_ptr will deal with a null ptr so don't bail */
    BTCES_MSG_MEDIUM("btces_pfal_dbus_audio_sink_stopped_sig_handler(): NULL msg!" BTCES_EOL);
  }

  /* Retrieve the remote device address */
  if(FALSE == btces_pfal_dbus_get_dev_address_from_msg(&event_data.addr.addr[0],
                                                       msg_ptr))
  {
    /** Set the address to 0 to avoid confusion, but still propagate
        event. Note that this will simply end up being discarded by
        BTC-ES which cannot find an ACL connection match for this
        address
    */
    memset(&event_data.addr,
           0,
           sizeof(event_data.addr));
    BTCES_MSG_ERROR("btces_pfal_dbus_audio_sink_stopped_sig_handler(): no remote addr!" BTCES_EOL);
  }

  /* Notify the btces core about the native event */
  btces_svc_native_event_in(BTCES_NATIVE_EVENT_A2DP_STREAM_STOP,
                            &event_data);

  BTCES_MSG_MEDIUM("btces_pfal_dbus_audio_sink_stopped_sig_handler(): audio sink stop success" BTCES_EOL);
}

/*==============================================================
FUNCTION:  btces_pfal_process_watch_event
==============================================================*/
static BTCES_STATUS btces_pfal_process_watch_event
(
  void
)
{
  int                            read_len;
  int                            old_fd;
  btces_pfal_watch_info_struct   watch_info;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Always read 1 int value from the watch pipe - we only expect the dbus fd */
  read_len = read(g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0],
                  &watch_info,
                  sizeof(watch_info));

  if(0 >= read_len)
  {
    BTCES_MSG_ERROR("btces_pfal_process_watch_event(): err reading from pipe" BTCES_EOL);
    return BTCES_FAIL;
  }

  BTCES_MSG_MEDIUM("btces_pfal_process_watch_event(): received watch_ptr: %p; flags: %d" BTCES_EOL,
                   watch_info.watch_ptr,
                   watch_info.flags);

  /* Store the watch ptr for handling the dbus_fd */
  g_btces_pfal_data.worker_thread_info.watch_ptr = watch_info.watch_ptr;

  /* Store the old fd to see if we need to update it */
  old_fd = g_btces_pfal_data.worker_thread_info.dbus_fd;

  /* Store the new value in the dbus_fd */
  g_btces_pfal_data.worker_thread_info.dbus_fd = watch_info.new_fd;

  /* If valid fd, then set this in the read_set; otherwise, clear the old one */
  if(0 <= watch_info.new_fd)
  {
    FD_SET(watch_info.new_fd,
           &g_btces_pfal_data.worker_thread_info.read_set);
  }
  else
  {
    if(0 <= old_fd)
    {
      FD_CLR(old_fd,
             &g_btces_pfal_data.worker_thread_info.read_set);
    }
  }

  BTCES_MSG_MEDIUM("btces_pfal_process_watch_event(): pipe read new fd: %d" BTCES_EOL,
                   g_btces_pfal_data.worker_thread_info.dbus_fd);
  return BTCES_SUCCESS;
}


/*==============================================================
FUNCTION:  btces_pfal_get_dev_id_from_path
==============================================================*/
static int btces_pfal_get_dev_id_from_path
(
  const char *object_path_ptr
)
{
  int dev_id = -1;
  char   *end_ptr = NULL;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(NULL == object_path_ptr)
  {
    BTCES_MSG_ERROR("btces_pfal_get_dev_id_from_path(): NULL ptr passed!" BTCES_EOL);
    return -1;
  }

  /** Assume max size of the object_path is BTCES_MAX_ADAPTER_SIZE
      Verify the `well-known' /org/bluez/ first from the path.
      Then try to parse the path and retrieve hciX, where X is the dev id of
      interest
      Assume dev id 0 <= X < 10
  */
  if(NULL == (end_ptr = strstr(object_path_ptr,
                               BTCES_BLUEZ_PATH)))
  {
    BTCES_MSG_ERROR("btces_pfal_get_dev_id_from_path(): invalid obj path!" BTCES_EOL);
    return -1;
  }

  end_ptr += BTCES_BLUEZ_PATH_LEN;

  /* end_ptr is now past /org/bluez/ */

  if(NULL == (end_ptr = strstr(end_ptr,
                               BTCES_HCI_STR)))
    {
    BTCES_MSG_ERROR("btces_pfal_get_dev_id_from_path(): no hci in string!" BTCES_EOL);
    return -1;
  }

  end_ptr += BTCES_HCI_STR_LEN;

  /* end_ptr is now past hci */

  dev_id = atoi(&end_ptr[0]);

  BTCES_MSG_MEDIUM("btces_pfal_get_dev_id_from_path(): object_path is: %s" BTCES_EOL,
                   object_path_ptr);
  BTCES_MSG_MEDIUM("btces_pfal_get_dev_id_from_path(): dev_id is: %d" BTCES_EOL,
                   dev_id);

  return dev_id;
}

/*==============================================================
FUNCTION:  btces_pfal_hci_open
==============================================================*/
static BTCES_STATUS btces_pfal_hci_open
(
  void
)
{
  struct sockaddr_hci   hci_addr;
  struct hci_filter     filter;
  int                   dev_id = -1;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(0 > (g_btces_pfal_data.worker_thread_info.hci_fd = socket(AF_BLUETOOTH, SOCK_RAW,
                                                               BTPROTO_HCI)))
  {
    BTCES_MSG_ERROR("btces_pfal_hci_open(): could not open socket" BTCES_EOL);
    return BTCES_FAIL;
  }

  /* Convert the object path into dev id */
  dev_id = btces_pfal_get_dev_id_from_path(&g_btces_pfal_data.worker_thread_info.default_adapter[0]);

  if(0 > dev_id)
  {
    BTCES_MSG_ERROR("btces_pfal_hci_open(): could not get dev_id" BTCES_EOL);
    return BTCES_FAIL;
  }

  memset(&hci_addr, 0, sizeof(hci_addr));

  /* The bind() needs to happen always before setsockopt() */
  hci_addr.hci_family = AF_BLUETOOTH;
  /* Device is the default retrieved from the adapter */
  hci_addr.hci_dev = (unsigned short)dev_id;

  if(0 > bind(g_btces_pfal_data.worker_thread_info.hci_fd,
              (struct sockaddr *)&hci_addr,
              sizeof(hci_addr)))
  {
    BTCES_MSG_ERROR("btces_pfal_hci_open(): could not bind" BTCES_EOL);
    return BTCES_FAIL;
  }

  /* Set up the filter over the socket */
  hci_filter_clear(&filter);
  hci_filter_all_ptypes(&filter);
  hci_filter_all_events(&filter);

  /* Specifically exclude the non-command and non-event packet types */
  hci_filter_clear_ptype(HCI_ACLDATA_PKT,
                         &filter);
  hci_filter_clear_ptype(HCI_SCODATA_PKT,
                         &filter);

  /* Specifically exclude the "number of completed packets" event */
  hci_filter_clear_event(EVT_NUM_COMP_PKTS,
                         &filter);

  if(0 > setsockopt(g_btces_pfal_data.worker_thread_info.hci_fd,
                    SOL_HCI,
                    HCI_FILTER,
                    &filter,
                    sizeof(filter)))
  {
    BTCES_MSG_ERROR("btces_pfal_hci_open(): could not set filter" BTCES_EOL);
    return BTCES_FAIL;
  }

  /* Socket is completely ready to start receiving events */

  /* Add the hci_fd to the file set for select() operation */
  FD_SET(g_btces_pfal_data.worker_thread_info.hci_fd,
         &g_btces_pfal_data.worker_thread_info.read_set);

  /* To help the WLAN chan propagation, open an HCI lib device and store it */
  g_btces_pfal_data.worker_thread_info.hci_lib_dd = hci_open_dev(dev_id);

  if(0 > g_btces_pfal_data.worker_thread_info.hci_lib_dd)
  {
    /* This means WLAN chan cannot be propagated, but not fatal for coex */
    BTCES_MSG_ERROR("btces_pfal_hci_open(): could not open hci lib dd (not fatal)" BTCES_EOL);
  }

  BTCES_MSG_MEDIUM("btces_pfal_hci_open(): HCI open success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_hci_close
==============================================================*/
static BTCES_STATUS btces_pfal_hci_close
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* If socket fd is valid, close it gracefully */
  if(0 <= g_btces_pfal_data.worker_thread_info.hci_fd)
  {
    /* Clear from the read set */
    FD_CLR(g_btces_pfal_data.worker_thread_info.hci_fd,
           &g_btces_pfal_data.worker_thread_info.read_set);

    (void) close(g_btces_pfal_data.worker_thread_info.hci_fd);
    g_btces_pfal_data.worker_thread_info.hci_fd = -1;
  }

  if(0 <= g_btces_pfal_data.worker_thread_info.hci_lib_dd)
  {
    (void) hci_close_dev(g_btces_pfal_data.worker_thread_info.hci_lib_dd);
    g_btces_pfal_data.worker_thread_info.hci_lib_dd = -1;
  }

  BTCES_MSG_MEDIUM("btces_pfal_hci_close(): HCI close success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_hci_process_socket_event
==============================================================*/
static BTCES_STATUS btces_pfal_hci_process_socket_event
(
  void
)
{
  int read_len = 0;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Read from the HCI socket */
  read_len = read(g_btces_pfal_data.worker_thread_info.hci_fd,
                  &g_btces_pfal_data.worker_thread_info.hci_socket_buf[0],
                  sizeof(g_btces_pfal_data.worker_thread_info.hci_socket_buf));

  if(0 >= read_len)
  {
    BTCES_MSG_ERROR("btces_pfal_hci_process_socket_event(): err reading from socket" BTCES_EOL);
    return BTCES_FAIL;
  }

  /* If there is at least one byte, we can identify a cmd or event to process */
  /* Note: byte 0 should be the type of data */
  switch(g_btces_pfal_data.worker_thread_info.hci_socket_buf[0])
  {
    case HCI_COMMAND_PKT:
      /* Propagate to core BTCES directly */
      btces_svc_hci_command_in((uint8 *)&g_btces_pfal_data.worker_thread_info.hci_socket_buf[1],
                               read_len-1);
      break;

    case HCI_EVENT_PKT:
      /* Propagate to core BTCES directly */
      btces_svc_hci_event_in((uint8 *)&g_btces_pfal_data.worker_thread_info.hci_socket_buf[1],
                             read_len-1);
      break;

    /* We do not currently care about sco data and acl data (or vendor) pkts */
    default:
      BTCES_MSG_LOW("btces_pfal_hci_process_socket_event(): unknown pkt" BTCES_EOL);
  }

  BTCES_MSG_MEDIUM("btces_pfal_hci_process_socket_event(): HCI read success" BTCES_EOL);
  return BTCES_SUCCESS;
}


/*==============================================================
FUNCTION:  btces_pfal_update_afh_map
==============================================================*/
static BTCES_STATUS btces_pfal_update_afh_map
(
  uint8 *afh_mask_ptr
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /**
     We take the route of using a device descriptor that was opened in the
     worker thread at the time that we detected a new HCI device added

     Since this read is happening in the BTC SVC thread, this could result in
     race conditions.
     We live with this knowing the following:
     - this should be protected by BTC-ES grabbing the PFAL token
     - even if it is not, we only *read* the dd here so in the worst case, after
       the following check, the dd gets wiped out, HCI lib rejects all further
       HCI requests and that's it. In this case, if there is no device it's okay
       for the requests to fail anyway (coex is meaningless) so this should be
       fine

     Note: we also separate the dd from the hci_fd for two reasons:
     - it keeps the socket for our HCI events/commands read clean of any bytes
       from the WLAN channel operations: easier debugging
     - if we re-use the hci_fd for the following operations, we also risk losing
       out some HCI events because when a command is issued via HCI lib, it
       updates the event filters, etc for the response. So the alternative is to
       avoid using HCI lib which while doable is just duplicating logic. The
       following approach immensely simplifies the PFAL implementation.
  */
  if(0 > g_btces_pfal_data.worker_thread_info.hci_lib_dd)
  {
    BTCES_MSG_ERROR("btces_pfal_update_afh_map(): No device for operation" BTCES_EOL);
    return BTCES_STATUS_INVALID_STATE;
  }

  if(0 > hci_set_afh_classification(g_btces_pfal_data.worker_thread_info.hci_lib_dd,
                                    afh_mask_ptr,
                                    BTCES_HCI_LIB_TIMEOUT))
  {
    BTCES_MSG_ERROR("btces_pfal_update_afh_map(): HCI request failed" BTCES_EOL);
    return BTCES_FAIL;
  }

  BTCES_MSG_MEDIUM("btces_pfal_update_afh_map(): AFH map update success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_update_ca_mode
==============================================================*/
static BTCES_STATUS btces_pfal_update_ca_mode
(
  /* TRUE --> turn off CA; FALSE --> turn on CA (if applicable) */
  boolean  turn_off_ca
)
{
  uint8   ca_mode;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /**
     We take the route of using a device descriptor that was opened in the
     worker thread at the time that we detected a new HCI device added

     Since this read is happening in the BTC SVC thread, this could result in
     race conditions.
     We live with this knowing the following:
     - this should be protected by BTC-ES grabbing the PFAL token
     - even if it is not, we only *read* the dd here so in the worst case, after
       the following check, the dd gets wiped out, HCI lib rejects all further
       HCI requests and that's it. In this case, if there is no device it's okay
       for the requests to fail anyway (coex is meaningless) so this should be
       fine

     Note: we also separate the dd from the hci_fd for two reasons:
     - it keeps the socket for our HCI events/commands read clean of any bytes
       from the WLAN channel operations: easier debugging
     - if we re-use the hci_fd for the following operations, we also risk losing
       out some HCI events because when a command is issued via HCI lib, it
       updates the event filters, etc for the response. So the alternative is to
       avoid using HCI lib which while doable is just duplicating logic. The
       following approach immensely simplifies the PFAL implementation.
  */
  if(0 > g_btces_pfal_data.worker_thread_info.hci_lib_dd)
  {
    BTCES_MSG_MEDIUM("btces_pfal_update_ca_mode(): No device for operation" BTCES_EOL);
    return BTCES_STATUS_INVALID_STATE;
  }

  if(TRUE == turn_off_ca)
  {
    /* Check if need to read from BlueZ */
    if(TRUE == g_btces_pfal_data.read_ca_from_bluez)
    {
      /* Read current state from BlueZ, store it and force CA off */
      if(0 > hci_read_afh_mode(g_btces_pfal_data.worker_thread_info.hci_lib_dd,
                               &ca_mode,
                               BTCES_HCI_LIB_TIMEOUT))
      {
        BTCES_MSG_ERROR("btces_pfal_update_ca_mode(): Read AFH failed" BTCES_EOL);
        return BTCES_FAIL;
      }
      /* If it's off no action needed (so keep it as UNKNOWN) */
      g_btces_pfal_data.initial_ca_mode = (CA_MODE_OFF == ca_mode) ? CA_MODE_UNKNOWN : CA_MODE_ON;
    }
    else
    {
      /* Just a sanity check */
      if(CA_MODE_UNKNOWN == g_btces_pfal_data.initial_ca_mode)
      {
        BTCES_MSG_ERROR("btces_pfal_update_ca_mode(): Unknown user config" BTCES_EOL);
        g_btces_pfal_data.initial_ca_mode = CA_MODE_ON;
      }
    }

    /* If the initial mode was on, only then write back */
    if(CA_MODE_ON == g_btces_pfal_data.initial_ca_mode)
    {
      ca_mode = CA_MODE_OFF;

      if(0 > hci_write_afh_mode(g_btces_pfal_data.worker_thread_info.hci_lib_dd,
                                ca_mode,
                                BTCES_HCI_LIB_TIMEOUT))
      {
        BTCES_MSG_ERROR("btces_pfal_update_ca_mode(): Turn off CA failed" BTCES_EOL);
        return BTCES_FAIL;
      }
    }
  }
  else
  {
    /* Restore previous state to BlueZ if needed */
    if(CA_MODE_ON == g_btces_pfal_data.initial_ca_mode)
    {
      /* Turn it back on */
      ca_mode = CA_MODE_ON;

      if(0 > hci_write_afh_mode(g_btces_pfal_data.worker_thread_info.hci_lib_dd,
                                ca_mode,
                                BTCES_HCI_LIB_TIMEOUT))
      {
        BTCES_MSG_ERROR("btces_pfal_update_ca_mode(): Turn on CA failed" BTCES_EOL);
        return BTCES_FAIL;
      }

      /* Check if previously read from BlueZ : if so, reset initial_ca_mode */
      if(TRUE == g_btces_pfal_data.read_ca_from_bluez)
      {
        /* Reset initial ca mode for next operation */
        g_btces_pfal_data.initial_ca_mode = CA_MODE_UNKNOWN;
      }
    }
  }

  BTCES_MSG_MEDIUM("btces_pfal_update_ca_mode(): CA mode update success" BTCES_EOL);
  return BTCES_SUCCESS;
}

/*==============================================================
FUNCTION:  btces_pfal_timer_notify_callback
==============================================================*/
static void btces_pfal_timer_notify_callback
(
  union sigval sig_value
)
{
  btces_pfal_timer_struct *timer_ptr;

  void                      *client_user_data;
  btces_pfal_timer_cb_type  *client_cb;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Retrieve previously allocated timer and sanity check */
  timer_ptr = (btces_pfal_timer_struct *) sig_value.sival_ptr;

  if(NULL == timer_ptr)
  {
    BTCES_MSG_ERROR("btces_pfal_timer_notify_callback(): NULL ptr passed!" BTCES_EOL);
    return;
  }

  /* Should not normally happen (leave as debug assert only)*/
  BTCES_ASSERT(BTCES_COOKIE == timer_ptr->cookie);

  /* Retrieve client info before freeing memory */
  client_cb = timer_ptr->client_callback;
  client_user_data = timer_ptr->client_user_data;

  /* Free the timer */
  (void) timer_delete(timer_ptr->timer);

  /* Free the memory allocated */
  free(timer_ptr);

  /* Invoke the client callback */
  if(NULL != client_cb)
  {
    BTCES_MSG_MEDIUM("btces_pfal_timer_notify_callback(): notifying client" BTCES_EOL);
    client_cb(client_user_data);
  }

}

/*==============================================================
FUNCTION:  bt_wlan_coex_update_afh_mask
==============================================================*/
/** Update a working copy of an AFH channel mask for one WLAN channel */
static void bt_wlan_coex_update_afh_mask
(
  uint8    wlan_chan_num, /**< WLAN Channel number, 1-14. 0 is invalid. */
  uint8    *afh_mask_ptr  /**< AFH Channel mask array, 10 bytes for 79 bits */
)
{
  uint16             wlan_freq, bt_chan_num, bt_chan_guard;
  uint8              i;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if((wlan_chan_num == 0) || (wlan_chan_num > 14) || (afh_mask_ptr == NULL))
  {
    return;   /* Leave if any parameters are invalid*/
  }

  /* Update the AHF mask bitmap passed in for the given WLAN channel */

  /* Convert WLAN channel number (1-14) to frequency in MHz:
     Channels 1 through 13 are spaced 5 MHz apart;
     Channel 14 is a special value
  */
  if(wlan_chan_num <= 13)
  {
    wlan_freq = WLAN_80211_RF_CH_1_MHZ +
                (WLAN_80211_RF_CH_SPACING_MHZ * (wlan_chan_num - 1));
  }
  else
  {
    wlan_freq = WLAN_80211_RF_CH_14_MHZ;
  }

  BTCES_MSG_LOW("btces_pfal_wlan_chan(): Masking for WLAN freq %d" BTCES_EOL, wlan_freq);

  /* Calculate Bluetooth channel number from the WLAN frequency:
     Since Bluetooth channels are on whole MHz boundaries, and are spaced 1 MHz
     apart, just subtract off the starting frequency of BT channels
  */
  bt_chan_num = wlan_freq - BT_RF_CHANNEL_0_MHZ;

  /* For each of the 79 Bluetooth channels, if the Bluetooth frequency is within
     a guard band of the WLAN frequency, mark it as not to be used (0). Because
     of the spacing of BT channels, BT channel numbers can be directly compared
     to the BT channel WLAN is centered on for their proximity instead of
     comparing raw frequencies.
  */

  bt_chan_guard = BT_DC_AFH_CH_EXCLUDE; /* This value could come from elsewhere */

  for(i = 0; i < 79; i++)
  {
    if(abs(i - bt_chan_num) <= bt_chan_guard)
    {
      // BTCES_MSG_LOW("btces_pfal_wlan_chan(): BT channel %d masked." BTCES_EOL, i );
      afh_mask_ptr[ i/8 ] &= ~( 1 << ( i % 8 ) );
    }
  }
} /* bt_wlan_coex_update_afh_mask */

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/

/*==============================================================
FUNCTION:  btces_pfal_init
==============================================================*/
/** Initialize the BTC-ES PFAL layer for this platform. See btces_pfal.h for details. */

BTCES_STATUS btces_pfal_init
(
  void
)
{
  BTCES_STATUS rval = BTCES_STATUS_ALREADY_INITIALIZED;

  pthread_mutexattr_t mutex_attr;
  pthread_attr_t  attr;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(!g_btces_pfal_data.initialized)
  {
    /* Simple init for now - don't care about return code */
    (void) btces_pfal_init_control_block();

    /* Check against user data to determine the CA behavior */
    (void) btces_pfal_configure_ca_support();

    /* Initialize mutex attributes */
    (void) pthread_mutexattr_init(&mutex_attr);

    (void) pthread_mutexattr_settype(&mutex_attr,
                                     PTHREAD_MUTEX_RECURSIVE);

    /* Initialize mutex (token) for use of BTCES PFAL clients */
    if(0 != pthread_mutex_init(&g_btces_pfal_data.client_token,
                               &mutex_attr))
    {
      (void) pthread_mutexattr_destroy(&mutex_attr);
      return BTCES_STATUS_INITIALIZATION_FAILED;
    }

    /* Destroy mutex attributes */
    (void) pthread_mutexattr_destroy(&mutex_attr);

    /* Set flag to indicate that worker thread should not close */
    g_btces_pfal_data.worker_thread_info.close_worker_thread = FALSE;

    /* Initialize worker thread attributes: must be joinable */
    (void) pthread_attr_init(&attr);

    (void) pthread_attr_setdetachstate(&attr,
                                       PTHREAD_CREATE_JOINABLE);

    /* Set this prior to creating thread (new thread might be higher pri) */
    g_btces_pfal_data.initialized = TRUE;

    /* Start worker thread */
    if(0 != pthread_create(&g_btces_pfal_data.worker_thread_info.thread_handle,
                           &attr,
                           btces_pfal_worker_thread,
                           NULL))
    {
      BTCES_MSG_ERROR("pfal_init(): worker thread creation failure" BTCES_EOL);

      /* Also destroy mutex */
      (void) pthread_mutex_destroy(&g_btces_pfal_data.client_token);

      /* Free the attribute (ignore return value) */
      (void) pthread_attr_destroy(&attr);

      return BTCES_STATUS_INITIALIZATION_FAILED;
    }

    /* Free the attribute (ignore return value) */
    (void) pthread_attr_destroy(&attr);

    BTCES_MSG_MEDIUM("pfal_init(): init success" BTCES_EOL);

    rval = BTCES_SUCCESS;
  }

  return rval;
} /* btces_pfal_init */

/*==============================================================
FUNCTION:  btces_pfal_deinit
==============================================================*/
/** De-initialize the BTC-ES PFAL layer for this platform. See btces_pfal.h for details. */

void btces_pfal_deinit
(
  void
)
{
  const unsigned int dummy_close = 0xC;  /* Dummy variable to write to close */
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  if(g_btces_pfal_data.initialized)
  {
    g_btces_pfal_data.initialized = FALSE;

    /* Notify worker thread to close (via pipe 1) */
    (void) write(g_btces_pfal_data.worker_thread_info.close_pipe_fd[1],
                 (void *)&dummy_close,
                 sizeof(dummy_close));

    /* This function is required to release the BTC-ES PFAL token, as callers like
       btces_deinit() have captured the token, and so presume this API releases it.

       Note: this does not guarantee that lock is unused if another thread is
       currently holding the lock
    */
    (void) pthread_mutex_unlock(&g_btces_pfal_data.client_token);

    /* Destroy the mutex (do we need to do this after joining the worker and
       main threads?)*/
    (void) pthread_mutex_destroy(&g_btces_pfal_data.client_token);

    /* No need to cancel worker thread as that should exit gracefully */
  }

  BTCES_MSG_MEDIUM("pfal_deinit(): deinit success" BTCES_EOL);

} /* btces_pfal_deinit */

/*==============================================================
FUNCTION:  btces_pfal_get_bt_power
==============================================================*/
/** Get the current power state of Bluetooth. See btces_pfal.h for details. */

BTCES_STATUS btces_pfal_get_bt_power
(
  int *bt_power_ptr
  /**< [out]: Pointer to where to store the Bluetooth power state;
     zero means "Off", non-zero means "On".
  */
)
{
  BTCES_STATUS  ret_val = BTCES_STATUS_INVALID_PARAMETERS;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(bt_power_ptr != NULL)
  {
    /* For Android+BlueZ, BTC-ES is initialized when Bluetooth daemon is
       started. At this point, we declare power-off (since this get_bt_power API
       seems to be called at btces init time). Subsequently native events should
       kick in and cause BT power to go on (alternatively, the first events
       should cause the same effect)
       Additionally, WLAN should not be looking at the 3-wire PTA until the SoC
       is really up and getting HCI commands anyway; it is better for WLAN to
       err on the side of thinking that Bluetooth is off instead of looking at
       the PTA lines that are not being driven.
    */

    /* We rely on the HCI fd being valid to determine power state */
    *bt_power_ptr = (0 > g_btces_pfal_data.worker_thread_info.hci_fd) ? 0 : 1;
    ret_val = BTCES_SUCCESS;
  }

  return  ret_val;
} /* btces_pfal_get_bt_power */

/*==============================================================
FUNCTION:  btces_pfal_malloc
==============================================================*/
/** Allocate an arbitrary memory block. See btces_pfal.h for details. */

void * btces_pfal_malloc
(
  int size
  /**< [in]: The requested size, in bytes, of the memory block to be allocated. */
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  return (void *)malloc(size);
} /* btces_pfal_malloc */

/*==============================================================
FUNCTION:  btces_pfal_free
==============================================================*/
/** Free an allocated memory block. See btces_pfal.h for details. */

void btces_pfal_free
(
  void *mem_ptr
  /**< [in]: A pointer to the memory block to be freed; a value of NULL is ignored. */
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  free(mem_ptr);
} /* btces_pfal_free */

/*==============================================================
FUNCTION:  btces_pfal_get_token
==============================================================*/
/** Capture a mutex for BTC-ES. See btces_pfal.h for details. */

BTCES_STATUS btces_pfal_get_token
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Pthread mutex `owned' by PFAL for BlueZ and offered to clients
   *  We try to optimize a little here and avoid race conditions by relying on
   *  lock to be initialized correctly if PFAL is initialized (and minimizing
   *  checks)
  */
  if(0 != pthread_mutex_lock(&g_btces_pfal_data.client_token))
  {
    return BTCES_FAIL;
  }

  return BTCES_SUCCESS;
} /* btces_pfal_get_token */

/*==============================================================
FUNCTION:  btces_pfal_release_token
==============================================================*/
/** Release the BTC-ES mutex. See btces_pfal.h for details. */

void btces_pfal_release_token
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Pthread mutex `owned' by PFAL for BlueZ and offered to clients
   *  We try to optimize a little here and avoid race conditions by relying on
   *  lock to be initialized correctly if PFAL is initialized (and minimizing
   *  checks)
  */
  pthread_mutex_unlock(&g_btces_pfal_data.client_token);

} /* btces_pfal_release_token */

/*==============================================================
FUNCTION:  btces_pfal_start_timer
==============================================================*/
/** Start a timer with a timeout callback function. See btces_pfal.h for details. */

BTCES_STATUS btces_pfal_start_timer
(
  uint16  timeout_ms,
  /**< [in]: Time value in milliseconds when the timer is to
             expire and the callback executed. */
  btces_pfal_timer_cb_type  *timer_cb_ptr,
  /**< [in]: Pointer a service to be called by BTC-ES when the timer
             expires. The pointer cannot be NULL. The callback
             service is described by btces_pfal_timer_cb_type */
  void  *user_data,
  /**< [in]: Opaque user-supplied data. This same value will always
             be passed to the callback service indicated by timer_cb_ptr. */
  void  **timer_id_ptr
  /**< [out]: Pointer to opaque platform-supplied timer ID. This same value will
              be used if the timer is canceled via btces_pfal_stop_timer(). */
)
{
  BTCES_STATUS rval = BTCES_FAIL;

  btces_pfal_timer_struct  *timer_ptr = NULL;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(!g_btces_pfal_data.initialized)
  {
    return BTCES_STATUS_NOT_INITIALIZED;
  }

  if((0 == timeout_ms) || (NULL == timer_cb_ptr) || (NULL == timer_id_ptr))
  {
    return BTCES_STATUS_INVALID_PARAMETERS;
  }

  /* Allocate timer structure */
  timer_ptr = (btces_pfal_timer_struct *) malloc(sizeof(btces_pfal_timer_struct));

  if(NULL == timer_ptr)
  {
    return BTCES_STATUS_OUT_OF_MEMORY;
  }

  /* Mark the cookie before we start */
  timer_ptr->cookie = BTCES_COOKIE;

  /** Set the event fields as follows:
      sigev_value: timer we just allocated
      sigev_notify: SIGEV_THREAD
      sigev_notify_function: pfal callback
      sigev_notify_attributes: NULL
  */
  timer_ptr->event.sigev_notify = SIGEV_THREAD;
  timer_ptr->event.sigev_notify_attributes = NULL;
  timer_ptr->event.sigev_notify_function = btces_pfal_timer_notify_callback;
  timer_ptr->event.sigev_value.sival_ptr = timer_ptr;
  timer_ptr->event.sigev_signo = 0;

  /* Create the timer */
  if(0 != timer_create(CLOCK_REALTIME,
                       &(timer_ptr->event),
                       &(timer_ptr->timer)))
  {
    BTCES_MSG_ERROR("btces_pfal_start_timer(): Failed to create timer!" BTCES_EOL);

    /* Free the previously allocated memory */
    free(timer_ptr);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  /* Fill in the client info in the structure */
  timer_ptr->client_callback = timer_cb_ptr;
  timer_ptr->client_user_data = user_data;

  /* Set the timer values (convert from ms): non-interval timer only */
  timer_ptr->timer_spec.it_interval.tv_sec = 0;
  timer_ptr->timer_spec.it_interval.tv_nsec = 0;
  timer_ptr->timer_spec.it_value.tv_sec =  BTCES_SEC_FROM_MS(timeout_ms);
  timer_ptr->timer_spec.it_value.tv_nsec =  BTCES_NS_FROM_MS(timeout_ms);

  /* Finally arm the timer */
  if(0 != timer_settime(timer_ptr->timer,
                        0,
                        &(timer_ptr->timer_spec),
                        NULL))
  {
    BTCES_MSG_ERROR("btces_pfal_start_timer(): Failed to set timer!" BTCES_EOL);

    /* Delete the timer */
    (void) timer_delete(timer_ptr->timer);

    /* Free the previously allocated memory */
    free(timer_ptr);
    return BTCES_STATUS_INITIALIZATION_FAILED;
  }

  /* Set the timer_id ptr */
  *timer_id_ptr = (void *)timer_ptr;

  BTCES_MSG_MEDIUM("btces_pfal_start_timer(): Scheduled timer_ptr: %p!" BTCES_EOL,
                   (void *)timer_ptr);

  rval = BTCES_SUCCESS;

  return rval;
} /* btces_pfal_start_timer */

/*==============================================================
FUNCTION:  btces_pfal_stop_timer
==============================================================*/
/** Cancel a running timer. See btces_pfal.h for details. */

void btces_pfal_stop_timer
(
  void  *timer_id
  /**< [in]: Opaque timer ID, originally returned by btces_pfal_start_timer().
             If the specified timer is no longer running, there are no side
             effects visible to BTC-ES. */
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  if(NULL != timer_id)
  {
    /**
       For now, we ignore the stop altogether because:
       a) it does not need to be used (there's more reliance on timer callback
       firing)
       b) in order to protect race conditions, we would need to introduce extra
       protection between the stop_timer and the callback being invoked, which
       seems superfluous given the current btces usage of timers

       In other words, every timer started will invariably fire and this is okay
       since the client for a timer is expected to handle this case anyway.
     */
    BTCES_MSG_LOW("btces_pfal_stop_timer: no-op on %p" BTCES_EOL,
                  timer_id);
  }

} /* btces_pfal_stop_timer */

/*==============================================================
FUNCTION:  btces_pfal_wlan_chan
==============================================================*/
/** Tell the Bluetooth SoC about WLAN channels in use. See btces_pfal.h for details. */

BTCES_STATUS btces_pfal_wlan_chan
(
  uint16 wlan_channels  /** [in] Bitmap of WLAN channels in use */
)
{
  uint8 afh_mask[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F }; /**< AFH mask array of 79 Bluetooth channel bits */
  uint8 channel_number;
  uint8 num_wlan_chans;
  uint8 num_bt_chans;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if(FALSE == g_btces_pfal_data.initialized)
  {
    BTCES_MSG_ERROR("btces_pfal_wlan_chan(): Not initialized!" BTCES_EOL);
    return BTCES_STATUS_NOT_INITIALIZED;
  }

  if(wlan_channels & BTCES_INVALID_WLAN_CHANS)
  {
    BTCES_MSG_ERROR("btces_pfal_wlan_chan(): Invalid channels!" BTCES_EOL);
    return BTCES_STATUS_INVALID_PARAMETERS;
  }

  BTCES_MSG_LOW("BTC-ES PFAL: WLAN Channels = 0x%04X" BTCES_EOL,
                wlan_channels);

  /* For each channel bit set in wlan_channels, update the AFH mask; if WLAN is
     inactive, the AFH mask will be left in its initialized state
  */

  for(channel_number = 1, num_wlan_chans = 0;
       wlan_channels != 0;
       channel_number++, wlan_channels >>= 1)
  {
    if(wlan_channels & 0x0001)
    {
      bt_wlan_coex_update_afh_mask(channel_number,
                                   afh_mask);
      num_wlan_chans++;  /* Count how many WLAN channels are in use */
    }
  }

  /* If there is more than 1 WLAN channel in use, check if there are still
     at least BT_N_MIN usable Bluetooth channels (Nmin is from the BT spec).
     The > 1 test presumes the guard band is such that a single WLAN channel
     in use leaves BT_N_MIN or more enabled (BT_DC_AFH_CH_EXCLUDE is <= 29).
  */
  if(num_wlan_chans > 1)
  {
    for(channel_number = 0, num_bt_chans = 0;
         (channel_number < 79) && (num_bt_chans < BT_N_MIN);
         channel_number++)
    {
      if(afh_mask[channel_number/8] & (1 << (channel_number % 8)))
      {
        num_bt_chans++;  /* Loop terminates when this reaches BT_N_MIN */
      }
    }

    if(num_bt_chans < BT_N_MIN)
    {
      BTCES_MSG_ERROR("btces_pfal_wlan_chan(): Not enough BT channels left after AFH!" BTCES_EOL);
      /* What else to do here? */
    }
  }

  /* Turn on/off Channel assessment with BlueZ if needed  */
  if(TRUE == g_btces_pfal_data.turn_off_ca_if_wlan)
  {
    /* At this point, if num_wlan_chans is non-zero, this means WLAN is active,
       so Channel Assessment needs to be turned off.
    */
    if(0 != num_wlan_chans)
    {
      /* Do not care about return condition (AFH map is the main focus) */
      (void) btces_pfal_update_ca_mode(TRUE);
    }
    else /* WLAN is now inactive */
    {
      /* Do not care about return condition (AFH map is the main focus) */
      (void) btces_pfal_update_ca_mode(FALSE);
    }
  }  /* End turn_off_ca_if_wlan operations */

  /* Send the AFH command via BlueZ lib. We don't care when it completes
  */
  if(BTCES_SUCCESS != btces_pfal_update_afh_map(afh_mask))
  {
    BTCES_MSG_ERROR("btces_pfal_wlan_chan(): Error updating AFH map!" BTCES_EOL);
    return BTCES_FAIL;
  }

  return BTCES_OK;
} /* btces_pfal_wlan_chan */



/*----------------------------------------------------------------------------
 * Daemon-related Function Definitions
 *
 * The following functions are `external
 * functions' that are not exposed via
 * btces_pfal.h
 *
 * These functions do not have a "btces" prefix
 * -------------------------------------------------------------------------*/

/**
FUNCTION sig_hdlr()

@brief
  This is the handler of all signals for the BTCES module.

  Upon term signal, BTCES will be shutdown.

@param sig : signal to be handled

@return None

DEPENDENCIES

SIDE EFFECTS
  None
*/
void sig_hdlr
(
  int sig
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  switch(sig)
  {
    case SIGINT:
      /* Fall through */
    case SIGTERM:
      BTCES_MSG_HIGH("SIGTERM/SIGINT received: one last notification to BTCES" BTCES_EOL);
      /* One last notification to BTCES */
      btces_svc_native_event_in(BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF,NULL);
      BTCES_MSG_HIGH("SIGTERM/SIGINT received, shutting down btces" BTCES_EOL);

      /* Call the shim close function */
      bt_coex_shim_close();
      break;

    default:
      BTCES_MSG_HIGH("unhandled signal %d" BTCES_EOL,
                     sig );
  }

}


/**
FUNCTION usage()

@brief
  Prints out options available to the user for btces

DEPENDENCIES

SIDE EFFECTS
  None
*/
void usage
(
  void
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  BTCES_MSG_MEDIUM("btces options: " BTCES_EOL);
  BTCES_MSG_MEDIUM("             -o Daemon mode off " BTCES_EOL);
  BTCES_MSG_MEDIUM("             -c Turn off CA if WLAN " BTCES_EOL);
  BTCES_MSG_MEDIUM("             -r Read CA mode from chip " BTCES_EOL);
  BTCES_MSG_MEDIUM("             -i Initial CA mode off (on by default) " BTCES_EOL);
  BTCES_MSG_MEDIUM("             -h Help " BTCES_EOL);
}


/**
FUNCTION main()

@brief
  This is the main entry point into the BTCES executable.

  This function should be called when BT services are required. This in
  turn triggers set up of the BTC-ES logic and causes registration with
  DBUS for relevant events.

@param argc : number of cmd line args

@param argv : array of cmd line args

@return status of the call

DEPENDENCIES

SIDE EFFECTS
  None
*/
int main
(
  int argc,
  char *argv[]
)
{
  int daemonize = 1; /* daemon on by default */
  int opt;
  struct sigaction sig_act;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Initialize the user data */
  g_btces_pfal_user_data.read_ca_from_bluez = FALSE;
  g_btces_pfal_user_data.turn_off_ca_if_wlan = FALSE;
  g_btces_pfal_user_data.initial_ca_mode = CA_MODE_UNKNOWN;

  /* Read args */
  while((opt = getopt(argc,
                      argv,
                      "ohcri")) != EOF)
  {
    switch(opt)
    {
      case 'o':
        daemonize = 0;
        break;

      case 'c':
        BTCES_MSG_HIGH("Turn off CA if WLAN" BTCES_EOL);
        g_btces_pfal_user_data.turn_off_ca_if_wlan = TRUE;
        break;

      case 'r':
        BTCES_MSG_HIGH("Read CA mode from BlueZ" BTCES_EOL);
        g_btces_pfal_user_data.read_ca_from_bluez = TRUE;
        break;

      case 'i':
        BTCES_MSG_HIGH("Initial CA mode off" BTCES_EOL);
        g_btces_pfal_user_data.initial_ca_mode = CA_MODE_OFF;
        break;

      default:
        usage();
        exit(0);
    }
  }

  BTCES_MSG_HIGH("Starting BTCES" BTCES_EOL);

  if(daemonize)
  {
    /* Convert to daemon */
    BTCES_MSG_HIGH("Daemonizing %s" BTCES_EOL, BTCES_DAEMON_NAME);

    if(daemon(0,
              0))
    {
      BTCES_MSG_ERROR("Error starting daemon %s" BTCES_EOL,
                      BTCES_DAEMON_NAME);
      exit(1);
    }
    else
    {
  BTCES_MSG_HIGH("Started %s daemon" BTCES_EOL,
                 BTCES_DAEMON_NAME);
    }
  }

  /* Register signal handlers */
  memset(&sig_act,
         0,
         sizeof(sig_act));

  sig_act.sa_handler = sig_hdlr;
  sigaction(SIGINT,
            &sig_act,
            NULL);
  sigaction(SIGTERM,
            &sig_act,
            NULL);

  /* Open BTC-ES (this should then come back to PFAL to set up worker thread) */
  if(0 != bt_coex_shim_open())
  {
    BTCES_MSG_ERROR("main(): bt_coex_shim_open() Failed" BTCES_EOL);
    exit(1);
  }

  /* Wait until told to exit */
  if(0 != pthread_join(g_btces_pfal_data.worker_thread_info.thread_handle,
                       NULL))
  {
    BTCES_MSG_ERROR("main(): error joining worker thread" BTCES_EOL);
    exit(1);
  }

  /* Clean up and exit gracefully */
  BTCES_MSG_HIGH("Exiting %s daemon" BTCES_EOL,
                 BTCES_DAEMON_NAME);

  exit(0);
}


/*----------------------------------------------------------------------------
 * Worker thread-related Function
 * Definitions
 *
 * The following functions are related
 * to the worker thread used to handle
 * the HCI and native events
 * -------------------------------------------------------------------------*/

void *btces_pfal_worker_thread
(
  void *unused
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_MSG_MEDIUM("worker_thread(): entered worker thread" BTCES_EOL);

  /* Init the worker thread */
  if(BTCES_SUCCESS != btces_pfal_init_worker_thread())
  {
    BTCES_MSG_ERROR("worker_thread(): error initializing" BTCES_EOL);

    goto worker_thread_exit;
  }

  /* Acquire dbus system*/
  if(BTCES_SUCCESS != btces_pfal_dbus_open())
  {
    BTCES_MSG_ERROR("worker_thread(): error opening bus" BTCES_EOL);

    goto worker_thread_exit;
  }

  /* Determine if BT is already on (and if so, start HCI processing) */
  if(TRUE == btces_pfal_dbus_get_default_adapter())
  {
    BTCES_MSG_MEDIUM("worker_thread(): turning on HCI!" BTCES_EOL);

    /* Determine if BT is already on (and if so, start HCI processing) */
    if(BTCES_SUCCESS != btces_pfal_hci_open())
    {
      BTCES_MSG_ERROR("worker_thread(): error setting up hci" BTCES_EOL);

      goto worker_thread_exit;
    }

    /* Notify the btces core about the native event */
    btces_svc_native_event_in(BTCES_NATIVE_EVENT_DEVICE_SWITCHED_ON,
                              NULL);
  }

  /* Set up dbus signals of interest */
  if(BTCES_SUCCESS != btces_pfal_dbus_enable_events())
  {
    BTCES_MSG_ERROR("worker_thread(): error setting up dbus signals" BTCES_EOL);

    goto worker_thread_exit;
  }

  /* Sanity check: the close_pipe_fd must always be valid! */
  BTCES_ASSERT(0 <= g_btces_pfal_data.worker_thread_info.close_pipe_fd[0]);
  BTCES_ASSERT(0 <= g_btces_pfal_data.worker_thread_info.close_pipe_fd[1]);

  /* Continuously process events as needed until told to exit */
  while(!g_btces_pfal_data.worker_thread_info.close_worker_thread)
  {
    int        ret_val = -1;

    /* Only read descriptors for pipes */
    int        nfds = BTCES_MAX(g_btces_pfal_data.worker_thread_info.dbus_fd,
                                BTCES_MAX(g_btces_pfal_data.worker_thread_info.hci_fd,
                                          BTCES_MAX(g_btces_pfal_data.worker_thread_info.close_pipe_fd[0],
                                                    g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0])));

    /* Reset to the enabled fd set each time */
    fd_set     read_set = g_btces_pfal_data.worker_thread_info.read_set;

    BTCES_MSG_MEDIUM("worker_thread(): main loop" BTCES_EOL);

    ret_val = select(nfds+1,
                     &read_set,
                     NULL,
                     NULL,
                     NULL );

    /* For now we do not care about the signals */
    if((-1 == ret_val) && (EINTR == errno))
    {
      BTCES_MSG_MEDIUM("worker_thread(): ret_val: %d; errno: %d" BTCES_EOL,
                       ret_val,
                       errno);
      continue;
    }

    if(0 > ret_val)
    {
      BTCES_MSG_ERROR("worker_thread(): error returned from select" BTCES_EOL);
      goto worker_thread_exit;
    }

    /* Process the close event first */
    if(FD_ISSET(g_btces_pfal_data.worker_thread_info.close_pipe_fd[0],
                &read_set))
    {
      BTCES_MSG_MEDIUM("worker_thread(): processing close event from select" BTCES_EOL);
      g_btces_pfal_data.worker_thread_info.close_worker_thread = TRUE;
      /* No need to read from this pipe - just bail out */
      continue;
    }

    /* Process the HCI event */
    if((0 <= g_btces_pfal_data.worker_thread_info.hci_fd) &&
       (FD_ISSET(g_btces_pfal_data.worker_thread_info.hci_fd,
                 &read_set)))
    {
      BTCES_MSG_MEDIUM("worker_thread(): processing HCI event from select" BTCES_EOL);
      (void) btces_pfal_hci_process_socket_event();
    }

    /* Process the watch event */
    if((0 <= g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0]) &&
       (FD_ISSET(g_btces_pfal_data.worker_thread_info.watch_pipe_fd[0],
                 &read_set)))
    {
      BTCES_MSG_MEDIUM("worker_thread(): processing watch event from select" BTCES_EOL);
      (void) btces_pfal_process_watch_event();
    }

    /* Process the dbus event */
    if((0 <= g_btces_pfal_data.worker_thread_info.dbus_fd) &&
       (FD_ISSET(g_btces_pfal_data.worker_thread_info.dbus_fd,
                 &read_set)))
    {
      BTCES_MSG_MEDIUM("worker_thread(): processing dbus event from select" BTCES_EOL);
      (void) btces_pfal_process_dbus_event();
    }

  }

worker_thread_exit:

  /* Release hci resources */
  (void) btces_pfal_hci_close();

  /* Release dbus resources */
  (void) btces_pfal_dbus_close();

  /* One last notification to BTCES */
  btces_svc_native_event_in(BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF,
                            NULL);

  BTCES_MSG_MEDIUM("worker_thread(): exiting worker thread" BTCES_EOL);

  /* Gracefully exit to let parent thread handle the join */
  pthread_exit((void *)unused);

  /* Keep the compiler happy */
  return (void *)unused;
}

