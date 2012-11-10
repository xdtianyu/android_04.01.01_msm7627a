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
  @file btces_api.cpp

  Bluetooth Coexistence Events Source

  This file implements the BTC-ES client (btces_*) and lower layer service
  (btces_svc_*) interfaces.
*/

/*=============================================================================

                       EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order. Please
  use ISO format for dates.

  when        who  what, where, why
  ----------  ---  -----------------------------------------------------------
  2010-04-26   pj  Fixed the incorrect queue behavior and added a flag for
                   Remote Name Request state.
  2010-04-05   pj  Modified the BTCES design to allow Inquiries to be queued
                   with ACL Connection setup and Remote Name Request.
  2010-03-03   pj  Initial Open Source version

=============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

#include "btces_plat.h"
#include "btces.h"
#include "btces_svc.h"
#include "btces_pfal.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/** Compare two Bluetooth addresses */
#define ADDR_IS_EQUAL( p1, p2 ) ( \
           ( std_memcmp( (p1), (p2), \
             sizeof( btces_bt_addr_struct ) ) == 0 ) ? TRUE : FALSE )

/** Maximum number of connections in the btces_state_data_struct:
    7 Bluetooth ACLs plus one active Remote Name Request procedure or one Inquiry procedure
*/
#define MAX_CONNS (8)

#define PAGE_TIMEOUT_DEFAULT (5120) /**< Default Page Timeout in msec, set up when the SoC is reset */

/** Extract a uint16 from an arbitrary location in an HCI buffer, Little Endian */
#define GET_HCI_UINT16(buff) ((uint16)((buff)[0]) | (((uint16)((buff)[1])) << 8))

/** Extract a Bluetooth address from an HCI stream (HCI Little Endian to Big Endian) */
#define GET_HCI_BT_ADDR(dest, src)  \
do                                  \
{                                   \
  (dest)[0] = (src)[5];             \
  (dest)[1] = (src)[4];             \
  (dest)[2] = (src)[3];             \
  (dest)[3] = (src)[2];             \
  (dest)[4] = (src)[1];             \
  (dest)[5] = (src)[0];             \
} while(0)

#define MAX_HEX_DUMP  16  /**< Max number of bytes output by btces_msg_w_hex() */

/** HCI Commands:

  HCI Commands as reported into btces_svc_hci_command_in(). The first two bytes
  are always the HCI_CMD_xxx opcode (OGF and OCF already are combined).

  Command parameters mentioned below in [brackets] are not of interest to BTC-ES.

  The HCI_CMD_xxx_LEN value is the parameter length required by BTC-ES for command
  xxx (based on the parameters of interest); the command's actual parameter
  length can be larger.

  Each HCI_CMD_xxx_yyy_OFST is the offset, from the start of the HCI command
  buffer, where field yyy begins. So every offset expression starts with 3+ to
  account for the command and parameter length bytes.

*/

/** Fetch the command's parameter length from a given command buffer */
#define GET_HCI_COMMAND_PARAM_LEN(buff) ((buff)[2])

/** HCI_Inquiry: [...] */
#define HCI_CMD_INQUIRY             (0x0401)

/** HCI_Inquiry_Cancel (no parameters) */
#define HCI_CMD_INQUIRY_CANCEL      (0x0402)

/** HCI_HCI_Periodic_Inquiry_Mode:
    Max_Period_Length(2), Min_Period_Length(2), LAP (3), Inquiry_Length(1), [...] */
#define HCI_CMD_PER_INQUIRY         (0x0403)
#define HCI_CMD_PER_INQUIRY_LEN              (2+2+3+1)
#define HCI_CMD_PER_INQUIRY_MIN_PER_OFST  (3+(2))
#define HCI_CMD_PER_INQUIRY_INQ_LEN_OFST  (3+(2+2+3))

/** HCI_HCI_Exit_Periodic_Inquiry_Mode (no parameters) */
#define HCI_CMD_EXIT_PER_INQUIRY    (0x0404)

/** HCI_Create_Connection: BT Addr, [...] */
#define HCI_CMD_CREATE_CONN         (0x0405)
#define HCI_CMD_CREATE_CONN_LEN                 (6)
#define HCI_CMD_CREATE_CONN_BT_ADDR_OFST     (3+(0))

/** HCI_Add_SCO_Connection: Connection Handle, [...] */
#define HCI_CMD_ADD_SCO_CONN        (0x0407)
#define HCI_CMD_ADD_SCO_CONN_LEN                (2)
#define HCI_CMD_ADD_SCO_CONN_HANDLE_OFST     (3+(0))

/** HCI_Remote_Name_Request: BT Addr, [...] */
#define HCI_CMD_REMOTE_NAME_REQ     (0x0419)
#define HCI_CMD_REMOTE_NAME_REQ_LEN              (6)
#define HCI_CMD_REMOTE_NAME_REQ_BT_ADDR_OFST  (3+(0))

/** HCI_Read_Page_Timeout (Only used for the associated Command Complete event) */
#define HCI_CMD_READ_PAGE_TIMEOUT   (0x0C17)

/** HCI_Write_Page_Timeout: Page_Timeout */
#define HCI_CMD_WRITE_PAGE_TIMEOUT  (0x0C18)
#define HCI_CMD_WRITE_PAGE_TIMEOUT_LEN           (2)
#define HCI_CMD_WRITE_PAGE_TIMEOUT_TIME_OFST  (3+(0))

/** HCI_Reset (no parameters) */
#define HCI_CMD_RESET               (0x0C03)

/** HCI_Setup_Synchronous_Connection: Connection_Handle, [...] */
#define HCI_CMD_SETUP_SYNC_CONN     (0x0428)
#define HCI_CMD_SETUP_SYNC_CONN_LEN              (2)
#define HCI_CMD_SETUP_SYNC_CONN_HANDLE_OFST   (3+(0))


/* HCI Events:

  HCI Events as reported into btces_svc_hci_event_in(). The first byte is always
  the event opcode, followed by the length byte and the event's parameters.

  Event parameters mentioned below in [brackets] are not of interest to BTC-ES.

  The HCI_EVENT_xxx_LEN value is the parameter length required by BTC-ES for event
  xxx (based on the parameters of interest); the event's actual parameter length
  can be larger.

  Each HCI_EVENT_xxx_yyy_OFST is the offset, from the start of the HCI event
  buffer, where field yyy begins. So every offset expression starts with 2+ to
  account for the event and parameter length bytes.
*/

/** Fetch the event opcode from a given event buffer */
#define GET_HCI_EVENT_OPCODE(buff)    ((buff)[0])

/** Fetch the event's parameter length from a given event buffer */
#define GET_HCI_EVENT_PARAM_LEN(buff) ((buff)[1])

/** HCI Event Status code for 'Success' */
#define HCI_EVENT_STATUS_SUCCESS  (0x00)

/** Inquiry Complete: [...] */
#define HCI_EVENT_INQUIRY_COMP          (0x01)

/** Connection Complete: Status, Connection Handle, BT Addr, Link Type, [...] */
#define HCI_EVENT_CONNECT_COMP          (0x03)
#define HCI_EVENT_CONNECT_COMP_LEN               (1+2+6+1)
#define HCI_EVENT_CONNECT_COMP_STATUS_OFST    (2+(0))
#define HCI_EVENT_CONNECT_COMP_HANDLE_OFST    (2+(1))
#define HCI_EVENT_CONNECT_COMP_BT_ADDR_OFST   (2+(1+2))
#define HCI_EVENT_CONNECT_COMP_LINK_TYPE_OFST (2+(1+2+6))

/** Connection Request: BT Addr, [Class of Device], Link Type */
#define HCI_EVENT_CONNECT_REQ           (0x04)
#define HCI_EVENT_CONNECT_REQ_LEN                (6+3+1)
#define HCI_EVENT_CONNECT_REQ_BT_ADDR_OFST    (2+(0))
#define HCI_EVENT_CONNECT_REQ_LINK_TYPE_OFST  (2+(6+3))

/** Disconnection Complete: [Status], Connection Handle, [...] */
#define HCI_EVENT_DISCONNECT_COMP       (0x05)
#define HCI_EVENT_DISCONNECT_COMP_LEN            (1+2)
#define HCI_EVENT_DISCONNECT_COMP_HANDLE_OFST (2+(1))

/** Remote Name Request Complete: [Status], BT Addr, [...] */
#define HCI_EVENT_REMOTE_NAME_REQ_COMP      (0x07)
#define HCI_EVENT_REMOTE_NAME_REQ_COMP_LEN  (1+6)
#define HCI_EVENT_REMOTE_NAME_REQ_COMP_BT_ADDR_OFST (2+(1))

/** Command Complete: [Num_HCI_Command_Packets], Command_Opcode, Return_Parameters;
    Used to see the results of HCI_Read_Page_Timeout Opcode (Status, Page Timeout)
*/
#define HCI_EVENT_COMMAND_COMP          (0x0E)
#define HCI_EVENT_COMMAND_COMP_LEN                       (1+2+(1+2))
#define HCI_EVENT_COMMAND_COMP_CMD_OFST               (2+(1))
#define HCI_EVENT_COMMAND_COMP_READ_PAGE_STATUS_OFST  (2+(1+2+(0)))
#define HCI_EVENT_COMMAND_COMP_READ_PAGE_TIMEOUT_OFST (2+(1+2+(1)))

/** Role Change: Status, BT Addr, [...] */
#define HCI_EVENT_ROLE_CHANGE           (0x12)
#define HCI_EVENT_ROLE_CHANGE_LEN              (1+6)
#define HCI_EVENT_ROLE_CHANGE_STATUS_OFST   (2+(0))
#define HCI_EVENT_ROLE_CHANGE_BT_ADDR_OFST  (2+(1))

/** Mode Change: Status, Connection_Handle, Current_Mode, [...] */
#define HCI_EVENT_MODE_CHANGE           (0x14)
#define HCI_EVENT_MODE_CHANGE_LEN            (1+2+1)
#define HCI_EVENT_MODE_CHANGE_STATUS_OFST (2+(0))
#define HCI_EVENT_MODE_CHANGE_HANDLE_OFST (2+(1))
#define HCI_EVENT_MODE_CHANGE_MODE_OFST   (2+(1+2))

/** PIN Code Request: BT Addr */
#define HCI_EVENT_PIN_CODE_REQ          (0x16)
#define HCI_EVENT_PIN_CODE_REQ_LEN             (6)
#define HCI_EVENT_PIN_CODE_REQ_BT_ADDR_OFST (2+(0))

/** Link Key Request: BT Addr */
#define HCI_EVENT_LINK_KEY_REQ          (0x17)
#define HCI_EVENT_LINK_KEY_REQ_LEN             (6)
#define HCI_EVENT_LINK_KEY_REQ_BT_ADDR_OFST (2+(0))

/** Synchronous Connection Complete: Status, Connection_Handle, BT Addr, Link type,
                                      Transmission_Interval, Retransmission Window, [...] */
#define HCI_EVENT_SYNC_CONNECT_COMP     (0x2C)
#define HCI_EVENT_SYNC_CONNECT_COMP_LEN                (1+2+6+1+1+1)
#define HCI_EVENT_SYNC_CONNECT_COMP_STATUS_OFST     (2+(0))
#define HCI_EVENT_SYNC_CONNECT_COMP_HANDLE_OFST     (2+(1))
#define HCI_EVENT_SYNC_CONNECT_COMP_BT_ADDR_OFST    (2+(1+2))
#define HCI_EVENT_SYNC_CONNECT_COMP_LINK_TYPE_OFST  (2+(1+2+6))
#define HCI_EVENT_SYNC_CONNECT_COMP_TX_INT_OFST     (2+(1+2+6+1))
#define HCI_EVENT_SYNC_CONNECT_COMP_RETX_WIN_OFST   (2+(1+2+6+1+1))

/** Synchronous Connection Changed: Status, Connection_Handle, Transmission_Interval,
                                      Retransmission Window, [...] */
#define HCI_EVENT_SYNC_CONNECT_CHANGED      (0x2D)
#define HCI_EVENT_SYNC_CONNECT_CHANGED_LEN               (1+2+1+1)
#define HCI_EVENT_SYNC_CONNECT_CHANGED_STATUS_OFST    (2+(0))
#define HCI_EVENT_SYNC_CONNECT_CHANGED_HANDLE_OFST    (2+(1))
#define HCI_EVENT_SYNC_CONNECT_CHANGED_TX_INT_OFST    (2+(1+2))
#define HCI_EVENT_SYNC_CONNECT_CHANGED_RETX_WIN_OFST  (2+(1+2+1))

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/** Connection State: The possible states of a connection in the State Data */
typedef enum
{
  CONN_STATE_INVALID = 0,         /**< Connection table entry is invalid, can be due to a failed connection attempt */
  CONN_STATE_INQUIRY,             /**< Inquiry procedure */
  CONN_STATE_REMOTE_NAME_REQUEST, /**< No ACL connection exists, used during Remote Name Request */
  CONN_STATE_SETUP_INCOMING,      /**< Incoming create connection request in progress */
  CONN_STATE_SETUP_OUTGOING,      /**< Outgoing create connection request in progress */
  CONN_STATE_CONNECTED,           /**< ACL connection established */
  CONN_STATE_STREAMING,           /**< A2DP streaming active over this ACL connection */
  CONN_STATE_MAX                  /**< This value and higher are invalid */
} btces_conn_state_enum;

/** SCO State: The possible states of a synchronous connection (added to an existing ACL connection) in the State Data */
typedef enum
{
  SCO_STATE_INVALID = 0,  /**< No synchronous connection exists */
  SCO_STATE_SETUP,        /**< Synchronous connection is being set up */
  SCO_STATE_SCO,          /**< Synchronous Connection exists, SCO type */
  SCO_STATE_ESCO,         /**< Synchronous Connection exists, eSCO type */
  SCO_STATE_MAX           /**< This value and higher are invalid */
} btces_sco_state_enum;

/** Connection Structure: This structure is used to describe an ACL connection */
typedef struct
{
  btces_conn_state_enum   conn_state;   /**< Connection state */
  btces_sco_state_enum    sco_state;    /**< SCO State */
  uint16                  acl_handle;   /**< ACL Connection handle */
  uint16                  sco_handle;   /**< SCO Handle (a connection handle) */
  btces_bt_addr_struct    addr;         /**< Remote device address */
  uint8                   acl_mode;     /**< ACL mode */
  uint8                   sco_interval; /**< SCO Instance, or Tsco, in number of slots */
  uint8                   sco_window;   /**< SCO Window, in number of slots */
  uint8                   retrans_win;  /**< eSCO retransmission window, in number of slots */
  uint8                   qpos;         /**< Queue position state */
} btces_conn_data_struct;

