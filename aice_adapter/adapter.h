/***************************************************************************
 *   Copyright (C) 2013 by Andes Technology                                *
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
#ifndef __ADAPTER_H__
#define __ADAPTER_H__

#define ERROR_OK            (0)
#define ERROR_FAIL          (-4)
#define ERROR_AICE_DISCONNECT  (-200)
#define ERROR_AICE_TIMEOUT     (-201)


/* Constants for AICE command WRITE_CTRL:JTAG_PIN_CONTROL, original from aice_usb.h */
#define AICE_JTAG_PIN_CONTROL_SRST      0x01
#define AICE_JTAG_PIN_CONTROL_TRST      0x02
#define AICE_JTAG_PIN_CONTROL_STOP      0x04
#define AICE_JTAG_PIN_CONTROL_RESTART   0x08

/* Constants for AICE command WRITE_CTRL:TCK_CONTROL */
#define AICE_TCK_CONTROL_TCK_SCAN       0x10

#define NDS_EDM_SR_EDMSW    0x30
#define NDS_EDMSW_WDV       (1 << 0)
#define NDS_EDMSW_RDV       (1 << 1)

#define NDS_DBGER_DEX       (0x1)
#define NDS_DBGER_DPED      (0x2)
#define NDS_DBGER_CRST      (0x4)
#define NDS_DBGER_AT_MAX    (0x8)
#define NDS_DBGER_ILL_SEC_ACC   (0x10)

#define CHK_DBGER_TIMEOUT   5000
#define MAX_ID_CODE         (0x10)
enum aice_error_s {
    AICE_OK,
    AICE_ACK,
    AICE_ERROR,
};

enum aice_target_state_s {
    AICE_DISCONNECT = 0,
    AICE_TARGET_DETACH,
    AICE_TARGET_UNKNOWN,
    AICE_TARGET_RUNNING,
    AICE_TARGET_HALTED,
    AICE_TARGET_RESET,
    AICE_TARGET_DEBUG_RUNNING,
};

/* EDM misc registers */
enum nds_edm_misc_reg {
    NDS_EDM_MISC_DIMIR = 0x0,
    NDS_EDM_MISC_SBAR,
    NDS_EDM_MISC_EDM_CMDR,
    NDS_EDM_MISC_DBGER,
    NDS_EDM_MISC_ACC_CTL,
    NDS_EDM_MISC_EDM_PROBE,
    NDS_EDM_MISC_GEN_PORT0,
    NDS_EDM_MISC_GEN_PORT1,
};

/*****************************************************************************/
#define set_u32(buffer, value) h_u32_to_le((uint8_t *)buffer, value)
#define set_u16(buffer, value) h_u16_to_le((uint8_t *)buffer, value)
#define get_u32(buffer) le_to_h_u32((const uint8_t *)buffer)
#define get_u16(buffer) le_to_h_u16((const uint8_t *)buffer)

static inline void h_u32_to_le(uint8_t* buf, int val)
{
    buf[3] = (uint8_t) (val >> 24);
    buf[2] = (uint8_t) (val >> 16);
    buf[1] = (uint8_t) (val >> 8);
    buf[0] = (uint8_t) (val >> 0);
}

static inline void h_u16_to_le(uint8_t* buf, int val)
{
    buf[1] = (uint8_t) (val >> 8);
    buf[0] = (uint8_t) (val >> 0);
}

static inline uint32_t le_to_h_u32(const uint8_t* buf)
{
    return (uint32_t)(buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24);
}

static inline uint16_t le_to_h_u16(const uint8_t* buf)
{
    return (uint16_t)(buf[0] | buf[1] << 8);
}

#endif
