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
#ifndef __AICE_USB_COMMAND_H__
#define __AICE_USB_COMMAND_H__

enum aice_usb_cmmd_index {
	AICE_CMDIDX_WRITE_CTRL = 0,
	AICE_CMDIDX_WRITE_MISC,
	AICE_CMDIDX_WRITE_EDMSR,
	AICE_CMDIDX_WRITE_DTR,
	AICE_CMDIDX_WRITE_DTR_FROM_BUFFER,
	AICE_CMDIDX_WRITE_DIM,
	AICE_CMDIDX_EXECUTE,
	AICE_CMDIDX_WRITE_MEM,
	AICE_CMDIDX_WRITE_MEM_H,
	AICE_CMDIDX_WRITE_MEM_B,
	AICE_CMDIDX_FASTWRITE_MEM,
	AICE_CMDIDX_BATCH_BUFFER_WRITE,
	AICE_CMDIDX_WRITE_PINS,

	AICE_CMDIDX_SCAN_CHAIN,
	AICE_CMDIDX_READ_CTRL,
	AICE_CMDIDX_READ_MISC,
	AICE_CMDIDX_READ_EDMSR,
	AICE_CMDIDX_READ_DTR,
	AICE_CMDIDX_READ_DTR_TO_BUFFER,
	AICE_CMDIDX_READ_MEM,
	AICE_CMDIDX_READ_MEM_H,
	AICE_CMDIDX_READ_MEM_B,
	AICE_CMDIDX_FASTREAD_MEM,
	AICE_CMDIDX_BATCH_BUFFER_READ,
	//AICE_CMDIDX_SELECT_TARGET,
};

enum aice_command_mode {
	AICE_COMMAND_MODE_NORMAL = 0x00,
	AICE_COMMAND_MODE_PACK,
	AICE_COMMAND_MODE_BATCH,
};

/* Constants for AICE command READ_CTRL */
#define AICE_READ_CTRL_INTF_FREQ            (0x00)
#define AICE_READ_CTRL_HARDWARE_VERSION     (0x01)
#define AICE_READ_CTRL_FPGA_VERSION         (0x02)
#define AICE_READ_CTRL_FIRMWARE_VERSION     (0x03)
#define AICE_READ_CTRL_ICE_CONFIG           (0x04)

#define AICE_READ_CTRL_GET_ICE_STATE         0x00
#define AICE_READ_CTRL_GET_HARDWARE_VERSION  0x01
#define AICE_READ_CTRL_GET_FPGA_VERSION      0x02
#define AICE_READ_CTRL_GET_FIRMWARE_VERSION  0x03
#define AICE_READ_CTRL_GET_JTAG_PIN_STATUS   0x04
#define AICE_READ_CTRL_BATCH_BUF_INFO        0x22
#define AICE_READ_CTRL_BATCH_STATUS          0x23
#define AICE_READ_CTRL_BATCH_BUF0_STATE      0x31
#define AICE_READ_CTRL_BATCH_BUF4_STATE      0x39
#define AICE_READ_CTRL_BATCH_BUF5_STATE      0x3B

/* Constants for AICE command WRITE_CTRL */
#define AICE_WRITE_CTRL_TCK_CONTROL          0x00
#define AICE_WRITE_CTRL_JTAG_PIN_CONTROL     0x01
#define AICE_WRITE_CTRL_CLEAR_TIMEOUT_STATUS 0x02
#define AICE_WRITE_CTRL_RESERVED             0x03
#define AICE_WRITE_CTRL_JTAG_PIN_STATUS      0x04
#define AICE_WRITE_CTRL_TIMEOUT              0x07
#define AICE_WRITE_CTRL_CUSTOM_DELAY         0x0D
#define AICE_WRITE_CTRL_BATCH_CTRL           0x20
#define AICE_WRITE_CTRL_BATCH_ITERATION      0x21
#define AICE_WRITE_CTRL_BATCH_DIM_SIZE       0x22
#define AICE_WRITE_CTRL_BATCH_CMD_BUF0_CTRL  0x30
#define AICE_WRITE_CTRL_BATCH_DATA_BUF0_CTRL 0x38
#define AICE_WRITE_CTRL_BATCH_DATA_BUF1_CTRL 0x3A
#define AICE_REG_BATCH_DATA_BUFFER_1_STATE   (AICE_READ_CTRL_BATCH_BUF5_STATE)