/** BTC-ES State Data: While BTC-ES is running, an instance of this structure
    describes its current state.
*/
typedef struct
{
  btces_cb_type *report_cb_ptr;     /**< Registered callback for event reports */
  void          *user_data;         /**< Opaque data associated with callback */
  boolean       bluetooth_is_on;    /**< Stack "power" state; FALSE = Off */
  boolean       connecting_now;     /**< TRUE: Connection procedure in progress */
  boolean       requesting_now;     /**< TRUE: Remote name request in progress */
  boolean       inquiry_is_active;  /**< TRUE: Inquiry procedure in progress */
  boolean       in_per_inq_mode;    /**< TRUE: In Periodic Inquiry Mode */
  boolean       paging_now;         /**< TRUE: Paging procedure in progress */
  uint32        page_timer_tag;     /**< Unique number for a page timer instance */
  uint32        per_inq_timer_tag;  /**< Unique number for an inquiry timer instance */
  void          *page_timer_id;     /**< Platform-defined page timer identifier */
  void          *per_inq_timer_id;  /**< Platform-defined inquiry timer identifier */
  uint16        page_timeout;       /**< The duration of a page procedure */
  uint16        per_inq_timeout;    /**< Time until the next periodic inquiry */
  btces_conn_data_struct *( conn_ptr_table[MAX_CONNS] );  /**< Array of connection structure pointers */
} btces_state_data_struct;

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/** BTC-ES global State Data pointer.

    If the pointer is NULL, then BTC-ES is not initialized. Otherwise it holds a
    pointer to an allocated structure containing BTC-ES State Data.
*/
static btces_state_data_struct  *btces_g_state_data_ptr = NULL;

/** BTC-ES global static WLAN Channels in use

    A bit field representing the WLAN channels in use ('1' = in use).
    It is static to retain the latest setting in case BTC-ES is not running.
*/
static uint16 btces_g_wlan_chan = 0x0000;

