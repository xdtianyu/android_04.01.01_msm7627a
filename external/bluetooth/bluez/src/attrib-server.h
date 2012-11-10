/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __packed
#define __packed __attribute__((packed))
#endif

struct server_def_val128 {
	uint128_t u128;
} __packed;

struct server_def_val16 {
	uint16_t u16;
} __packed;

struct include_def_val128 {
	uint16_t start;
	uint16_t end;
	uint128_t u128;
} __packed;

struct include_def_val16 {
	uint16_t start;
	uint16_t end;
	uint16_t u16;
} __packed;

struct char_def_val128 {
	uint8_t props;
	uint16_t handle;
	uint128_t u128;
} __packed;

struct char_def_val16 {
	uint8_t props;
	uint16_t handle;
	uint16_t uuid;
} __packed;

struct char_desc_aggregate{
	uint16_t handles[0];
} __packed;

#define ATT_ATTR_NOT_FOUND		"ATT_ATTR_NOT_FOUND"
#define ATT_INVALID_HANDLE		"ATT_INVALID_HANDLE"
#define ATT_READ_NOT_PERM		"ATT_READ_NOT_PERM"
#define ATT_WRITE_NOT_PERM		"ATT_WRITE_NOT_PERM"
#define ATT_INSUFF_AUTHENTICATION	"ATT_INSUFF_AUTHENTICATION"
#define ATT_INSUFF_AUTHORIZATION	"ATT_INSUFF_AUTHORIZATION"
#define ATT_INSUFF_ENCRYPTION		"ATT_INSUFF_ENCRYPTION"
#define ATT_INSUFF_RESOURCES		"ATT_INSUFF_RESOURCES"
#define ATT_INVALID_PDU		"ATT_INVALID_PDU"
#define ATT_REQ_NOT_SUPP		"ATT_REQ_NOT_SUPP"
#define ATT_INVALID_OFFSET		"ATT_INVALID_OFFSET"
#define ATT_PREP_QUEUE_FULL	"ATT_PREP_QUEUE_FULL"
#define ATT_ATTR_NOT_LONG		"ATT_ATTR_NOT_LONG"
#define ATT_INSUFF_ENCR_KEY_SIZE	"ATT_INSUFF_ENCR_KEY_SIZE"
#define ATT_INVAL_ATTR_VALUE_LEN	"ATT_INVAL_ATTR_VALUE_LEN"
#define ATT_UNLIKELY		"ATT_UNLIKELY"
#define ATT_UNSUPP_GRP_TYPE	"ATT_UNSUPP_GRP_TYPE"

int attrib_server_init(void);
void attrib_server_exit(void);
void attrib_server_dbus_enable(void);
int attrib_server_reg_adapter(void *adapter);
void attrib_server_unreg_adapter(void *adapter);

uint16_t attrib_db_find_end(void);
uint16_t attrib_db_find_avail(uint16_t nitems);
struct attribute *attrib_db_add(uint16_t handle, bt_uuid_t *uuid, int read_reqs,
				int write_reqs, const uint8_t *value, int len);
int attrib_db_update(uint16_t handle, bt_uuid_t *uuid, const uint8_t *value,
					int len, struct attribute **attr);
int attrib_db_del(uint16_t handle);
int attrib_gap_set(uint16_t uuid, const uint8_t *value, int len);
uint32_t attrib_create_sdp(uint16_t handle, const char *name);
void attrib_free_sdp(uint32_t sdp_handle);