#define AICE_BATCH_COMMAND_BUFFER_0  0x00
#define AICE_BATCH_COMMAND_BUFFER_1  0x01
#define AICE_BATCH_COMMAND_BUFFER_2  0x02
#define AICE_BATCH_COMMAND_BUFFER_3  0x03
#define AICE_BATCH_DATA_BUFFER_0     0x04
#define AICE_BATCH_DATA_BUFFER_1     0x05
#define AICE_BATCH_DATA_BUFFER_2     0x06
#define AICE_BATCH_DATA_BUFFER_3     0x07

extern unsigned char usb_out_packets_buffer[];
extern unsigned char usb_in_packets_buffer[];
extern unsigned int usb_out_packets_buffer_length;
extern unsigned int usb_in_packets_buffer_length;
extern enum aice_command_mode aice_command_mode;
extern unsigned int aice_usb_rx_max_packet;
extern unsigned int aice_usb_tx_max_packet;

extern int aice_usb_open(unsigned int usb_vid, unsigned int usb_pid);
extern int aice_usb_close(void);
extern int aice_usb_write(unsigned char *out_buffer, unsigned int out_length);
extern int aice_usb_read(unsigned char *in_buffer, unsigned int expected_size);
extern int aice_reset_box(void);
extern int aice_scan_chain(unsigned int *id_codes, unsigned char *num_of_ids);
extern int aice_access_cmmd(unsigned char cmdidx, unsigned char target_id, unsigned int address, unsigned char *pdata, unsigned int length);
extern int aice_write_ctrl(unsigned int address, unsigned int WriteData);
extern int aice_read_ctrl(unsigned int address, unsigned int *pReadData);
extern int aice_read_misc(unsigned char target_id, unsigned int address, unsigned int *pReadData);
extern int aice_write_misc(unsigned char target_id, unsigned int address, unsigned int WriteData);
extern int aice_read_edmsr(unsigned char target_id, unsigned int address, unsigned int *pReadData);
extern int aice_write_edmsr(unsigned char target_id, unsigned int address, unsigned int WriteData);
extern int aice_write_mem_b(unsigned char target_id, unsigned int address, unsigned int WriteData);
extern int aice_write_mem_h(unsigned char target_id, unsigned int address, unsigned int WriteData);
extern int aice_write_mem(unsigned char target_id, unsigned int address, unsigned int WriteData);
extern int aice_fastread_mem(unsigned char target_id, unsigned char *pReadData, unsigned int num_of_words);
extern int aice_fastwrite_mem(unsigned char target_id, const unsigned char *pWriteData, unsigned int num_of_words);
extern int aice_read_mem_b(unsigned char target_id, unsigned int address, unsigned int *pReadData);
extern int aice_read_mem_h(unsigned char target_id, unsigned int address, unsigned int *pReadData);
extern int aice_read_mem(unsigned char target_id, unsigned int address, unsigned int *pReadData);
extern int aice_write_dim(unsigned char target_id, unsigned int *word, unsigned char num_of_words);
extern int aice_do_execute(unsigned char target_id);
extern int aice_read_dtr(unsigned char target_id, unsigned int *pReadData);
extern int aice_read_dtr_to_buffer(unsigned char target_id, unsigned int buffer_idx);
extern int aice_write_dtr(unsigned char target_id, unsigned int WriteData);
extern int aice_write_dtr_from_buffer(unsigned char target_id, unsigned int buffer_idx);

extern int aice_batch_buffer_write(unsigned int buf_index, const unsigned char *pWriteData, unsigned int num_of_words);
extern int aice_batch_buffer_read(unsigned char buf_index, unsigned int *pReadData, unsigned int num_of_words);
extern int aice_write_pins(unsigned int num_of_words, unsigned int *pWriteData);
extern int aice_usb_packet_flush(void);
extern int aice_usb_set_command_mode(enum aice_command_mode command_mode);

extern struct aice_usb_cmmd_info usb_cmmd_pack_info;

#endif