/**
    Defined a dummy Bluetooth address to create a connection table for HCI Inquiry.
*/
static const uint8 bt_addr_array_dummy[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#ifdef BTCES_DEBUG
/* Array of hex digits for debug output */
static const unsigned char digits[] = "0123456789ABCDEF";
#endif

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*==============================================================
FUNCTION:  btces_create_inq_entry()
==============================================================*/

/** Create a connection table for Inquiry and queue it or start it. */

static void btces_create_inq_entry( void );

#ifdef BTCES_DEBUG
/*==============================================================
FUNCTION:  btces_msg_w_hex()
==============================================================*/

/** Output a hex dump debug message. */

static void btces_msg_w_hex
(
  unsigned int num_bytes,      /**< [in]: Number of bytes to output as hex */
  uint8 *hex_buf        /**< [in]: Bytes to output */
)
{
  unsigned char buffer[MAX_HEX_DUMP * (2+1)]; /* 2 chars per hex byte plus a space */
  unsigned char *buf = buffer;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( num_bytes > 0 );
  BTCES_ASSERT( hex_buf != NULL );

  if ( num_bytes > MAX_HEX_DUMP )
  {
    num_bytes = MAX_HEX_DUMP;   /* Limit the output */
  }

  do
  {
    *buf++ = digits[ *hex_buf >> 4 ];
    *buf++ = digits[ *hex_buf++ & 0x0F ];
    *buf++ = ' ';
  } while ( --num_bytes );

  *(--buf) = '\0';    /* Change last ending space to NUL */

  BTCES_MSG_LOW( "%s" BTCES_EOL, buffer );
}
#endif

/*==============================================================
FUNCTION:  btces_byte_to_mode()
==============================================================*/

/** Change the incoming byte value to the corresponding connection mode.

  @return BTCES_MODE_TYPE_ACTIVE, _HOLD, _SNIFF, _PARK: Byte was a valid mode.
  @return BTCES_MODE_TYPE_MAX: Byte was not a valid mode.
*/

static uint8 btces_byte_to_mode
(
  uint8 byte_mode   /**< The mode value as a byte */
)
{
  /* The byte values are taken from the Bluetooth Spec */
  switch ( byte_mode )
  {
    case 0: return ( BTCES_MODE_TYPE_ACTIVE );
    case 1: return ( BTCES_MODE_TYPE_HOLD );
    case 2: return ( BTCES_MODE_TYPE_SNIFF );
    case 3: return ( BTCES_MODE_TYPE_PARK );
    default: return ( BTCES_MODE_TYPE_MAX );
  }
}

/*==============================================================
FUNCTION:  btces_byte_to_link()
==============================================================*/

/** Change the incoming byte value to the corresponding connection link type.

  @return BTCES_LINK_TYPE_SCO, _ACL, _ESCO: Byte was a valid link type.
  @return BTCES_LINK_TYPE_MAX: Byte was not a valid link type.
*/

static uint8 btces_byte_to_link
(
  uint8 byte_link   /**< The link type value as a byte */
)
{
  /* The byte values are taken from the Bluetooth Spec */
  switch ( byte_link )
  {
    case 0: return ( BTCES_LINK_TYPE_SCO );
    case 1: return ( BTCES_LINK_TYPE_ACL );
    case 2: return ( BTCES_LINK_TYPE_ESCO );
    default: return ( BTCES_LINK_TYPE_MAX );
  }
}

/*==============================================================
FUNCTION:  btces_test_init()
==============================================================*/

/** Check if BTCES has been initialized and grab the token if so.

  Any return value besides BTCES_OK means BTCES is not running.

  Example:

    ret_val = btces_test_init();
    if ( ret_val == BTCES_OK )
    {
      ... BTC-ES is initialized and the exclusion token is captured, so it is safe
          to use btces_g_state_data_ptr and do critical section processing ...

      btces_pfal_release_token();
    }
    else
    {
      ... BTC-ES APIs are not ready to use; ret_val should be BTCES_STATUS_NOT_INITIALIZED .
    }

  @return BTCES_OK: BTCES is running, the token was acquired, and the caller must
                    remember to free it when processing is complete.
  @return BTCES_STATUS_NOT_INITIALIZED: btces_init() has not been called yet.
*/

static BTCES_STATUS btces_test_init( void )
{
  BTCES_STATUS ret_val = BTCES_STATUS_NOT_INITIALIZED;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* If it looks like there is an instance of State Data, grab the token */
  if ( btces_g_state_data_ptr != NULL )
  {
    ret_val = btces_pfal_get_token();

    if ( ret_val == BTCES_OK )
    {
      /* Check to ensure that BTC-ES was not being brought down while we were
         waiting on the token. This check may simplify the platform
         implementation of btces_pfal_get_token().
      */
      if ( btces_g_state_data_ptr != NULL )
      {
        ret_val = BTCES_OK;
      }
      else
      {
        BTCES_MSG_LOW( "BTC-ES: Shut down happened while waiting for token." BTCES_EOL );
        ret_val = BTCES_STATUS_NOT_INITIALIZED;
      }
    }
  }

  return( ret_val );
}

/*==============================================================
FUNCTION:  btces_report_bt_power()
==============================================================*/

/** Report the BT power state as a new event. */

static void btces_report_bt_power( void )
{
  btces_event_enum  event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    if ( btces_g_state_data_ptr->bluetooth_is_on )
    {
      BTCES_MSG_LOW( "BTC-ES: Reporting BT On Event" BTCES_EOL );
      event = BTCES_EVENT_DEVICE_SWITCHED_ON;
    }
    else
    {
      BTCES_MSG_LOW( "BTC-ES: Reporting BT Off Event" BTCES_EOL );
      event = BTCES_EVENT_DEVICE_SWITCHED_OFF;
    }

    btces_g_state_data_ptr->report_cb_ptr( event, NULL,
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping BT On/Off Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_test_bt_on()
==============================================================*/

/** The caller thinks Bluetooth is "On". If it isn't in the State Data, do some
    initialization and report the BTCES_EVENT_DEVICE_SWITCHED_ON event.
*/

static void btces_test_bt_on( void )
{
  uint8 i;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( !( btces_g_state_data_ptr->bluetooth_is_on ) )
  {
    /* Do limited initialization of State Data. All connection table entries should
       be empty. Leave the callback registration, timer IDs and timer tags alone.
     */
    btces_g_state_data_ptr->connecting_now = FALSE;
    btces_g_state_data_ptr->requesting_now = FALSE;
    btces_g_state_data_ptr->inquiry_is_active = FALSE;
    btces_g_state_data_ptr->in_per_inq_mode = FALSE;
    btces_g_state_data_ptr->paging_now = FALSE;
    btces_g_state_data_ptr->page_timeout = PAGE_TIMEOUT_DEFAULT;

    for ( i = 0; i < MAX_CONNS; i++ )
    {
      if ( btces_g_state_data_ptr->conn_ptr_table[i] != NULL )
      {
        btces_pfal_free( btces_g_state_data_ptr->conn_ptr_table[i] );
        btces_g_state_data_ptr->conn_ptr_table[i] = NULL;
      }
    }

    /* Inform the platform if there are any WLAN channels in use */
    if ( btces_g_wlan_chan != 0x0000 )
    {
      /* Return value doesn't affect the caller */
      btces_pfal_wlan_chan( btces_g_wlan_chan );
    }

    /* Set that BT is "On" and report the event */
    btces_g_state_data_ptr->bluetooth_is_on = TRUE;
    btces_report_bt_power();
  }
}

/*==============================================================
FUNCTION:  btces_report_inquiry()
==============================================================*/

/** Report the state of Inquiry activity as a new event. */

static void btces_report_inquiry( void )
{
  btces_event_enum  event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    if ( btces_g_state_data_ptr->inquiry_is_active )
    {
      BTCES_MSG_LOW( "BTC-ES: Reporting Inquiry Start Event" BTCES_EOL );
      event = BTCES_EVENT_INQUIRY_STARTED;
    }
    else
    {
      BTCES_MSG_LOW( "BTC-ES: Reporting Inquiry Stop Event" BTCES_EOL );
      event = BTCES_EVENT_INQUIRY_STOPPED;
    }

    btces_g_state_data_ptr->report_cb_ptr( event, NULL,
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping Inquiry Start/Stop Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_paging()
==============================================================*/

/** Report the state of Paging activity as a new event. */

static void btces_report_paging( void )
{
  btces_event_enum  event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    if ( btces_g_state_data_ptr->paging_now )
    {
      BTCES_MSG_LOW( "BTC-ES: Reporting Page Start Event" BTCES_EOL );
      event = BTCES_EVENT_PAGE_STARTED;
    }
    else
    {
      BTCES_MSG_LOW( "BTC-ES: Reporting Page Stop Event" BTCES_EOL );
      event = BTCES_EVENT_PAGE_STOPPED;
    }

    btces_g_state_data_ptr->report_cb_ptr( event, NULL,
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping Paging Start/Stop Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_acl_create()
==============================================================*/

/** Report a BTCES_EVENT_CREATE_ACL_CONNECTION event. */

static void btces_report_acl_create
(
  btces_conn_data_struct *conn_ptr /**< [in]: Pointer to connection table entry */
)
{
  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    BTCES_MSG_LOW( "BTC-ES: Reporting ACL Create Event" BTCES_EOL );
    btces_g_state_data_ptr->report_cb_ptr( BTCES_EVENT_CREATE_ACL_CONNECTION,
                                           (btces_event_data_union *)( &( conn_ptr->addr ) ),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping ACL Create Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_acl_complete()
==============================================================*/

/** Report a BTCES_EVENT_ACL_CONNECTION_COMPLETE event.

    This may be called when the connection is successful or failed. A failed
    connection is indicated by the CONN_STATE_INVALID connection state.
*/

static void btces_report_acl_complete
(
  btces_conn_data_struct *conn_ptr /**< [in]: Pointer to connection table entry */
)
{
  btces_event_data_acl_comp_struct  acl_comp_event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    acl_comp_event.addr = conn_ptr->addr;

    if ( conn_ptr->conn_state == CONN_STATE_INVALID )
    {
      acl_comp_event.conn_handle = BTCES_INVALID_CONN_HANDLE;
      acl_comp_event.conn_status = BTCES_CONN_STATUS_FAIL;

      BTCES_MSG_LOW( "BTC-ES: Reporting ACL Create Complete Event (fail)" BTCES_EOL );
    }
    else /* Connection state must be CONN_STATE_CONNECTED or CONN_STATE_STREAMING */
    {
      BTCES_ASSERT( ( conn_ptr->conn_state == CONN_STATE_CONNECTED ) ||
                    ( conn_ptr->conn_state == CONN_STATE_STREAMING ) );

      acl_comp_event.conn_handle = conn_ptr->acl_handle;
      acl_comp_event.conn_status = BTCES_CONN_STATUS_SUCCESS;

      BTCES_MSG_LOW( "BTC-ES: Reporting ACL Create Complete Event (success)" BTCES_EOL );
    }

    btces_g_state_data_ptr->report_cb_ptr( BTCES_EVENT_ACL_CONNECTION_COMPLETE,
                                           (btces_event_data_union *)( &( acl_comp_event )),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping ACL Create Complete Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_mode_chg()
==============================================================*/

/** Report a BTCES_EVENT_MODE_CHANGED event. */

static void btces_report_mode_chg
(
  btces_conn_data_struct *conn_ptr /**< [in]: Pointer to connection table entry */
)
{
  btces_event_data_mode_struct  mode_event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    mode_event.conn_handle = conn_ptr->acl_handle;
    mode_event.mode = conn_ptr->acl_mode;

    BTCES_MSG_LOW( "BTC-ES: Reporting Mode Changed Event" BTCES_EOL );
    btces_g_state_data_ptr->report_cb_ptr( BTCES_EVENT_MODE_CHANGED,
                                           (btces_event_data_union *)( &( mode_event ) ),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping Mode Change Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_a2dp_chg()
==============================================================*/

/** Report a BTCES_EVENT_A2DP_STREAM_START or _STOP event. */

static void btces_report_a2dp_chg
(
  btces_conn_data_struct *conn_ptr /**< [in]: Pointer to connection table entry */
)
{
  btces_event_enum  event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    if ( conn_ptr->conn_state == CONN_STATE_CONNECTED )
    {
      BTCES_MSG_LOW( "BTC-ES: Reporting A2DP Streaming Stop Event" BTCES_EOL );
      event = BTCES_EVENT_A2DP_STREAM_STOP;
    }
    else /* Must be CONN_STATE_STREAMING */
    {
      BTCES_ASSERT( conn_ptr->conn_state == CONN_STATE_STREAMING );
      BTCES_MSG_LOW( "BTC-ES: Reporting A2DP Streaming Start Event" BTCES_EOL );
      event = BTCES_EVENT_A2DP_STREAM_START;
    }

    btces_g_state_data_ptr->report_cb_ptr( event,
                                           (btces_event_data_union *)( &(conn_ptr->addr ) ),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping A2DP Streaming Start/Stop Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_sync_create()
==============================================================*/

/** Report a BTCES_EVENT_CREATE_SYNC_CONNECTION event. */

static void btces_report_sync_create
(
  btces_conn_data_struct *conn_ptr /**< [in]: Pointer to connection table entry */
)
{
  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    BTCES_MSG_LOW( "BTC-ES: Reporting Sync Create Event" BTCES_EOL );
    btces_g_state_data_ptr->report_cb_ptr( BTCES_EVENT_CREATE_SYNC_CONNECTION,
                                           (btces_event_data_union *)( &( conn_ptr->addr ) ),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping Sync Create Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_sync_complete()
==============================================================*/

/** Report a BTCES_EVENT_SYNC_CONNECTION_COMPLETE event.

    This may be called when the sync connection is successful or failed. A failed
    connection is indicated by the SCO_STATE_INVALID sync connection state.
*/

static void btces_report_sync_complete
(
  btces_conn_data_struct *conn_ptr /**< [in]: Pointer to connection table entry */
)
{
  btces_event_data_sync_comp_up_struct  sync_comp_up_event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    sync_comp_up_event.addr = conn_ptr->addr;

    if ( conn_ptr->sco_state == SCO_STATE_INVALID )
    {
      sync_comp_up_event.conn_handle = BTCES_INVALID_CONN_HANDLE;
      sync_comp_up_event.conn_status = BTCES_CONN_STATUS_FAIL;
      sync_comp_up_event.link_type = BTCES_LINK_TYPE_MAX; /* Invalid */
      sync_comp_up_event.sco_interval = 0;
      sync_comp_up_event.sco_window = 0;
      sync_comp_up_event.retrans_win = 0;

      BTCES_MSG_LOW( "BTC-ES: Reporting Sync Create Complete Event (fail)" BTCES_EOL );
    }
    else /* Connection state must be SCO_STATE_SCO or SCO_STATE_ESCO */
    {
      BTCES_ASSERT( ( conn_ptr->sco_state == SCO_STATE_SCO ) ||
                    ( conn_ptr->sco_state == SCO_STATE_ESCO ) );

      sync_comp_up_event.conn_handle = conn_ptr->sco_handle;
      sync_comp_up_event.conn_status = BTCES_CONN_STATUS_SUCCESS;
      if ( conn_ptr->sco_state == SCO_STATE_SCO )
      {
        sync_comp_up_event.link_type = BTCES_LINK_TYPE_SCO;
      }
      else
      {
        sync_comp_up_event.link_type = BTCES_LINK_TYPE_ESCO;
      }
      sync_comp_up_event.sco_interval = conn_ptr->sco_interval;
      sync_comp_up_event.sco_window = conn_ptr->sco_window;
      sync_comp_up_event.retrans_win = conn_ptr->retrans_win;

      BTCES_MSG_LOW( "BTC-ES: Reporting Sync Create Complete Event (success)" BTCES_EOL );
    }

    btces_g_state_data_ptr->report_cb_ptr( BTCES_EVENT_SYNC_CONNECTION_COMPLETE,
                                           (btces_event_data_union *)( &( sync_comp_up_event ) ),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping Sync Create Complete Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_sync_change()
==============================================================*/

/** Report a BT_EVENT_SYNC_CONNECTION_UPDATED event. */

static void btces_report_sync_change
(
  btces_conn_data_struct *conn_ptr /**< [in]: Pointer to connection table entry */
)
{
  btces_event_data_sync_comp_up_struct  sync_comp_up_event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    sync_comp_up_event.addr = conn_ptr->addr;
    sync_comp_up_event.conn_handle = conn_ptr->sco_handle;
    sync_comp_up_event.conn_status = BTCES_CONN_STATUS_SUCCESS;
    if ( conn_ptr->sco_state == SCO_STATE_SCO )
    {
      sync_comp_up_event.link_type = BTCES_LINK_TYPE_SCO;
    }
    else
    {
      BTCES_ASSERT( conn_ptr->sco_state == SCO_STATE_ESCO );
      sync_comp_up_event.link_type = BTCES_LINK_TYPE_ESCO;
    }
    sync_comp_up_event.sco_interval = conn_ptr->sco_interval;
    sync_comp_up_event.sco_window = conn_ptr->sco_window;
    sync_comp_up_event.retrans_win = conn_ptr->retrans_win;

    BTCES_MSG_LOW( "BTC-ES: Reporting Sync Connection Updated Event" BTCES_EOL );
    btces_g_state_data_ptr->report_cb_ptr( BTCES_EVENT_SYNC_CONNECTION_UPDATED,
                                           (btces_event_data_union *)( &( sync_comp_up_event ) ),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping Sync Connection Updated Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_report_disconnect()
==============================================================*/

/** Report a BTCES_EVENT_DISCONNECTION_COMPLETE event.

    This may be called for a sync or ACL type connection.
*/

static void btces_report_disconnect
(
  uint16 conn_handle   /**< [in]: Connection Handle (for ACL or Sync connection) */
)
{
  btces_event_data_disc_comp_struct  disc_event;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
  {
    disc_event.conn_handle = conn_handle;

    BTCES_MSG_LOW( "BTC-ES: Reporting Disconnect Event" BTCES_EOL );
    btces_g_state_data_ptr->report_cb_ptr( BTCES_EVENT_DISCONNECTION_COMPLETE,
                                           (btces_event_data_union *)( &( disc_event ) ),
                                           btces_g_state_data_ptr->user_data );
  }
  else
  {
    BTCES_MSG_LOW( "BTC-ES: Skipping Disconnect Event report" BTCES_EOL );
  }
}

/*==============================================================
FUNCTION:  btces_make_state_report()
==============================================================*/

/** Output a series of events representing the current state of Bluetooth. */

static void btces_make_state_report( void )
{
  uint8 i;
  btces_conn_data_struct *conn_ptr;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  btces_report_bt_power();    /* Always report the current power state */

  /* If Bluetooth is on, there may be more events to report */
  if ( btces_g_state_data_ptr->bluetooth_is_on )
  {
    if ( btces_g_state_data_ptr->inquiry_is_active )
    {
      btces_report_inquiry();
    }

    if ( btces_g_state_data_ptr->paging_now )
    {
      btces_report_paging();
    }

    /* Now report events about each connection */
    for ( i = 0; i < MAX_CONNS ; i++ )
    {
      conn_ptr = btces_g_state_data_ptr->conn_ptr_table[i];

      /* Skip entries that are in the queue and report an active entry only. */
      if ( ( conn_ptr != NULL ) &&
           ( conn_ptr->qpos == 0 ) )
      {
        switch ( conn_ptr->conn_state )
        {
          case CONN_STATE_SETUP_INCOMING:
          case CONN_STATE_SETUP_OUTGOING:
          {
            btces_report_acl_create( conn_ptr );
            break;
          }

          case CONN_STATE_CONNECTED:
          case CONN_STATE_STREAMING:
          {
            btces_report_acl_complete( conn_ptr );

            /* BTCES_MODE_TYPE_ACTIVE is the default state for an ACL connection */
            if ( conn_ptr->acl_mode != BTCES_MODE_TYPE_ACTIVE )
            {
              btces_report_mode_chg( conn_ptr );
            }

            if ( conn_ptr->conn_state == CONN_STATE_STREAMING )
            {
              btces_report_a2dp_chg( conn_ptr );
            }

            switch ( conn_ptr->sco_state )
            {
              case SCO_STATE_SETUP:
              {
                btces_report_sync_create( conn_ptr );
                break;
              }

              case SCO_STATE_SCO:
              case SCO_STATE_ESCO:
              {
                btces_report_sync_complete( conn_ptr );
                break;
              }

              /* Ignore other possible SCO states */
              default:
              {
                break;
              }

            } /* switch ( conn_ptr->sco_state ) */

            break;
          }

          /* Ignore other possible connection states */
          default:
          {
            break;
          }
        } /* switch ( conn_ptr->conn_state )*/
      } /* ( conn_ptr != NULL ) && ( conn_ptr->qpos == 0 ) */
    } /* for ( i = 0; i < MAX_CONNS ; i++ ) */
  } /* !( btces_g_state_data_ptr->bluetooth_is_on ) */
}

/*==============================================================
FUNCTION:  btces_find_next_qpos()
==============================================================*/

/** Find the queue position to push the connection table entry.

  Find the largest queue position of all table entries and add one to it.
  If no table entry is found in the queue, return the queue position with
  the value 1 so as to be the first to be pulled off.

  @return  New queue position to push the connection table entry.
*/

static uint8 btces_find_next_qpos( void )
{
  uint8 i;
  uint8 max = 0;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  for ( i = 0; i < MAX_CONNS; i++ )
  {
    /* Find the largest queue position of all table entries */
    if ( (btces_g_state_data_ptr->conn_ptr_table[i] != NULL) &&
         (btces_g_state_data_ptr->conn_ptr_table[i]->qpos > max) )
    {
      max = btces_g_state_data_ptr->conn_ptr_table[i]->qpos;
    }
  }
  /* Return a new queue position to push the connection table entry. */
  return ( max + 1 );
}

/*==============================================================
FUNCTION:  btces_dequeue_conn_entry()
==============================================================*/

/** Find the connection table entry index to pull from the queue.

  Decrement the queue position of all table entries and
  pull off the table entry if decremented queue position is zero.

  @return  Connection table entry index pulled from the queue.
           -1 if the queue is empty.
*/

static int8 btces_dequeue_conn_entry( void )
{
  uint8 i;
  int8 index = -1;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  for ( i = 0; i < MAX_CONNS; i++ )
  {
    /* Decrement the queue position of all table entries */
    if ( (btces_g_state_data_ptr->conn_ptr_table[i] != NULL) &&
         (btces_g_state_data_ptr->conn_ptr_table[i]->qpos > 0) )
    {
      /* Store the index to the table entry if decremented queue position is zero. */
      if ( --(btces_g_state_data_ptr->conn_ptr_table[i]->qpos) == 0)
      {
        BTCES_ASSERT( index == -1 );
        index = i;
      }
    }
  }
  /* Return the index of next table entry in the queue, or -1 if the queue is empty. */
  return ( index );
}

/*==============================================================
FUNCTION:  btces_remove_conn_entry_from_queue()
==============================================================*/

/** Remove the given connection table entry from the queue.

  Decrement the queue position of the table entries whose queue
  position is larger than the queue position of the given entry.
*/

static void btces_remove_conn_entry_from_queue
(
  uint8 entry_index
)
{
  uint8 i;
  uint8 entry_qpos;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );
  BTCES_ASSERT( btces_g_state_data_ptr->conn_ptr_table[entry_index] != NULL );

  entry_qpos = btces_g_state_data_ptr->conn_ptr_table[entry_index]->qpos;

  BTCES_ASSERT( entry_qpos != 0 );

  /* Set the entry queue position to zero for removal.*/
  btces_g_state_data_ptr->conn_ptr_table[entry_index]->qpos = 0;

  for ( i = 0; i < MAX_CONNS; i++ )
  {
    if ( btces_g_state_data_ptr->conn_ptr_table[i] != NULL )
    {
      /* Decrement the queue position if larger than the given entry queue position.*/
      if ( btces_g_state_data_ptr->conn_ptr_table[i]->qpos > entry_qpos )
      {
        --(btces_g_state_data_ptr->conn_ptr_table[i]->qpos);
      }
    }
  }
}

/*==============================================================
FUNCTION:  btces_close_open_events()
==============================================================*/

/** Issue a series of events as needed to "close out" any ongoing activity.

  These actions are done when the HCI_Reset command is received or if Bluetooth
  is turned off.
*/

static void btces_close_open_events( void )
{
  uint8 i;
  btces_conn_data_struct *conn_ptr;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  /* If inquiry was in progress, stop it and report that event */
  if ( btces_g_state_data_ptr->inquiry_is_active )
  {
    btces_g_state_data_ptr->inquiry_is_active = FALSE;
    btces_report_inquiry();
  }
  else if ( btces_g_state_data_ptr->in_per_inq_mode )
  {
    /* We were waiting for the next periodic inquiry to start; stop the timer */
    btces_pfal_stop_timer( btces_g_state_data_ptr->per_inq_timer_id );
  }

  /* Not in periodic inquiry mode any more */
  btces_g_state_data_ptr->in_per_inq_mode = FALSE;

  /* If paging was in progress, stop the timer and report that event */
  if ( btces_g_state_data_ptr->paging_now )
  {
    btces_g_state_data_ptr->paging_now = FALSE;
    btces_pfal_stop_timer( btces_g_state_data_ptr->page_timer_id );
    btces_report_paging();
  }

  /* "Expire" possibly running timers; 0 does not need to be avoided */
  btces_g_state_data_ptr->page_timer_tag++;
  btces_g_state_data_ptr->per_inq_timer_tag++;

  /* Now check for active connections and close them */
  for ( i = 0; i < MAX_CONNS; i++)
  {
    conn_ptr = btces_g_state_data_ptr->conn_ptr_table[i];

    if ( conn_ptr != NULL )
    {
      /* If the connection is not queued, it should be active, so end the activity and report it. */
      if ( conn_ptr->qpos == 0 )
      {
        /* Check if the ACL connection is streaming */
        if ( conn_ptr->conn_state == CONN_STATE_STREAMING )
        {
          /* Change the state to Connected in order to report Streaming Stopped */
          conn_ptr->conn_state = CONN_STATE_CONNECTED;
          btces_report_a2dp_chg( conn_ptr );

          /* The next block will handle reporting the ACL disconnect */
        }

        switch ( conn_ptr->conn_state )
        {
          case CONN_STATE_SETUP_INCOMING:
          case CONN_STATE_SETUP_OUTGOING:
          {
            /* Set the state to Invalid, so that a failure will be reported */
            conn_ptr->conn_state = CONN_STATE_INVALID;
            btces_report_acl_complete( conn_ptr );
            break;
          }

          case CONN_STATE_CONNECTED:
          {
            /* See if there is a SCO connection for this ACL */
            switch ( conn_ptr->sco_state )
            {
              case SCO_STATE_SETUP:
              {
                /* Set the state to Invalid, so that a failure will be reported */
                conn_ptr->sco_state = SCO_STATE_INVALID;
                btces_report_sync_complete( conn_ptr );
                break;
              }

              case SCO_STATE_SCO:
              case SCO_STATE_ESCO:
              {
                /* Report disconnect for this SCO or eSCO connection */
                btces_report_disconnect( conn_ptr->sco_handle );
                break;
              }

              /* Ignore other possible SCO states */
              default:
              {
                break;
              }
            } /* switch ( conn_ptr->sco_state ) */

            /* Now report Disconnect for this ACL connection */
            btces_report_disconnect( conn_ptr->acl_handle );
            break;
          }

          /* Ignore other possible connection states */
          default:
          {
            break;
          }
        } /* switch ( conn_ptr->conn_state ) */
      }
      /* Done with this connection entry, so free it */
      btces_pfal_free( conn_ptr );
      btces_g_state_data_ptr->conn_ptr_table[i] = NULL;

    } /* conn_ptr != NULL */
  } /* end for */
}

/*==============================================================
FUNCTION:  btces_find_conn_from_addr()
==============================================================*/

/** Find connection table entry from a given Device Address.

  This function returns a connection table address (or NULL), and the
  associated index in the connection array. The index is optionally requested
  by the caller in the case that the table entry must be freed later.

  If this service returns NULL, the requested index value is to be ignored.

  @return Pointer to the connection table entry if found or NULL.
*/

static btces_conn_data_struct * btces_find_conn_from_addr
(
  btces_bt_addr_struct  *addr_ptr,  /**< Pointer to Remote Device Address */
  uint8                 *index_ptr  /**< [out]: Pointer to table index (optional) */
)
{
  uint8                   i;
  btces_conn_data_struct *conn_ptr;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );
  BTCES_ASSERT(addr_ptr != NULL);

  for ( i = 0; i < MAX_CONNS; i++ )
  {
    conn_ptr = btces_g_state_data_ptr->conn_ptr_table[i];

    if ( (conn_ptr != NULL) &&
         ( ADDR_IS_EQUAL( addr_ptr, &(conn_ptr->addr) ) ) )
    {
      /* Match found, exit loop */
      break;
    }
  }

  /* If no match, return NULL */
  if ( i >= MAX_CONNS )
  {
    conn_ptr = NULL;
  }

  /* Optional: output the matching index */
  if (index_ptr != NULL)
  {
    *index_ptr = i;
  }

  return( conn_ptr );
}

/*==============================================================
FUNCTION:  btces_close_page_activity()
==============================================================*/

/** Close out a Paging activity if one is active. */

static void btces_close_page_activity
(
  boolean time_out  /** < [in]: Whether paging ended due to a timeout or not */
)
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( btces_g_state_data_ptr->paging_now )
  {
    btces_g_state_data_ptr->paging_now = FALSE;

    /* Increment the timer tag to ensure a pending timeout will be ignored; zero
       does not need to be avoided in this case, only when a new tag is made when
       a new timer is started.
    */
    btces_g_state_data_ptr->page_timer_tag++;

    /* Cancel the timer unless paging stopped due to a timeout. */
    if ( !time_out )
    {
      btces_pfal_stop_timer( btces_g_state_data_ptr->page_timer_id );
    }
    btces_report_paging();
  }
}

/*==============================================================
FUNCTION:  btces_close_conn_activity()
==============================================================*/

/** Close out a Connection activity. */

static void btces_close_conn_activity( void )
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  btces_g_state_data_ptr->connecting_now = FALSE;
}

/*==============================================================
FUNCTION:  btces_close_req_activity()
==============================================================*/

/** Close out a Remote Name Request activity. */

static void btces_close_req_activity( void )
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  btces_g_state_data_ptr->requesting_now = FALSE;
}

/*==============================================================
FUNCTION:  btces_close_inq_activity()
==============================================================*/

/** Close out an Inquiry activity if one is active. */

static void btces_close_inq_activity( void )
{
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  /* If an inquiry was in progress, stop it and report that event. */
  if ( btces_g_state_data_ptr->inquiry_is_active )
  {
    /* Update state data and report the event */
    btces_g_state_data_ptr->inquiry_is_active = FALSE;
    btces_report_inquiry();
  }
}

/*==============================================================
FUNCTION:  btces_page_timeout_cb()
==============================================================*/

/** When the paging timer expires, this callback is executed. */

static void btces_page_timeout_cb
(
  void *user_data     /**< [in]: Opaque data associated with the timer */
)
{
  BTCES_STATUS ret_val;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /* Make sure BTC-ES is running and grab the Token if so */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
    /* The timer tag (passed in user_data) must match the one currently set,
       else ignore this callback instance
    */
    if ( (uint32)( user_data ) == btces_g_state_data_ptr->page_timer_tag )
    {
      BTCES_MSG_LOW( "BTC-ES: Page timer expired, tag = %p" BTCES_EOL, user_data );

      /* End the current page activity (due to timeout) and report it */
      btces_close_page_activity( TRUE );
    }
    else
    {
      BTCES_MSG_LOW( "BTC-ES: Page timeout ignored, tag = %p" BTCES_EOL, user_data );
    }

    btces_pfal_release_token();
  }
}

/*==============================================================
FUNCTION:  btces_per_inq_timeout_cb()
==============================================================*/

/** When the periodic inquiry timer expires, this callback is executed. */

static void btces_per_inq_timeout_cb
(
  void *user_data     /**< [in]: Opaque data associated with the timer */
)
{
  BTCES_STATUS ret_val;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  /* Make sure BTC-ES is running and grab the Token if so */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
    /* The timer tag (passed in user_data) must match the one currently set and
       we must be in periodic inquiry mode; else ignore this callback instance
    */
    if ( (uint32)( user_data ) == btces_g_state_data_ptr->per_inq_timer_tag &&
         (btces_g_state_data_ptr->in_per_inq_mode) )
    {
      BTCES_MSG_LOW( "BTC-ES: Periodic inquiry timer expired, tag = %p" BTCES_EOL, user_data );

      if ( !(btces_g_state_data_ptr->inquiry_is_active) )
      {
        /* Create a table entry for Inquiry and queue it, or start it and report it. */
        btces_create_inq_entry();
      }
      else
      {
        BTCES_MSG_HIGH( "BTC-ES: Inquiry already in progress!" BTCES_EOL );
      }
    }
    else
    {
      BTCES_MSG_LOW( "BTC-ES: Periodic inquiry timeout ignored, tag = %p" BTCES_EOL, user_data );
    }

    btces_pfal_release_token();
  }
}

/*==============================================================
FUNCTION:  btces_start_page_timer()
==============================================================*/

/** Start a timer with the paging timeout and a new timer tag. */

static void btces_start_page_timer( void )
{
  BTCES_STATUS ret_val;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  /* Make a new non-zero page timer tag to associate with a new timer */
  if ( ++btces_g_state_data_ptr->page_timer_tag == 0)
  {
    btces_g_state_data_ptr->page_timer_tag = 1; /* Rollover occurred */
  }

  BTCES_MSG_LOW( "BTC-ES: Starting page timer, tag = %lu" BTCES_EOL, btces_g_state_data_ptr->page_timer_tag );

  /* Start the timer with the timeout, callback and the new timer tag. */
  ret_val = btces_pfal_start_timer( btces_g_state_data_ptr->page_timeout,
                                    &btces_page_timeout_cb,
                                    (void *)( btces_g_state_data_ptr->page_timer_tag ),
                                    &(btces_g_state_data_ptr->page_timer_id) );

  if ( ret_val != BTCES_OK )
  {
    BTCES_MSG_HIGH( "BTC-ES: Start timer error: %d" BTCES_EOL, ret_val );
  }
}

/*==============================================================
FUNCTION:  btces_start_per_inq_timer()
==============================================================*/

/** Start a timer with the periodic inquiry timeout and a new timer tag. */

static void btces_start_per_inq_timer( void )
{
  BTCES_STATUS ret_val;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  /* Make a new non-zero timer tag to associate with a new timer */
  if ( ++btces_g_state_data_ptr->per_inq_timer_tag == 0)
  {
    btces_g_state_data_ptr->per_inq_timer_tag = 1; /* Rollover occurred */
  }

  BTCES_MSG_LOW( "BTC-ES: Starting periodic inquiry timer, tag = %lu" BTCES_EOL, btces_g_state_data_ptr->per_inq_timer_tag );

  /* Start the timer with the timeout, callback and the new timer tag. */
  ret_val = btces_pfal_start_timer( btces_g_state_data_ptr->per_inq_timeout,
                                    &btces_per_inq_timeout_cb,
                                    (void *)( btces_g_state_data_ptr->per_inq_timer_tag ),
                                    &(btces_g_state_data_ptr->per_inq_timer_id) );

  if ( ret_val != BTCES_OK )
  {
    BTCES_MSG_HIGH( "BTC-ES: Start timer error: %d" BTCES_EOL, ret_val );
  }
}

/*==============================================================
FUNCTION:  btces_next_queue_activity()
==============================================================*/

/** Begin the next Inquiry/Paging/Connection activity if one is pending in the queue and there is no currently active activity. */

static void btces_next_queue_activity( void )
{
  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  if ( (btces_g_state_data_ptr->connecting_now) ||
       (btces_g_state_data_ptr->requesting_now) ||
       (btces_g_state_data_ptr->inquiry_is_active) )
  {
    return;
  }

  /* Dequeue the next event pair from the queue (FIFO) */
  int8 i = btces_dequeue_conn_entry();

  /* Return if there is no connection table entry in the queue. */
  if ( i == -1 )
  {
    return;
  }
  btces_conn_data_struct * next_conn_ptr = btces_g_state_data_ptr->conn_ptr_table[(uint8)i];

  switch ( next_conn_ptr->conn_state )
  {
    case CONN_STATE_REMOTE_NAME_REQUEST:
    {
      btces_g_state_data_ptr->requesting_now = TRUE;

      btces_start_page_timer();
      btces_g_state_data_ptr->paging_now = TRUE;
      btces_report_paging();
      break;
    }

    case CONN_STATE_SETUP_OUTGOING:
    {
      btces_g_state_data_ptr->connecting_now = TRUE;

      /* Report the ACL event. */
      btces_report_acl_create( next_conn_ptr );

      btces_start_page_timer();
      btces_g_state_data_ptr->paging_now = TRUE;
      btces_report_paging();
      break;
    }

    case CONN_STATE_INQUIRY:
    {
      btces_g_state_data_ptr->inquiry_is_active = TRUE;
      btces_report_inquiry();
      break;
    }

    default:
    {
      BTCES_MSG_HIGH( "BTC-ES: Unexpected connection state: %d" BTCES_EOL, next_conn_ptr->conn_state );
      break;
    }
  }
}

/*==============================================================
FUNCTION:  btces_create_conn_entry()
==============================================================*/

/** Find an open slot in the table and create a connection table entry.

    @return New connection table entry's address or NULL if error.
*/

static btces_conn_data_struct * btces_create_conn_entry
(
  btces_bt_addr_struct *addr  /**< [in}: Pointer to Bluetooth address, will be put in the new entry */
)
{
  uint8                   i;
  btces_conn_data_struct *conn_ptr;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  for ( i = 0; i < MAX_CONNS; i++ )
  {
    conn_ptr = btces_g_state_data_ptr->conn_ptr_table[i];

    if ( conn_ptr == NULL )
    {
      /* Open slot found, exit loop */
      break;
    }
  }

  /* If no slots available, return NULL */
  if ( i >= MAX_CONNS )
  {
    conn_ptr = NULL;
    BTCES_MSG_ERROR( "BTC-ES: Conn Table full!" BTCES_EOL );
  }
  else
  {
    /* Allocate and initialize a table entry  */
    conn_ptr = (btces_conn_data_struct *)btces_pfal_malloc( sizeof( *conn_ptr ) );
    if ( conn_ptr != NULL )
    {
      /* The std_memset() will do this set of actions:
      conn_ptr->conn_state = CONN_STATE_INVALID;
      conn_ptr->addr.addr[0..5] = 0;
      conn_ptr->sco_interval = 0;
      conn_ptr->sco_window = 0;
      conn_ptr->retrans_win = 0;
      conn_ptr->qpos = 0;
      */
      std_memset( conn_ptr, 0, sizeof( *conn_ptr ) );

      /* These non-zero items are initialized manually */
      conn_ptr->acl_mode = BTCES_MODE_TYPE_MAX;
      conn_ptr->acl_handle = BTCES_INVALID_CONN_HANDLE;
      conn_ptr->sco_state = SCO_STATE_INVALID;
      conn_ptr->sco_handle = BTCES_INVALID_CONN_HANDLE;

      /* Put in the given Bluetooth address */
      std_memmove( &(conn_ptr->addr), addr, sizeof(conn_ptr->addr) );

      /* Place the connection entry into the table */
      btces_g_state_data_ptr->conn_ptr_table[i] = conn_ptr;
    }
  }

  /* Return the new table entry, or else NULL */
  return( conn_ptr );
}

/*==============================================================
FUNCTION:  btces_find_conn_from_handle()
==============================================================*/

/** Find the connection table entry with this handle; could be ACL or Sync.
    If this function returns an entry, the ACL will be in Connected or
    Streaming states. The index is optionally requested by the caller in the
    case that the table entry must be freed later.

    If this service returns NULL, the requested index value is to be ignored.

    @return Pointer to the connection table entry if found or NULL.
*/

static btces_conn_data_struct * btces_find_conn_from_handle
(
  uint16  handle,     /**< [in]: Connection Handle (ACL or Sync) */
  uint8   *index_ptr  /**< [out]: Pointer to matching table index (optional) */
)
{
  uint8                   i;
  btces_conn_data_struct *conn_ptr;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  BTCES_ASSERT( btces_g_state_data_ptr != NULL );

  for ( i = 0; i < MAX_CONNS; i++ )
  {
    conn_ptr = btces_g_state_data_ptr->conn_ptr_table[i];

    if ( conn_ptr != NULL )
    {
      /* See if this is an active ACL connection first */
      if ( ( conn_ptr->conn_state == CONN_STATE_CONNECTED ) ||
           ( conn_ptr->conn_state == CONN_STATE_STREAMING ) )
      {
        /* And then is it this ACL connection? */
        if ( conn_ptr->acl_handle == handle )
        {
          /* Handle matches this ACL connection, exit loop */
          break;
        }

        /* Or is it this Sync connection? */
        if ( ( conn_ptr->sco_state == SCO_STATE_SCO ) ||
             ( conn_ptr->sco_state == SCO_STATE_ESCO ) )
        {
          if ( conn_ptr->sco_handle == handle )
          {
            /* Handle matches this Sync connection, exit loop */
            break;
          }
        }
      }
    }
  }

  /* If no match found, return NULL */
  if ( i >= MAX_CONNS )
  {
    conn_ptr = NULL;
  }

  /* Optional: output the matching index */
  if (index_ptr != NULL)
  {
    *index_ptr = i;
  }

  return( conn_ptr );
}

/*==============================================================
FUNCTION:  btces_create_inq_entry()
==============================================================*/

/** Create a connection table entry for Inquiry and queue it or start it. */

static void btces_create_inq_entry( void )
{
  btces_conn_data_struct  *conn_ptr;

  /* There should not be a connection entry with the dummy BT address */
  conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array_dummy,
                                        NULL );

  /* If there is NOT an associated connection */
  if ( conn_ptr == NULL )
  {
    /* Allocate a new connection with the dummy BT address */
    conn_ptr = btces_create_conn_entry( (btces_bt_addr_struct *)bt_addr_array_dummy );

    /* Not getting an allocation is a serious error, but check anyway */
    if ( conn_ptr != NULL )
    {
      /* This connection table entry is for HCI Inquiry */
      conn_ptr->conn_state = CONN_STATE_INQUIRY;

      /* Add a new activity to the queue */
      conn_ptr->qpos = btces_find_next_qpos();

      BTCES_MSG_LOW( "BTC-ES: CONN_STATE_INQUIRY is inserted into the queue! qpos = %d " BTCES_EOL, conn_ptr->qpos );

      /* if this is the first activity, start it right away if idle. */
      if ( conn_ptr->qpos == 1 )
      {
        /* Start an activity if idle and report it. */
        btces_next_queue_activity();
      }
    }
  }
  else
  {
    BTCES_MSG_HIGH( "BTC-ES: Some type of Inquiry already in progress!" BTCES_EOL );
  }
}


/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/
/*============================================================================
 * Externalized btces_* APIs
 *==========================================================================*/

/*==============================================================
FUNCTION:  btces_init()
==============================================================*/

/* Initialize BTC-ES. See btces.h for description. */
BTCES_STATUS btces_init( void )
{
  BTCES_STATUS            ret_val = BTCES_STATUS_ALREADY_INITIALIZED;
  btces_state_data_struct *temp_state_data_ptr;
  int                     bt_power;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* If there is no instance of State Data, BTCES is not running. */
  if ( btces_g_state_data_ptr == NULL )
  {
    /* Start-up the platform layer */
    ret_val = btces_pfal_init();

    if ( ret_val == BTCES_OK )
    {
      /* Create an instance of State Data and initialize it.
         The initialization policy for the State Data will be:
         - When BTC-ES is initialized: Completely clear it and set it up (see below)
         - When BTC-ES decides Bluetooth changed from "Off" to "On": Targeted init only
         - When BTC-ES decides Bluetooth changed from "On" to "Off": Connection
           table entries will be freed as any open connections are closed out.
       */
      temp_state_data_ptr = (btces_state_data_struct *)btces_pfal_malloc( sizeof( *temp_state_data_ptr ) );

      if ( temp_state_data_ptr != NULL )
      {
        /*
          temp_state_data_ptr->bluetooth_is_on = FALSE;
          temp_state_data_ptr->report_cb_ptr = NULL;
          temp_state_data_ptr->connecting_now = FALSE;
          temp_state_data_ptr->requesting_now = FALSE;
          temp_state_data_ptr->inquiry_is_active = FALSE;
          temp_state_data_ptr->in_per_inq_mode = FALSE;
          temp_state_data_ptr->paging_now = FALSE;
          temp_state_data_ptr->page_timer_tag = 0;
          temp_state_data_ptr->per_inq_timer_tag = 0;
          temp_state_data_ptr->page_timer_id = (void *)0;
          temp_state_data_ptr->per_inq_timer_id = (void *)0;
          temp_state_data_ptr->conn_ptr_table[0 .. MAX_CONNS-1] = NULL;
        */
        std_memset( temp_state_data_ptr, 0, sizeof( *temp_state_data_ptr ) );

        /* Pick up anything that needs something besides 0, FALSE or NULL */

        temp_state_data_ptr->page_timeout = PAGE_TIMEOUT_DEFAULT;

        /* Get the initial stack state; we rely on HCI Traffic and Native Events after that */
        ret_val = btces_pfal_get_bt_power( &bt_power );

        if ( ret_val == BTCES_OK )
        {
          if ( bt_power )
          {
            temp_state_data_ptr->bluetooth_is_on = TRUE;

            /* Inform the platform if there are any WLAN channels in use */
            if ( btces_g_wlan_chan != 0x0000 )
            {
              /* Return value doesn't affect BTC-ES initialization */
              btces_pfal_wlan_chan( btces_g_wlan_chan );
            }
          }

          /* All done. Make the public APIs callable. */
          btces_g_state_data_ptr = temp_state_data_ptr;
        }
        else /* Getting initial BT power state failed! */
        {
          btces_pfal_free( temp_state_data_ptr );

          /* btces_pfal_deinit() expects the token to be held, so just grab it here */
          btces_pfal_get_token();
          btces_pfal_deinit();
        }
      }
      else /* temp_state_data_ptr == NULL */
      {
        ret_val = BTCES_STATUS_OUT_OF_MEMORY;
      }
    }
  }

  return( ret_val );
}

/*==============================================================
FUNCTION:  btces_deinit()
==============================================================*/

/* De-initialize BTC-ES. See btces.h for description. */
BTCES_STATUS btces_deinit( void )
{
  BTCES_STATUS ret_val;
  uint8 i;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Make sure BTC-ES is running and grab the Token if so */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
    /* Free any connections left in the State Data */
    for ( i = 0 ; i < MAX_CONNS ; i++ )
    {
      if ( btces_g_state_data_ptr->conn_ptr_table[i] != NULL )
      {
        btces_pfal_free( btces_g_state_data_ptr->conn_ptr_table[i] );
      }
    }

    /* Free the State Data memory itself */
    btces_pfal_free( btces_g_state_data_ptr );

    /* Block new calls into BTC-ES APIs; this is done after this thread gets the
      Token, so any thread that has the Token can freely use the global pointer.
    */
    btces_g_state_data_ptr = NULL;

    /* Shut down the platform layer; this implicitly releases the Token */
    btces_pfal_deinit();
  }

  return( ret_val );
}

/*==============================================================
FUNCTION:  btces_register()
==============================================================*/

/* Register a report callback with BTC-ES. See btces.h for description. */
BTCES_STATUS btces_register( btces_cb_type *event_cb_ptr, void *user_data )
{
  BTCES_STATUS ret_val = BTCES_STATUS_NOT_INITIALIZED;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Make sure BTC-ES is running and grab the Token if so */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
    /* Make sure nothing is registered as the caller expects */
    if ( btces_g_state_data_ptr->report_cb_ptr == NULL )
    {
      if ( event_cb_ptr != NULL )
      {
        btces_g_state_data_ptr->report_cb_ptr = event_cb_ptr;
        btces_g_state_data_ptr->user_data = user_data;

        /* Send out a series of events according to the current BT state */
        btces_make_state_report();
      }
      else
      {
        ret_val = BTCES_STATUS_INVALID_PARAMETERS;
      }
    }
    else
    {
      ret_val = BTCES_STATUS_ALREADY_REGISTERED;
    }

    btces_pfal_release_token();
  }

  return( ret_val );
}

/*==============================================================
FUNCTION:  btces_deregister()
==============================================================*/

/* Unregister for BTC-ES reports. See btces.h for description. */
BTCES_STATUS btces_deregister
(
  void  **user_data_ptr
)
{
  BTCES_STATUS ret_val = BTCES_STATUS_NOT_INITIALIZED;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Make sure BTC-ES is running and grab the Token if so */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
    /* Make sure BTC-ES is registered as the caller expects */
    if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
    {
      /* Return the previously registered user_data value if requested */
      if ( user_data_ptr != NULL )
      {
        *user_data_ptr = btces_g_state_data_ptr->user_data;
      }
      btces_g_state_data_ptr->report_cb_ptr = NULL;
    }
    else
    {
      ret_val = BTCES_STATUS_NOT_REGISTERED;
    }

    btces_pfal_release_token();
  }

  return( ret_val );
}

/*==============================================================
FUNCTION:  btces_state_report()
==============================================================*/

/* Generate a series of events for the current BT state. See btces.h for description. */
BTCES_STATUS btces_state_report( void )
{
  BTCES_STATUS ret_val = BTCES_STATUS_NOT_INITIALIZED;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Make sure BTC-ES is running and grab the Token if so */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
    /* Since the client is requesting events, they should be registered. */
    if ( btces_g_state_data_ptr->report_cb_ptr != NULL )
    {
      /* Send out a series of events according to the current BT state */
      btces_make_state_report();
    }
    else
    {
      ret_val = BTCES_STATUS_NOT_REGISTERED;
    }

    btces_pfal_release_token();
  }

  return( ret_val );
}

/*==============================================================
FUNCTION:  btces_wlan_chan
==============================================================*/

/* Tell BTC-ES what WLAN channels are in use. See btces.h for description. */
BTCES_STATUS btces_wlan_chan
(
  uint16  wlan_channels
)
{
  BTCES_STATUS ret_val = BTCES_STATUS_INVALID_PARAMETERS;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Only accept valid channel data */
  if ( !(wlan_channels & BTCES_INVALID_WLAN_CHANS) )
  {
    /* Make sure BTC-ES is running and grab the Token if so */
    ret_val = btces_test_init();

    if ( ret_val == BTCES_OK )
    {
      /* If the WLAN channel data has changed, save it */
      if ( wlan_channels != btces_g_wlan_chan )
      {
        btces_g_wlan_chan = wlan_channels;

        /* Inform the platform if Bluetooth is in the "on" state */
        if ( btces_g_state_data_ptr->bluetooth_is_on )
        {
          ret_val = btces_pfal_wlan_chan( wlan_channels );
        }
      }

      btces_pfal_release_token();
    }
    else
    {
      /* BTC-ES is not running, so just store the channel data */
      btces_g_wlan_chan = wlan_channels;
      ret_val = BTCES_OK;
    }
  }
  return( ret_val );
}

/*============================================================================
 * Externalized btces_svc_* APIs
 *==========================================================================*/

/*==============================================================
FUNCTION:  btces_svc_native_event_in()
==============================================================*/
/** BTC-ES is told of a platform event. See btces_svc.h for details. */

void btces_svc_native_event_in
(
  btces_native_event_enum       native_event,
  btces_native_event_data_union *native_event_data_ptr
)
{
  BTCES_STATUS            ret_val;
  btces_conn_data_struct  *conn_ptr;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Make sure BTC-ES is running and grab the Token if so; else do nothing */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
    BTCES_MSG_LOW( "btces_svc_native_event_in: %d" BTCES_EOL, native_event );
    switch ( native_event )
    {
      case BTCES_NATIVE_EVENT_DEVICE_SWITCHED_ON:
      {
        /* This checks if we are not on, and sends the event if not. */
        btces_test_bt_on();
        break;
      }

      case BTCES_NATIVE_EVENT_DEVICE_SWITCHED_OFF:
      {
        /* There is only work to be done if BT is in the "On" state */
        if ( btces_g_state_data_ptr->bluetooth_is_on )
        {
          /* Close out any "open" events, just like HCI_Reset processing */
          btces_close_open_events();

          /* Finally, set the state to "Off" and send the power off event */
          btces_g_state_data_ptr->bluetooth_is_on = FALSE;
          btces_report_bt_power();
        }
        break;
      }

      case BTCES_NATIVE_EVENT_A2DP_STREAM_START:
      {
        /* Check if we thought BT was off, and send the "On" event if needed */
        btces_test_bt_on();

        if ( native_event_data_ptr != NULL )
        {
          conn_ptr = btces_find_conn_from_addr(
            (btces_bt_addr_struct *)(&(((btces_bt_addr_struct *)native_event_data_ptr)->addr)),
            NULL );

          /* If there is an associated connection and it is connected */
          if ( (conn_ptr != NULL) &&
               (conn_ptr->conn_state == CONN_STATE_CONNECTED)
             )
          {
            /* Then change the state and report it */
            conn_ptr->conn_state = CONN_STATE_STREAMING;
            btces_report_a2dp_chg( conn_ptr );
          } /* else, Take no action; BTC-ES maybe was initialized after the ACL was set up */
        }
        else
        {
          BTCES_MSG_HIGH( "BTC-ES: Stream Start Native Event: No data!" BTCES_EOL );
        }

        break;
      }

      case BTCES_NATIVE_EVENT_A2DP_STREAM_STOP:
      {
        /* Check if we thought BT was off, and send the "On" event if needed */
        btces_test_bt_on();

        if ( native_event_data_ptr != NULL )
        {
          /* Locate the existing ACL connection for the device;
             BT Address is already in Big Endian format, use it as-is
          */
          conn_ptr = btces_find_conn_from_addr(
           (btces_bt_addr_struct *)(&(((btces_bt_addr_struct *)native_event_data_ptr)->addr)),
            NULL );

          /* If there is an associated connection and it is streaming */
          if ( (conn_ptr != NULL) &&
               (conn_ptr->conn_state == CONN_STATE_STREAMING)
             )
          {
            /* Then change the state and report it */
            conn_ptr->conn_state = CONN_STATE_CONNECTED;
            btces_report_a2dp_chg( conn_ptr );
          } /* Else, Take no action; BTC-ES maybe was initialized after the ACL was set up,
              or BTC-ES already decided streaming was over due to HCI Traffic
              (like seeing the ACL disconnect first)
            */
        }
        else
        {
          BTCES_MSG_HIGH( "BTC-ES: Stream Stop Native Event: No data!" BTCES_EOL );
        }

        break;
      }

      default:
      {
        BTCES_MSG_HIGH( "BTC-ES: Unknown native event: %d" BTCES_EOL, native_event );
        break;
      }
    } /* switch( native_event ) */

    btces_pfal_release_token();
  }
}

/*==============================================================
FUNCTION:  btces_svc_hci_command_in()
==============================================================*/
/** BTC-ES is told of an HCI command. See btces_svc.h for details. */

void btces_svc_hci_command_in
(
  uint8         *hci_command_buffer_ptr,
  unsigned int  length
)
{
  BTCES_STATUS            ret_val;
  btces_conn_data_struct  *conn_ptr;
  uint16                  hci_command;
  uint16                  hci_handle;
  uint16                  time_slots;
  uint8                   hci_command_param_len;
  uint8                   i;
  uint8                   bt_addr_array[6];

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Make sure BTC-ES is running and grab the Token if so; else do nothing */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
#ifdef BTCES_DEBUG
    BTCES_MSG_LOW( "btces_svc_hci_command_in: %d bytes:" BTCES_EOL, length );
    btces_msg_w_hex( length, hci_command_buffer_ptr );
#endif

    /* Since an HCI Command happened, make sure BTC-ES and the client know BT is "On" */
    btces_test_bt_on();

    /* The caller has to have passed a buffer and a non-zero length;
       BTC-ES does not need any command parameters from some commands, but all
       should come with a parameter length field, so length > 2 is required here
       so as to fetch the parameter length field once for all commands.
    */
    if ( ( hci_command_buffer_ptr != NULL ) && ( length > 2 ) )
    {
      hci_command_param_len = GET_HCI_COMMAND_PARAM_LEN( hci_command_buffer_ptr );
      hci_command = GET_HCI_UINT16( hci_command_buffer_ptr ); /* No offset to command */

      switch( hci_command )
      {
        /* HCI_Inquiry command */
        case HCI_CMD_INQUIRY:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Inquiry" BTCES_EOL );

          /* There are no additional length checks required for this command */

          if ( !(btces_g_state_data_ptr->inquiry_is_active) &&
               !(btces_g_state_data_ptr->in_per_inq_mode) )
          {
            /* Create a connection for Inquiry */
            btces_create_inq_entry();
          }
          else
          {
            BTCES_MSG_HIGH( "BTC-ES: Inquiry (or periodic inquiry) already in progress!" BTCES_EOL );
          }
          break;
        } /* End HCI_Inquiry command */

        /* HCI_Inquiry_Cancel command */
        case HCI_CMD_INQUIRY_CANCEL:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Inquiry_Cancel" BTCES_EOL );

          /* There are no additional length checks required for this command */

          /* Find the connection entry with the dummy BT address for Inquiry */
          conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array_dummy,
                                                &i );

          /* If there is a connection entry with the dummy BT address for the Inquiry connection status */
          if ( conn_ptr != NULL )
          {
            BTCES_ASSERT( conn_ptr->conn_state == CONN_STATE_INQUIRY );

            /* If the inquiry is queued, it cannot be active, so just remove it. */
            if ( conn_ptr->qpos > 0 )
            {
              btces_remove_conn_entry_from_queue( i );
            }
            else if ( btces_g_state_data_ptr->inquiry_is_active )
            {
              /* End the inquiry activity and report it. */
              btces_close_inq_activity();

              /* If in periodic inquiry mode, set time to when the next inquiry may start */
              if ( btces_g_state_data_ptr->in_per_inq_mode )
              {
                BTCES_MSG_HIGH( "BTC-ES: Command unexpected in periodic inquiry mode!" BTCES_EOL );

                /* HCI_Inquiry_Cancel is only expected to be used with HCI_Inquiry;
                   however, the BT controller will end an inquiry even if it is in
                   Periodic Inquiry Mode. So we must start the Periodic Inquiry timer
                   anyway, as there will not be an Inquiry Complete event in this case.
                */
                btces_start_per_inq_timer();
              }
              /* Start the next activity sequence if idle. */
              btces_next_queue_activity();
            }
            /* Done with this connection entry, so free it */
            btces_pfal_free( conn_ptr );
            btces_g_state_data_ptr->conn_ptr_table[i] = NULL;
          }
          break;
        } /* End HCI_Inquiry_Cancel command */

        /* HCI_Periodic_Inquiry_Mode command */
        case HCI_CMD_PER_INQUIRY:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Periodic_Inquiry_Mode" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             command to use the parameters; +3 is for the command and length bytes.
          */
          if ( ( length >= HCI_CMD_PER_INQUIRY_LEN+3 ) &&
               ( hci_command_param_len >= HCI_CMD_PER_INQUIRY_LEN ) )
          {
            /* The time between a periodic inquiry ending and the next starting
               can be as small as (Min_Period_Length - Inquiry_Length).
               Min_Period_Length is 2 bytes, Inquiry_Length is 1 byte; both are
               in increments of 1.28 seconds; convert final answer to msec.
            */
            btces_g_state_data_ptr->per_inq_timeout =
              (GET_HCI_UINT16( hci_command_buffer_ptr+HCI_CMD_PER_INQUIRY_MIN_PER_OFST ) -
              (uint16)(hci_command_buffer_ptr[HCI_CMD_PER_INQUIRY_INQ_LEN_OFST])) * 1280;

            BTCES_MSG_LOW( "BTC-ES: Periodic Inquiry time: %d" BTCES_EOL, btces_g_state_data_ptr->per_inq_timeout );

            if ( !(btces_g_state_data_ptr->in_per_inq_mode) )
            {
              /* Update state data for Periodic Inquiry mode */
              btces_g_state_data_ptr->in_per_inq_mode = TRUE;

              if ( !(btces_g_state_data_ptr->inquiry_is_active) )
              {
                /* Create a connection for Inquiry */
                btces_create_inq_entry();
              }
              else
              {
                BTCES_MSG_HIGH( "BTC-ES: Some type of Inquiry already in progress!" BTCES_EOL );
              }
            }
            else
            {
              BTCES_MSG_HIGH( "BTC-ES: Already in Periodic Inquiry mode!" BTCES_EOL );
            }
          }
          break;
        } /* End HCI_Periodic_Inquiry_Mode command */

        /* HCI_Exit_Periodic_Inquiry_Mode command */
        case HCI_CMD_EXIT_PER_INQUIRY:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Exit_Periodic_Inquiry_Mode" BTCES_EOL );

          /* There are no additional length checks required for this command */

          if ( btces_g_state_data_ptr->in_per_inq_mode )
          {
            btces_g_state_data_ptr->in_per_inq_mode = FALSE;

            /* Find the connection entry with the dummy BT address for Inquiry */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array_dummy,
                                                  &i );

            /* If there is a connection entry for Inquiry */
            if ( conn_ptr != NULL )
            {
              BTCES_ASSERT( conn_ptr->conn_state == CONN_STATE_INQUIRY );

              /* If the Inquiry is queued, it cannot be active, so just remove it. */
              if ( conn_ptr->qpos > 0 )
              {
                btces_remove_conn_entry_from_queue( i );
              }
              else
              {
                /* The inquiry must be active, so end it and report it */
                BTCES_ASSERT( btces_g_state_data_ptr->inquiry_is_active );

                btces_close_inq_activity();

                /* Start the next activity sequence if idle. */
                btces_next_queue_activity();
              }
              /* In either case, done with this connection entry, so free it */
              btces_pfal_free( conn_ptr );
              btces_g_state_data_ptr->conn_ptr_table[i] = NULL;
            }
            else
            {
              /* We were waiting for the next periodic inquiry to start; stop the timer */
              btces_pfal_stop_timer( btces_g_state_data_ptr->per_inq_timer_id );

              /* "Expire" the timer; 0 does not need to be avoided */
              btces_g_state_data_ptr->per_inq_timer_tag++;
            }
          }
          else
          {
            BTCES_MSG_HIGH( "BTC-ES: Not in Periodic Inquiry Mode!" BTCES_EOL );
          }
          break;
        } /* End HCI_Exit_Periodic_Inquiry_Mode command */

        /* HCI_Create_Connection command */
        case HCI_CMD_CREATE_CONN:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Create_Connection" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             command to use the parameters; +3 is for the command and length bytes.
          */
          if ( ( length >= HCI_CMD_CREATE_CONN_LEN+3 ) &&
               ( hci_command_param_len >= HCI_CMD_CREATE_CONN_LEN ) )
          {
            /* Extract the BT address from the HCI command */
            GET_HCI_BT_ADDR( bt_addr_array, hci_command_buffer_ptr+HCI_CMD_CREATE_CONN_BT_ADDR_OFST );

            /* There should not be a connection entry with the given BT address */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  NULL );

            /* If there is NOT an associated connection */
            if ( conn_ptr == NULL )
            {
              /* Allocate a new connection with the remote device address */
              conn_ptr = btces_create_conn_entry( (btces_bt_addr_struct *)bt_addr_array );

              /* Not getting an allocation is a serious error, but check anyway */
              if ( conn_ptr != NULL )
              {
                /* The connection request originated from the Host device */
                conn_ptr->conn_state = CONN_STATE_SETUP_OUTGOING;

                /* Add a new activity to the queue */
                conn_ptr->qpos = btces_find_next_qpos();

                BTCES_MSG_LOW( "BTC-ES: CONN_STATE_SETUP_OUTGOING is inserted into the queue! qpos = %d " BTCES_EOL, conn_ptr->qpos );

                /* if this is the first activity, start it right away if idle. */
                if ( conn_ptr->qpos == 1 )
                {
                  /* Start an activity if idle and report it. */
                  btces_next_queue_activity();
                }
              }
            }
            else
            {
              BTCES_MSG_HIGH( "BTC-ES: HCI_Create_Connection: Connection already exists!" BTCES_EOL );
            }
          } /* End if there are enough bytes in the command */
          break;
        } /* End HCI_Create_Connection command */

         /* HCI_Add_SCO_Connection */
        case HCI_CMD_ADD_SCO_CONN:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Add_SCO_Connection" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             command to use the parameters; +3 is for the command and length bytes.
          */
          if ( ( length >= HCI_CMD_ADD_SCO_CONN_LEN+3 ) &&
               ( hci_command_param_len >= HCI_CMD_ADD_SCO_CONN_LEN ) )
          {
            /* Extract the connection handle from the HCI command */
            hci_handle = GET_HCI_UINT16( hci_command_buffer_ptr+HCI_CMD_ADD_SCO_CONN_HANDLE_OFST );

            /* Find the associated connection entry from the event's connection handle */
            conn_ptr = btces_find_conn_from_handle( hci_handle, NULL );

            if ( conn_ptr != NULL )
            {
              /* The located connection should be for an ACL, not a Sync */
              if ( conn_ptr->acl_handle == hci_handle )
              {
                /* There should not be a Sync connection */
                if ( conn_ptr->sco_state == SCO_STATE_INVALID )
                {
                  /* A Sync connection is being set up, so report it */
                  conn_ptr->sco_state = SCO_STATE_SETUP;
                  btces_report_sync_create( conn_ptr );
                }
                else
                {
                  BTCES_MSG_HIGH( "HCI_Add_SCO_Connection command: Sync Connection was not invalid: %d!" BTCES_EOL,
                                   conn_ptr->sco_state );
                }
              }
              else
              {
                BTCES_MSG_HIGH( "HCI_Add_SCO_Connection command: Handle was for a Sync connection!" BTCES_EOL );
              }
            }
          }/* End if there are enough bytes in the command */
          break;
        } /* End HCI_Add_SCO_Connection command */

        /* HCI_Remote_Name_Request command */
        case HCI_CMD_REMOTE_NAME_REQ:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Remote_Name_Request" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             command to use the parameters; +3 is for the command and length bytes.
          */
          if ( ( length >= HCI_CMD_REMOTE_NAME_REQ_LEN+3 ) &&
               ( hci_command_param_len >= HCI_CMD_REMOTE_NAME_REQ_LEN ) )
          {
            /* Extract the BT address from the HCI command */
            GET_HCI_BT_ADDR( bt_addr_array, hci_command_buffer_ptr+HCI_CMD_REMOTE_NAME_REQ_BT_ADDR_OFST );

            /* There might be a connection entry with the given BT address */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  NULL );

            /* If there is NOT an associated connection, paging will occur */
            if ( conn_ptr == NULL )
            {
              /* Allocate a new connection with the remote device address */
              conn_ptr = btces_create_conn_entry( (btces_bt_addr_struct *)bt_addr_array );

              /* Not getting an allocation is a serious error, but check anyway */
              if ( conn_ptr != NULL )
              {
                /* This connection is only for a Remote Name Request */
                conn_ptr->conn_state = CONN_STATE_REMOTE_NAME_REQUEST;

                /* Add a new activity to the queue */
                conn_ptr->qpos = btces_find_next_qpos();

                BTCES_MSG_LOW( "BTC-ES: CONN_STATE_REMOTE_NAME_REQUEST is inserted into the queue! qpos = %d " BTCES_EOL, conn_ptr->qpos );

                /* if this is the first activity, start it right away if idle. */
                if ( conn_ptr->qpos == 1 )
                {
                  /* Start an activity if idle and report it. */
                  btces_next_queue_activity();
                }
              }
            } /* Else, there is an ACL connection and paging is not needed */
          } /* End if there are enough bytes in the command */
          break;
        } /* End HCI_Remote_Name_Request command */

        /* HCI_Write_Page_Timeout command */
        case HCI_CMD_WRITE_PAGE_TIMEOUT:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Write_Page_Timeout" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             command to use the parameters; +3 is for the command and length bytes.
          */
          if ( ( length >= HCI_CMD_WRITE_PAGE_TIMEOUT_LEN+3 ) &&
               ( hci_command_param_len >= HCI_CMD_WRITE_PAGE_TIMEOUT_LEN ) )
          {
            /* Extract the timeout value (slots), convert to msec (0.625 = 5/8),
               and update BTC-ES State Data; ensure resulting timeout is non-zero,
               and that the Host is not attempting to write 0, which is invalid.
               Note that BTC-ES does not look for a successful command complete
               event for this command, and so presumes the SoC will take it.
            */
            time_slots = GET_HCI_UINT16( hci_command_buffer_ptr+HCI_CMD_WRITE_PAGE_TIMEOUT_TIME_OFST );
            if ( time_slots > 0 )
            {
              btces_g_state_data_ptr->page_timeout = ( time_slots * 5 ) / 8;
              if (btces_g_state_data_ptr->page_timeout == 0)
              {
                btces_g_state_data_ptr->page_timeout = 1;
              }
            }
          } /* End if there are enough bytes in the command */
          break;
        } /* End HCI_Write_Page_Timeout command */

        /* HCI_Reset command */
        case HCI_CMD_RESET:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Reset" BTCES_EOL );

          /* There are no additional length checks required for this command */

          /* Close out any open BTC-ES events */
          btces_close_open_events();

          /* Do limited initialization of BTC-ES State Data due to the reset:
             - Bluetooth is already "On" from btces_test_bt_on()
             - Registered callback and user data are left as-is
             - Timer Tags were incremented by btces_close_open_events()
             - Timer IDs are left as-is (no need to initialize)
             - Inquiry, Peridoic Inquiry and Paging flags are FALSE from btces_close_open_events()
             - The connection table is empty from btces_close_open_events()

             So that leaves the Page Timeout.
          */

          /* Set the Page Timeout value back to its default */
          btces_g_state_data_ptr->page_timeout = PAGE_TIMEOUT_DEFAULT;

          /* Inform the platform if there are any WLAN channels in use,
             since HCI_Reset clears out the SoC's AFH data
          */
          if ( btces_g_wlan_chan != 0x0000 )
          {
            /* Return value doesn't affect HCI_Reset processing */
            btces_pfal_wlan_chan( btces_g_wlan_chan );
          }

          break;
        } /* End HCI_Reset command */

        /* HCI_Setup_Synchronous_Connection command */
        case HCI_CMD_SETUP_SYNC_CONN:
        {
          BTCES_MSG_LOW( "btces_svc_hci_command_in: HCI_Setup_Synchronous_Connection" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             command to use the parameters; +3 is for the command and length bytes.
          */
          if ( ( length >= HCI_CMD_SETUP_SYNC_CONN_LEN+3 ) &&
               ( hci_command_param_len >= HCI_CMD_SETUP_SYNC_CONN_LEN ) )
          {
            /* Extract the connection handle from the HCI command */
            hci_handle = GET_HCI_UINT16( hci_command_buffer_ptr+HCI_CMD_SETUP_SYNC_CONN_HANDLE_OFST );

            /* Find the associated connection entry from the event's connection handle */
            conn_ptr = btces_find_conn_from_handle( hci_handle, NULL );

            if ( conn_ptr != NULL )
            {
              /* The handle in the command can target an existing ACL connection
                (to create a SCO/eSCO link) or, it can target an existing sync
                connection (to modify an eSCO link). Only report a sync connection
                being created here. The Synchronous Connection Changed Event happens
                in the case of an eSCO link being modified).
              */

              /* Make sure the located connection is for an ACL, not a Sync */
              if ( conn_ptr->acl_handle == hci_handle )
              {
                /* There should not be a Sync connection */
                if ( conn_ptr->sco_state == SCO_STATE_INVALID )
                {
                  /* A Sync connection is being set up, so report it */
                  conn_ptr->sco_state = SCO_STATE_SETUP;
                  btces_report_sync_create( conn_ptr );
                }
                else
                {
                  BTCES_MSG_HIGH( "BTC-ES: HCI_Setup_Synchronous_Connection: Unexpected Sync Connection state: %d!" BTCES_EOL,
                                   conn_ptr->sco_state );
                }
              }
            }
          } /* End if there are enough bytes in the command */
          break;
        } /* End HCI_Setup_Synchronous_Connection command */

        default:
        {
          /* Some other HCI command that BTC-ES does not care about */
          BTCES_MSG_LOW( "btces_svc_hci_command_in: command ignored" BTCES_EOL );
          break;
        }
      } /* End switch ( hci_command ) */
    } /* End if (hci_command_buffer_ptr != NULL ) && (length > 2) */
    else
    {
      BTCES_MSG_HIGH( "btces_svc_hci_command_in: Invalid parameters!" BTCES_EOL );
    }

    btces_pfal_release_token();

  } /* End if BTC-ES is running */
} /* End of btces_svc_hci_command_in() */

