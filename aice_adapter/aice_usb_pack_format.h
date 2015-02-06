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
#ifndef __AICE_USB_PACK_FORMAT_H__
#define __AICE_USB_PACK_FORMAT_H__

enum aice_usb_cmmd_type {
	AICE_CMDTYPE_HTDA = 0,
	AICE_CMDTYPE_HTDB,
	AICE_CMDTYPE_HTDC,
	AICE_CMDTYPE_HTDD,
	AICE_CMDTYPE_HTDMA,
	AICE_CMDTYPE_HTDMB,
	AICE_CMDTYPE_HTDMC,
	AICE_CMDTYPE_HTDMD,
	AICE_CMDTYPE_DTHA,
	AICE_CMDTYPE_DTHB,
	AICE_CMDTYPE_DTHMA,
	AICE_CMDTYPE_DTHMB,
};

/* Constants for AICE command format length */
#define AICE_CMDSIZE_HTDA   3
#define AICE_CMDSIZE_HTDB   6
#define AICE_CMDSIZE_HTDC   7
#define AICE_CMDSIZE_HTDD   10
#define AICE_CMDSIZE_HTDMA  4
#define AICE_CMDSIZE_HTDMB  8
#define AICE_CMDSIZE_HTDMC  8
#define AICE_CMDSIZE_HTDMD  12
#define AICE_CMDSIZE_DTHA   6
#define AICE_CMDSIZE_DTHB   2
#define AICE_CMDSIZE_DTHMA  8
#define AICE_CMDSIZE_DTHMB  4

struct aice_usb_cmmd_info {
	unsigned char *pusb_buffer;
	unsigned char cmdtype;
	unsigned char access_little_endian;
	unsigned char cmd;
	unsigned char target;
	unsigned char length;
	unsigned int addr;
	unsigned char *pword_data;
};

extern void aice_pack_usb_cmmd(struct aice_usb_cmmd_info *pusb_cmmd_info);
extern unsigned int aice_get_usb_cmmd_size(unsigned int usb_cmmd_type);

#endif
