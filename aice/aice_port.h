/***************************************************************************
 *   Copyright (C) 2013 by Andes Technology                                *
 *   Hsiangkai Wang <hkwang@andestech.com>                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/
#ifndef _AICE_PORT_H_
#define _AICE_PORT_H_

#include "config.h"
#include "nds32_edm.h"

#define ERROR_OK 		(0)
#define ERROR_FAIL 		(-1)
#define ERROR_DISCONNECT	(-2)
#define ERROR_AICE_TIMEOUT	(-3)

#define AICE_MAX_NUM_CORE      (0x10)

#define ERROR_AICE_DISCONNECT  (-200)
#define MAX_ID_CODE 	(16)

enum aice_target_state_s {
	AICE_DISCONNECT = 0,
	AICE_TARGET_DETACH,
	AICE_TARGET_UNKNOWN,
	AICE_TARGET_RUNNING,
	AICE_TARGET_HALTED,
	AICE_TARGET_RESET,
	AICE_TARGET_DEBUG_RUNNING,
};

enum aice_srst_type_s {
	AICE_SRST = 0x1,
	AICE_RESET_HOLD = 0x8,
};

enum aice_target_endian {
	AICE_LITTLE_ENDIAN = 0,
	AICE_BIG_ENDIAN,
};

enum aice_api_s {
	AICE_OPEN = 0x0,
	AICE_CLOSE,
	AICE_RESET,
	AICE_IDCODE,
	AICE_SET_JTAG_CLOCK,
	AICE_ASSERT_SRST,
	AICE_RUN,
	AICE_HALT,
	AICE_STEP,
	AICE_READ_REG,
	AICE_WRITE_REG,
	AICE_READ_REG_64,
	AICE_WRITE_REG_64,
	AICE_READ_MEM_UNIT,
	AICE_WRITE_MEM_UNIT,
	AICE_READ_MEM_BULK,
	AICE_WRITE_MEM_BULK,
	AICE_READ_DEBUG_REG,
	AICE_WRITE_DEBUG_REG,
	AICE_STATE,
	AICE_MEMORY_ACCESS,
	AICE_MEMORY_MODE,
	AICE_READ_TLB,
	AICE_CACHE_CTL,
	AICE_SET_RETRY_TIMES,
	AICE_PROGRAM_EDM,
	AICE_SET_COMMAND_MODE,
	AICE_EXECUTE,
	AICE_SET_CUSTOM_SRST_SCRIPT,
	AICE_SET_CUSTOM_TRST_SCRIPT,
	AICE_SET_CUSTOM_RESTART_SCRIPT,
	AICE_SET_COUNT_TO_CHECK_DBGER,
	AICE_SET_DATA_ENDIAN,
};

enum aice_error_s {
	AICE_OK,
	AICE_ACK,
	AICE_ERROR,
};

enum aice_cache_ctl_type {
	AICE_CACHE_CTL_L1D_INVALALL = 0,
	AICE_CACHE_CTL_L1D_VA_INVAL,
	AICE_CACHE_CTL_L1D_WBALL,
	AICE_CACHE_CTL_L1D_VA_WB,
	AICE_CACHE_CTL_L1I_INVALALL,
	AICE_CACHE_CTL_L1I_VA_INVAL,
	AICE_CACHE_CTL_LOOPCACHE_ISYNC,
};

enum aice_command_mode {
	AICE_COMMAND_MODE_NORMAL,
	AICE_COMMAND_MODE_PACK,
	AICE_COMMAND_MODE_BATCH,
};

struct aice_port_param_s {
	/** */
	char *device_desc;
	/** */
	char *serial;
	/** */
	uint16_t vid;
	/** */
	uint16_t pid;
	/** */
	char *adapter_name;
};

struct aice_port_s {
	/** */
	uint32_t coreid;
	/** */
	const struct aice_port *port;
};

enum aice_memory_access {
	AICE_MEMORY_ACC_BUS = 0,
	AICE_MEMORY_ACC_CPU,
};

enum aice_memory_mode {
	AICE_MEMORY_MODE_AUTO = 0,
	AICE_MEMORY_MODE_MEM = 1,
	AICE_MEMORY_MODE_ILM = 2,
	AICE_MEMORY_MODE_DLM = 3,
};

static inline void set_u32(void *_buffer, uint32_t value)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	buffer[3] = (value >> 24) & 0xff;
	buffer[2] = (value >> 16) & 0xff;
	buffer[1] = (value >> 8) & 0xff;
	buffer[0] = (value >> 0) & 0xff;
}

static inline uint32_t get_u32(const void *_buffer)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	return (((uint32_t)buffer[3]) << 24) |
		(((uint32_t)buffer[2]) << 16) |
		(((uint32_t)buffer[1]) << 8) |
		(((uint32_t)buffer[0]) << 0);
}

static inline void set_u16(void *_buffer, uint16_t value)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	buffer[1] = (value >> 8) & 0xff;
	buffer[0] = (value >> 0) & 0xff;
}

static inline uint16_t get_u16(const void *_buffer)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	return (((uint16_t)buffer[1]) << 8) |
		(((uint16_t)buffer[0]) << 0);
}

#endif