/*==============================================================
FUNCTION:  btces_svc_hci_event_in()
==============================================================*/
/** BTC-ES is told of an HCI event. See btces_svc.h for details. */

void btces_svc_hci_event_in
(
  uint8         *hci_event_buffer_ptr,
  unsigned int  length
)
{
  BTCES_STATUS            ret_val;
  btces_conn_data_struct  *conn_ptr;
  uint8                   mode;
  uint16                  hci_handle;
  uint8                   hci_event;
  uint8                   hci_event_param_len;
  uint8                   link_type;
  uint8                   i;
  uint8                   bt_addr_array[6];

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Make sure BTC-ES is running and grab the Token if so; else do nothing */
  ret_val = btces_test_init();

  if ( ret_val == BTCES_OK )
  {
#ifdef BTCES_DEBUG
    BTCES_MSG_LOW( "btces_svc_hci_event_in: %d bytes:" BTCES_EOL, length );
    btces_msg_w_hex( length, hci_event_buffer_ptr );
#endif

    /* Since an HCI Event happened, make sure BTC-ES and the client know BT is "On" */
    btces_test_bt_on();

    /* The caller has to have passed a buffer and a non-zero length;
       BTC-ES does not need any event parameters from the Inquiry Complete event,
       but all should come with a Status field, so length > 1 is required here so
       as to fetch the parameter length field once for all events.
    */
    if ( ( hci_event_buffer_ptr != NULL ) && ( length > 1 ) )
    {
      hci_event_param_len = GET_HCI_EVENT_PARAM_LEN( hci_event_buffer_ptr );
      hci_event = GET_HCI_EVENT_OPCODE( hci_event_buffer_ptr );

      switch( hci_event )
      {
        /* Inquiry Complete event */
        case HCI_EVENT_INQUIRY_COMP:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Inquiry Complete" BTCES_EOL );

          /* There are no additional length checks required for this event */

          /* Find the connection entry with the dummy BT address for Inquiry */
          conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array_dummy,
                                                &i );

          /* If there is a connection entry with the dummy BT address for the Inquiry connection status */
          if ( conn_ptr != NULL )
          {
            BTCES_ASSERT( conn_ptr->conn_state == CONN_STATE_INQUIRY );

            /* If the connection is queued, it cannot be active, so just remove it. */
            if ( conn_ptr->qpos > 0 )
            {
              btces_remove_conn_entry_from_queue( i );
            }
            else if ( btces_g_state_data_ptr->inquiry_is_active )
            {
              /* End the inquiry activity and report it. */
              btces_close_inq_activity();

              /* If in periodic inquiry mode, set time to when the next inquiry may start */
              if ( btces_g_state_data_ptr->in_per_inq_mode )
              {
                btces_start_per_inq_timer();
              }
              /* Start the next activity sequence if idle. */
              btces_next_queue_activity();
            }
            /* Done with this connection entry, so free it */
            btces_pfal_free( conn_ptr );
            btces_g_state_data_ptr->conn_ptr_table[i] = NULL;
          }
          break;
        } /* case HCI_EVENT_INQUIRY_COMP: */

        /*  Connection Complete event */
        case HCI_EVENT_CONNECT_COMP:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Connection Complete" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_CONNECT_COMP_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_CONNECT_COMP_LEN ) )
          {
            /* Extract the BT address from the HCI event */
            GET_HCI_BT_ADDR( bt_addr_array, hci_event_buffer_ptr+HCI_EVENT_CONNECT_COMP_BT_ADDR_OFST );

            /* Find the associated connection entry from the event's BT Addr */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  &i );

            /* If there is an associated connection */
            if ( conn_ptr != NULL )
            {
              /* Action depends on Link Type */
              link_type = btces_byte_to_link( hci_event_buffer_ptr[HCI_EVENT_CONNECT_COMP_LINK_TYPE_OFST] );
              if ( link_type == BTCES_LINK_TYPE_ACL )
              {
                /* Take this action if an incoming ACL was being set up */
                if ( conn_ptr->conn_state == CONN_STATE_SETUP_INCOMING )
                {
                  /* If the connection set-up failed */
                  if ( hci_event_buffer_ptr[HCI_EVENT_CONNECT_COMP_STATUS_OFST] !=
                       HCI_EVENT_STATUS_SUCCESS )
                  {
                    conn_ptr->conn_state = CONN_STATE_INVALID;
                  }
                  else /* Connection set-up was successful */
                  {
                    conn_ptr->conn_state = CONN_STATE_CONNECTED;
                    conn_ptr->acl_mode = BTCES_MODE_TYPE_ACTIVE;
                    conn_ptr->acl_handle = GET_HCI_UINT16(hci_event_buffer_ptr+HCI_EVENT_CONNECT_COMP_HANDLE_OFST);
                  }
                  /* Report the ACL connection complete. */
                  btces_report_acl_complete( conn_ptr );

                  /* If the connection set-up failed, now free the connection table entry */
                  if ( conn_ptr->conn_state == CONN_STATE_INVALID )
                  {
                    btces_pfal_free( conn_ptr );
                    btces_g_state_data_ptr->conn_ptr_table[i] = NULL;
                  }
                }
                /* Take this action if an outgoing ACL was being set up */
                else if ( conn_ptr->conn_state == CONN_STATE_SETUP_OUTGOING )
                {
                  /* The connection entry is not expected to be queued */
                  if ( conn_ptr->qpos > 0 )
                  {
                    BTCES_MSG_HIGH( "BTC-ES: Connection Complete HCI Event: Bad connection entry!" BTCES_EOL );
                    btces_remove_conn_entry_from_queue( i );
                  }
                  /* If the connection set-up failed */
                  if ( hci_event_buffer_ptr[HCI_EVENT_CONNECT_COMP_STATUS_OFST] !=
                       HCI_EVENT_STATUS_SUCCESS )
                  {
                    conn_ptr->conn_state = CONN_STATE_INVALID;
                  }
                  else /* Connection set-up was successful */
                  {
                    conn_ptr->conn_state = CONN_STATE_CONNECTED;
                    conn_ptr->acl_mode = BTCES_MODE_TYPE_ACTIVE;
                    conn_ptr->acl_handle = GET_HCI_UINT16(hci_event_buffer_ptr+HCI_EVENT_CONNECT_COMP_HANDLE_OFST);
                  }
                  /* End possible page activity (no timeout) and report it. */
                  btces_close_page_activity( FALSE );

                  /* Since connection setup was in progress, close it. */
                  btces_close_conn_activity();

                  /* Then report the ACL connection complete. */
                  btces_report_acl_complete( conn_ptr );

                  /* Start the next activity sequence if idle. */
                  btces_next_queue_activity();

                  /* If the connection set-up failed, now free the connection table entry */
                  if ( conn_ptr->conn_state == CONN_STATE_INVALID )
                  {
                    btces_pfal_free( conn_ptr );
                    btces_g_state_data_ptr->conn_ptr_table[i] = NULL;
                  }
                }
              }
              else if ( link_type == BTCES_LINK_TYPE_SCO )
              {
                /* Only take action if the ACL is connected (or streaming) and
                   a sync connection was being set up
                */
                if ( ( ( conn_ptr->conn_state == CONN_STATE_CONNECTED ) ||
                       ( conn_ptr->conn_state == CONN_STATE_STREAMING ) ) &&
                     ( conn_ptr->sco_state == SCO_STATE_SETUP ) )
                {
                  /* If the SCO connection set-up failed */
                  if ( hci_event_buffer_ptr[HCI_EVENT_CONNECT_COMP_STATUS_OFST] !=
                       HCI_EVENT_STATUS_SUCCESS )
                  {
                    conn_ptr->sco_state = SCO_STATE_INVALID;
                  }
                  else /* SCO connection was successful */
                  {
                    conn_ptr->sco_state = SCO_STATE_SCO;
                    conn_ptr->sco_handle = GET_HCI_UINT16(hci_event_buffer_ptr+HCI_EVENT_CONNECT_COMP_HANDLE_OFST);
                    conn_ptr->sco_interval = 6; /* Assumed; see BTC-ES HLD Doc */
                    conn_ptr->sco_window = 2+0; /* 2 (estimated) + Retransmission Window */
                    conn_ptr->retrans_win = 0;  /* Always 0 for SCO links */
                  }
                  /* Now report the event */
                  btces_report_sync_complete( conn_ptr );
                }
              }
              else
              {
                BTCES_MSG_HIGH( "BTC-ES: Unexpected Connection Complete Link Type: %d" BTCES_EOL, link_type );
              }
            } /* End if there is an associated connection */
          }  /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_CONNECT_COMP: */

        /*  Connection Request event */
        case HCI_EVENT_CONNECT_REQ:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Connection Request" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_CONNECT_REQ_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_CONNECT_REQ_LEN ) )
          {
            /* Extract the BT address from the HCI event */
            GET_HCI_BT_ADDR( bt_addr_array, hci_event_buffer_ptr+HCI_EVENT_CONNECT_REQ_BT_ADDR_OFST );

            /* Find the associated connection entry from the event's BT Addr (if any) */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  NULL );

            /* Action depends on link type in the command */
            link_type = btces_byte_to_link( hci_event_buffer_ptr[HCI_EVENT_CONNECT_REQ_LINK_TYPE_OFST] );

            if ( link_type == BTCES_LINK_TYPE_ACL )
            {
              /* It is expected that there is NOT a connection entry here */
              if ( conn_ptr == NULL )
              {
                /* Allocate a new connection with the incoming device address */
                conn_ptr = btces_create_conn_entry( (btces_bt_addr_struct *)( bt_addr_array ) );

                /* Not getting an allocation is a serious error, but check anyway */
                if ( conn_ptr != NULL )
                {
                  /* The connection request originated from the remote device */
                  conn_ptr->conn_state = CONN_STATE_SETUP_INCOMING;

                  /* Report the ACL event. */
                  btces_report_acl_create( conn_ptr );
                }
              }
              else
              {
                BTCES_MSG_HIGH( "BTC-ES: Connect Req HCI Event: ACL table entry already exists!" BTCES_EOL );
              }
            }
            else if ( ( link_type == BTCES_LINK_TYPE_SCO ) ||
                      ( link_type == BTCES_LINK_TYPE_ESCO ) )
            {
              /* It is expected that there IS a connection, it is in the connected
                 state (or streaming), and that the SCO state is invalid
              */
              if ( ( conn_ptr != NULL ) &&
                   ( ( conn_ptr->conn_state == CONN_STATE_CONNECTED ) ||
                     ( conn_ptr->conn_state == CONN_STATE_STREAMING ) ) &&
                   ( conn_ptr->sco_state == SCO_STATE_INVALID ) )
              {
                /* Indicate a Sync connection is being set up, and report it */
                conn_ptr->sco_state = SCO_STATE_SETUP;
                btces_report_sync_create( conn_ptr );
              }
            }
            else
            {
              BTCES_MSG_HIGH( "BTC-ES: Connect Req HCI Event: Unknown Link Type %d" BTCES_EOL, link_type );
            }
          } /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_CONNECT_REQ: */

        /*  Disconnection Complete event */
        case HCI_EVENT_DISCONNECT_COMP:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Disconnection Complete" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_DISCONNECT_COMP_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_DISCONNECT_COMP_LEN ) )
          {
            /* The status field in this event will be ignored, since if the Host
               initiated a disconnect command, it probably will ignore any error
               and consider the disconnection complete. */

            /* Extract the connection handle from the HCI event */
            hci_handle = GET_HCI_UINT16( hci_event_buffer_ptr+HCI_EVENT_DISCONNECT_COMP_HANDLE_OFST );

            /* Find the associated connection entry from the event's connection handle */
            conn_ptr = btces_find_conn_from_handle( hci_handle, &i );

            if ( conn_ptr != NULL )
            {
              /* See if it is a Sync connection to be disconnected */
              if ( ( ( conn_ptr->sco_state == SCO_STATE_SCO ) ||
                     ( conn_ptr->sco_state == SCO_STATE_ESCO ) ) &&
                   ( conn_ptr->sco_handle == hci_handle ) )
              {
                /* Clear out the Sync connection's state and information */
                conn_ptr->sco_state = SCO_STATE_INVALID;
                conn_ptr->sco_handle = BTCES_INVALID_CONN_HANDLE;
                conn_ptr->sco_interval = 0;
                conn_ptr->sco_window = 0;
                conn_ptr->retrans_win = 0;

                /* Report the Sync disconnection */
                btces_report_disconnect( hci_handle );
              }
              else /* It must be that it is the ACL to be disconnected */
              {
                /* Do some extra checking and reporting if this ACL
                   connection is doing more that just being in the
                   connected state.
                */

                /* If streaming, report streaming has ended */
                if ( conn_ptr->conn_state == CONN_STATE_STREAMING )
                {
                  conn_ptr->conn_state = CONN_STATE_CONNECTED;
                  btces_report_a2dp_chg( conn_ptr );
                }

                /* If there is a Sync Connection being set up, report that it failed */
                if ( conn_ptr->sco_state == SCO_STATE_SETUP )
                {
                  conn_ptr->sco_state = SCO_STATE_INVALID;
                  btces_report_sync_complete( conn_ptr );
                }
                /* Else if there is an active sync connection, disconnect it */
                else if ( ( conn_ptr->sco_state == SCO_STATE_SCO ) ||
                          ( conn_ptr->sco_state == SCO_STATE_ESCO ) )
                {
                  btces_report_disconnect( conn_ptr->sco_handle );
                }

                /* Now report the ACL disconnection itself */
                btces_report_disconnect( hci_handle );

                /* And free the connection in the table */
                btces_pfal_free( conn_ptr );
                btces_g_state_data_ptr->conn_ptr_table[i] = NULL;

              } /* End Sync or ACL disconnect */
            } /* End if conn_ptr != NULL */
          } /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_DISCONNECT_COMP: */

        /*  Remote Name Request Complete event */
        case HCI_EVENT_REMOTE_NAME_REQ_COMP:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Remote Name Request Complete" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_REMOTE_NAME_REQ_COMP_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_REMOTE_NAME_REQ_COMP_LEN ) )
          {
            /* Extract the BT address from the HCI event */
            GET_HCI_BT_ADDR( bt_addr_array, hci_event_buffer_ptr+HCI_EVENT_REMOTE_NAME_REQ_COMP_BT_ADDR_OFST );

            /* Find the associated connection entry from the event's BT Addr */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  &i );

            /* If there is an associated connection */
            if ( conn_ptr != NULL )
            {
              /* If the connection entry was made just for the name request */
              if ( conn_ptr->conn_state == CONN_STATE_REMOTE_NAME_REQUEST )
              {
                /* The connection entry is not expected to be queued */
                if ( conn_ptr->qpos > 0 )
                {
                  BTCES_MSG_HIGH( "BTC-ES: Remote Name Request Complete HCI Event: Bad connection entry!" BTCES_EOL );
                  btces_remove_conn_entry_from_queue( i );
                }
                /* If paging was in progress */
                if ( btces_g_state_data_ptr->paging_now )
                {
                  /* End the current page activity (no timeout) and report it. */
                  btces_close_page_activity( FALSE );
                }
                /* Since remote name request was in progress, close it. */
                btces_close_req_activity();

                /* Start the next activity sequence if idle. */
                btces_next_queue_activity();

                /* Done with this connection entry, so free it */
                btces_pfal_free( conn_ptr );
                btces_g_state_data_ptr->conn_ptr_table[i] = NULL;
              } /* End if this is a Remote Name Request connection entry */
            } /* End if conn_ptr != NULL */
          } /* End if there are enough bytes in the event */
          break;
        } /* HCI_EVENT_REMOTE_NAME_REQ_COMP: */

        /* Command Complete event */
        case HCI_EVENT_COMMAND_COMP:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Command Complete" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
             This command is only used to see the reply to HCI_CMD_READ_PAGE_TIMEOUT,
             and so the length tests below are set up specifically for that case.
          */
          if ( ( length >= HCI_EVENT_COMMAND_COMP_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_COMMAND_COMP_LEN ) )
          {
            /* If this Command Complete event is for HCI_Read_Page_Timeout */
            if ( GET_HCI_UINT16( hci_event_buffer_ptr+HCI_EVENT_COMMAND_COMP_CMD_OFST ) ==
                 HCI_CMD_READ_PAGE_TIMEOUT )
            {
              /* and reading the page timeout was successful */
              if ( hci_event_buffer_ptr[HCI_EVENT_COMMAND_COMP_READ_PAGE_STATUS_OFST] ==
                   HCI_EVENT_STATUS_SUCCESS )
              {
                /* Extract the timeout value (slots), convert to msec (0.625 = 5/8),
                   and update BTC-ES State Data; ensure resulting timeout is non-zero.
                */
                btces_g_state_data_ptr->page_timeout =
                  ( GET_HCI_UINT16( hci_event_buffer_ptr+HCI_EVENT_COMMAND_COMP_READ_PAGE_TIMEOUT_OFST ) * 5 ) / 8;
                if ( btces_g_state_data_ptr->page_timeout == 0 )
                {
                  btces_g_state_data_ptr->page_timeout = 1;
                }
              }
            }
          }
          break;
        } /* case HCI_EVENT_COMMAND_COMP: */

        /* Role Change event */
        case HCI_EVENT_ROLE_CHANGE:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Role Change" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_ROLE_CHANGE_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_ROLE_CHANGE_LEN ) )
          {
            /* Extract the BT address from the HCI event */
            GET_HCI_BT_ADDR( bt_addr_array, hci_event_buffer_ptr+HCI_EVENT_ROLE_CHANGE_BT_ADDR_OFST );

            /* Find the associated connection entry from the event's BT Addr */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  &i );

            /* If there is an associated connection */
            if ( conn_ptr != NULL )
            {
              /* If an outgoing ACL connection is being set up */
              if ( conn_ptr->conn_state == CONN_STATE_SETUP_OUTGOING )
              {
                /* If the connection is queued, it cannot be active, so just remove it. */
                if ( conn_ptr->qpos > 0 )
                {
                  BTCES_MSG_HIGH( "BTC-ES: Role Change HCI Event: Bad connection entry!" BTCES_EOL );
                  btces_remove_conn_entry_from_queue( i );
                }
                /* End possible page activity (no timeout) and report it. */
                btces_close_page_activity( FALSE );
              }
            } /* End if conn_ptr != NULL */
          } /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_ROLE_CHANGE: */

        /* Mode Change event */
        case HCI_EVENT_MODE_CHANGE:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Mode Change" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_MODE_CHANGE_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_MODE_CHANGE_LEN ) )
          {
            /* Only process this event if the mode change was successful */
            if ( hci_event_buffer_ptr[HCI_EVENT_MODE_CHANGE_STATUS_OFST] ==
                 HCI_EVENT_STATUS_SUCCESS )
            {
              /* Extract the connection handle from the HCI event */
              hci_handle = GET_HCI_UINT16( hci_event_buffer_ptr+HCI_EVENT_MODE_CHANGE_HANDLE_OFST );

              /* Find the associated connection entry from the event's connection handle */
              conn_ptr = btces_find_conn_from_handle( hci_handle, NULL );

              if ( conn_ptr != NULL )
              {
                /* The located connection should be for an ACL, not a Sync */
                if ( conn_ptr->acl_handle == hci_handle )
                {
                  /* If the mode in the event is valid and different than the current mode */
                  mode = btces_byte_to_mode( hci_event_buffer_ptr[HCI_EVENT_MODE_CHANGE_MODE_OFST] );
                  if ( ( mode != BTCES_MODE_TYPE_MAX ) && ( mode != conn_ptr->acl_mode ) )
                  {
                    /* Update the connection and report the new mode */
                    conn_ptr->acl_mode = mode;
                    btces_report_mode_chg( conn_ptr );
                  }
                }
                else
                {
                  BTCES_MSG_HIGH( "BTC-ES: Mode Change HCI Event: Bad handle!" BTCES_EOL );
                }
              }
            } /* End if the Mode Change status was a success */
          } /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_MODE_CHANGE: */

        /* PIN Code Request event and Link Key Request event can have the same
           case, as the command formats are the same and the action is the same
        */
        case HCI_EVENT_PIN_CODE_REQ:
        case HCI_EVENT_LINK_KEY_REQ:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: PIN Code or Link Key Request" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_PIN_CODE_REQ_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_PIN_CODE_REQ_LEN ) )
          {
            /* Extract the BT address from the HCI event */
            GET_HCI_BT_ADDR( bt_addr_array, hci_event_buffer_ptr+HCI_EVENT_PIN_CODE_REQ_BT_ADDR_OFST );

            /* Find the associated connection entry from the event's BT Addr */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  &i );

            /* If there is an associated connection */
            if ( conn_ptr != NULL )
            {
              /* If an outgoing ACL connection is being set up */
              if ( conn_ptr->conn_state == CONN_STATE_SETUP_OUTGOING )
              {
                /* If the connection is queued, it cannot be active, so just remove it. */
                if ( conn_ptr->qpos > 0 )
                {
                  BTCES_MSG_HIGH( "BTC-ES: PIN Code or Link Key Request HCI Event: Bad connection entry!" BTCES_EOL );
                  btces_remove_conn_entry_from_queue( i );
                }
                /* End possible page activity (no timeout) and report it. */
                btces_close_page_activity( FALSE );
              }
            } /* End if conn_ptr != NULL */
          } /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_PIN_CODE_REQ:, case HCI_EVENT_LINK_KEY_REQ: */

        /* Synchronous Connection Complete event */
        case HCI_EVENT_SYNC_CONNECT_COMP:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Synchronous Connection Complete" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_SYNC_CONNECT_COMP_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_SYNC_CONNECT_COMP_LEN ) )
          {
            /* Extract the BT address from the HCI event */
            GET_HCI_BT_ADDR( bt_addr_array, hci_event_buffer_ptr+HCI_EVENT_SYNC_CONNECT_COMP_BT_ADDR_OFST );

            /* Find the associated connection entry from the event's BT Addr */
            conn_ptr = btces_find_conn_from_addr( (btces_bt_addr_struct *)bt_addr_array,
                                                  NULL );

            /* If there is an associated connection */
            if ( conn_ptr != NULL )
            {
              link_type = btces_byte_to_link( hci_event_buffer_ptr[HCI_EVENT_SYNC_CONNECT_COMP_LINK_TYPE_OFST] );

              /* If the event's link type is SCO or eSCO */
              if ( ( link_type == BTCES_LINK_TYPE_SCO ) ||
                   ( link_type == BTCES_LINK_TYPE_ESCO ) )
              {
                /* If the ACL is connected/streaming and Sync is being set up */
                if ( ( ( conn_ptr->conn_state == CONN_STATE_CONNECTED ) ||
                       ( conn_ptr->conn_state == CONN_STATE_STREAMING ) ) &&
                     ( conn_ptr->sco_state == SCO_STATE_SETUP ) )
                {
                  /* If the Sync connection set-up failed */
                  if ( hci_event_buffer_ptr[HCI_EVENT_SYNC_CONNECT_COMP_STATUS_OFST] !=
                       HCI_EVENT_STATUS_SUCCESS )
                  {
                    conn_ptr->sco_state = SCO_STATE_INVALID;
                  }
                  else /* Sync connection was successful */
                  {
                    if ( link_type == BTCES_LINK_TYPE_SCO )
                    {
                      conn_ptr->sco_state = SCO_STATE_SCO;
                      conn_ptr->sco_interval = 6; /* Assumed, see BTC-ES HLD Doc; this field is 0 for SCO links */
                    }
                    else
                    {
                      conn_ptr->sco_state = SCO_STATE_ESCO;
                      conn_ptr->sco_interval = hci_event_buffer_ptr[HCI_EVENT_SYNC_CONNECT_COMP_TX_INT_OFST];
                    }
                    /* See the BTC-ES HLD document regarding the SCO Window value */
                    conn_ptr->sco_handle = GET_HCI_UINT16( hci_event_buffer_ptr+HCI_EVENT_SYNC_CONNECT_COMP_HANDLE_OFST );
                    conn_ptr->retrans_win = hci_event_buffer_ptr[HCI_EVENT_SYNC_CONNECT_COMP_RETX_WIN_OFST];
                    conn_ptr->sco_window = 2 + conn_ptr->retrans_win; /* 2 (estimated) + Retransmission Window */
                  }
                  /* Now report the event */
                  btces_report_sync_complete( conn_ptr );
                }
              }
            } /* End if conn_ptr != NULL */
          } /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_SYNC_CONNECT_COMP: */

        /* Synchronous Connection Changed event */
        case HCI_EVENT_SYNC_CONNECT_CHANGED:
        {
          BTCES_MSG_LOW( "btces_svc_hci_event_in: Synchronous Connection Changed" BTCES_EOL );

          /* Make sure enough bytes were passed, and there are enough bytes in the
             event to use the event parameters; +2 is for the event and length bytes.
          */
          if ( ( length >= HCI_EVENT_SYNC_CONNECT_CHANGED_LEN+2 ) &&
               ( hci_event_param_len >= HCI_EVENT_SYNC_CONNECT_CHANGED_LEN ) )
          {
            /* Extract the connection handle from the HCI event */
            hci_handle = GET_HCI_UINT16( hci_event_buffer_ptr+HCI_EVENT_SYNC_CONNECT_CHANGED_HANDLE_OFST );

            /* Find the associated connection entry from the event's connection handle */
            conn_ptr = btces_find_conn_from_handle( hci_handle, NULL );

            if ( conn_ptr != NULL )
            {
              /* It should be the Sync connection that matched */
              if ( conn_ptr->sco_handle == hci_handle )
              {
                /* Update the Sync connection's information and report it;
                   see the BTC-ES HLD document regarding the SCO Window value
                */
                conn_ptr->sco_interval = hci_event_buffer_ptr[HCI_EVENT_SYNC_CONNECT_CHANGED_TX_INT_OFST];
                conn_ptr->retrans_win = hci_event_buffer_ptr[HCI_EVENT_SYNC_CONNECT_CHANGED_RETX_WIN_OFST];
                conn_ptr->sco_window = 2 + conn_ptr->retrans_win; /* 2 (estimated) + Retransmission Window */

                btces_report_sync_change( conn_ptr );
              }
              else
              {
                BTCES_MSG_HIGH( "BTC-ES: HCI Sync Connection Changed event: Bad handle!" BTCES_EOL );
              }
            } /* End if conn_ptr != NULL */
          } /* End if there are enough bytes in the event */
          break;
        } /* case HCI_EVENT_SYNC_CONNECT_CHANGED: */

        default:
        {
          /* Some other HCI Event that BTC-ES does not care about */
          BTCES_MSG_LOW( "btces_svc_hci_event_in: event ignored" BTCES_EOL );
          break;
        }
      } /* End switch ( hci_event ) */
    } /* End if (hci_event_buffer_ptr != NULL ) && (length > 1) */
    else
    {
      BTCES_MSG_HIGH( "btces_svc_hci_event_in: Invalid parameters!" BTCES_EOL );
    }

    btces_pfal_release_token();

  } /* ret_val == BTCES_OK: Is BTC-ES running ?*/
} /* End of btces_svc_hci_event_in() */

/* End of btces_api.cpp */
