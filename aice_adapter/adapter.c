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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "adapter.h"
#include "aice_usb_pack_format.h"
#include "aice_usb_command.h"
#include "aice_pipe_command.h"
#include "aice_log.h"
#include "aice_jdp.h"

#define MAXLINE 8192

#ifdef __MINGW32__
    #include <windows.h>
#endif // __MINGW32__


static uint32_t jtag_clock;
static unsigned int aice_clk_setting = 16;
extern int usb_command_directly;   /// set libopenocd directly use aice_access_cmmd
unsigned int aice_num_of_target_id_codes = 0;

/********************************************************************************************/
static int pipe_read(void *buffer, int length)
{
#ifdef __MINGW32__
	BOOL success;
	DWORD has_read;

	success = ReadFile (GetStdHandle(STD_INPUT_HANDLE), buffer, length, &has_read, NULL);
	if (!success)
	{
		aice_log_add (AICE_LOG_ERROR, "read pipe failed\n");
		return -1;
	}

	return has_read;
#else
	return read (STDIN_FILENO, buffer, length);
#endif
}

static int pipe_write(const void *buffer, int length)
{
#ifdef __MINGW32__
	BOOL success;
	DWORD written;

	success = WriteFile (GetStdHandle(STD_OUTPUT_HANDLE), buffer, length, &written, NULL);
	if (!success)
	{
		aice_log_add (AICE_LOG_ERROR, "write pipe failed\n");
		return -1;
	}

	return written;
#else
	return write (STDOUT_FILENO, buffer, length);
#endif
}
/********************************************************************************************/

///Modified from aice_usb.c:aice_get_info()
static int aice_get_version_info()
{
    uint32_t hardware_version;
    uint32_t firmware_version;
    uint32_t fpga_version;
    uint32_t ice_config;
    uint32_t batch_data_buf1_size;


	//version info
	if (aice_read_ctrl(AICE_READ_CTRL_GET_HARDWARE_VERSION, &hardware_version) != ERROR_OK)
		return ERROR_FAIL;

	if (aice_read_ctrl(AICE_READ_CTRL_GET_FIRMWARE_VERSION, &firmware_version) != ERROR_OK)
		return ERROR_FAIL;

	if (aice_read_ctrl(AICE_READ_CTRL_GET_FPGA_VERSION, &fpga_version) != ERROR_OK)
		return ERROR_FAIL;

	aice_log_add(AICE_LOG_DEBUG, "\t AICE version: hw_ver = 0x%x, fw_ver = 0x%x, fpga_ver = 0x%x\n",
		hardware_version, firmware_version, fpga_version);

	//hardware features info
	if (aice_read_ctrl(AICE_READ_CTRL_ICE_CONFIG, &ice_config) != ERROR_OK)
		return ERROR_FAIL;
	aice_log_add(AICE_LOG_DEBUG,"\t AICE hardware features: 0x%08x\n", ice_config);

	//batch buffer size info
	if (aice_read_ctrl(AICE_REG_BATCH_DATA_BUFFER_1_STATE, &batch_data_buf1_size) != ERROR_OK)
		return ERROR_FAIL;
	batch_data_buf1_size &= 0xffffu;
	aice_log_add(AICE_LOG_DEBUG,"\t AICE batch data buffer size: %d words\n", batch_data_buf1_size);

	return ERROR_OK;
}

static int aice_usb_set_clock(int set_clock)
{
	aice_write_ctrl(AICE_WRITE_CTRL_TIMEOUT, CHK_DBGER_TIMEOUT);

	if (aice_write_ctrl(AICE_WRITE_CTRL_TCK_CONTROL,
				AICE_TCK_CONTROL_TCK_SCAN) != ERROR_OK)
		goto aice_usb_set_clock_ERR;

	/* Read out TCK_SCAN clock value */
	uint32_t scan_clock;
	if (aice_read_ctrl(AICE_READ_CTRL_GET_ICE_STATE, &scan_clock) != ERROR_OK)
		goto aice_usb_set_clock_ERR;

	scan_clock &= 0x0F;

	uint32_t scan_base_freq;
	if (scan_clock & 0x8)
		scan_base_freq = 48000; /* 48 MHz */
	else
		scan_base_freq = 30000; /* 30 MHz */

	uint32_t set_base_freq;
	if (set_clock == 16) {
		// Users do NOT specify the jtag clock, use scan-freq
		aice_log_add(AICE_LOG_DEBUG, "Use scan-freq");
		set_clock = scan_clock;
		// If scan-freq = 48MHz, use 24MHz by default
		if (set_clock == 8)
			set_clock = 9;
		set_base_freq = scan_base_freq;
	}
	else if (set_clock & 0x8)
		set_base_freq = 48000;
	else
		set_base_freq = 30000;

	uint32_t set_freq;
	uint32_t scan_freq;
	set_freq = set_base_freq >> (set_clock & 0x7);
	scan_freq = scan_base_freq >> (scan_clock & 0x7);

	if (scan_freq < set_freq) {
		aice_log_add(AICE_LOG_ERROR, "User specifies higher jtag clock than TCK_SCAN clock");
	}

	if (aice_write_ctrl(AICE_WRITE_CTRL_TCK_CONTROL, set_clock) != ERROR_OK)
		goto aice_usb_set_clock_ERR;

	uint32_t check_speed;
	if (aice_read_ctrl(AICE_READ_CTRL_GET_ICE_STATE, &check_speed) != ERROR_OK)
		goto aice_usb_set_clock_ERR;

	if (((int)check_speed & 0x0F) != set_clock) {
		aice_log_add(AICE_LOG_ERROR, "Set jtag clock failed");
		goto aice_usb_set_clock_ERR;
	}

	aice_clk_setting = set_clock;
	return ERROR_OK;

aice_usb_set_clock_ERR:
	aice_clk_setting = set_clock | 0x80;
	return ERROR_FAIL;
}

/********************************************************************************************/
/// Modified from aice_usb.c:aice_open_device()
static void aice_open (const char *input)
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_open>: \n");

	char response[MAXLINE];
	uint16_t vid;
	uint16_t pid;

	vid = get_u16 (input + 1);
	pid = get_u16 (input + 3);

	aice_log_add (AICE_LOG_DEBUG, "\t VID: 0x%04x, PID: 0x%04x\n", vid, pid);

	if (ERROR_OK != aice_usb_open(vid, pid)) {
        aice_log_add(AICE_LOG_ERROR, "\t <-- Can not open usb -->\n");
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

    if(ERROR_OK != aice_reset_box()) {
        aice_log_add(AICE_LOG_ERROR, "\t Cannot initial AICE box!\n");
        response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
    }

	if (ERROR_FAIL == aice_get_version_info()) {
        aice_log_add(AICE_LOG_ERROR, "\t Cannot get AICE info!\n");
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

	response[0] = AICE_OK;
	pipe_write (response, 1);
}


static void aice_set_jtag_clock (const char *input)
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_set_jtag_clock>: \n");

	char response[MAXLINE];

	jtag_clock = get_u32 (input + 1);

	aice_log_add (AICE_LOG_DEBUG, "\t CLOCK: %x \n", jtag_clock);

	if (ERROR_OK != aice_usb_set_clock (jtag_clock)) {
		response[0] = AICE_ERROR;
        pipe_write (response, 1);
        aice_log_add(AICE_LOG_ERROR, "\t Failed to set jtag clock!! \n");
        return;
	}
	else {
        response[0] = AICE_OK;
        set_u32(response+1, aice_clk_setting);
        pipe_write (response, 5);
        return;
	}
}

/// Modified from aice_usb.c:aice_close_device()
static void aice_close (const char *input)
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_close> \n");

	char response[MAXLINE];

	aice_usb_close ();

	response[0] = AICE_OK;
	pipe_write (response, 1);

	exit (0);
}

/// Modified from aice_usb.c:aice_usb_reset()
static void aice_reset (const char *input)
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_reset> \n");

	char response[MAXLINE];

	if ( ERROR_OK != aice_reset_box() ) {
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

    if( ERROR_OK != aice_usb_set_clock(jtag_clock) ) {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
    }

    response[0] = AICE_OK;
	pipe_write (response, 1);
}

static void aice_idcode (const char *input)
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_idcode>:\n");

	char response[MAXLINE];
	uint8_t num_of_idcode;
	uint32_t idcode[MAX_ID_CODE];
	uint8_t i;
    int retval;

    retval = aice_scan_chain(idcode, &num_of_idcode);
    if( (retval!=ERROR_OK) || (num_of_idcode==0xFF) ) {
        aice_log_add (AICE_LOG_ERROR, "\t No target connected\n");
        response[0] = 0xFF;
        pipe_write (response, 1);
        return;
    }
    else if( num_of_idcode >= MAX_ID_CODE ) {
        aice_log_add (AICE_LOG_ERROR, "\t The ice chain over 16 targets\n");
        response[0] = 0;
        pipe_write (response, 1);
        return;
    }

    num_of_idcode++;
	response[0] = num_of_idcode;

	aice_log_add (AICE_LOG_DEBUG, "\t # of target: %d\n", num_of_idcode);
	for (i = 0 ; i < num_of_idcode ; i++)
	{
		set_u32 (response + 1 + i*4, idcode[i]);
		aice_log_add (AICE_LOG_DEBUG, "\t\t IDCode%d: 0x%08X\n", i, idcode[i]);
	}

	pipe_write (response, 1 + num_of_idcode * 4);
}

static void aice_state (const char *input)
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_state>: \n");


///Un-support now
//    char response[MAXLINE];
//	enum aice_target_state_s state = aice_usb_state ();
//	response[0] = state;
//
//	pipe_write (response, 1);
}

static int aice_read_edm (const char *input)
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_read_edm>: \n");

	char response[MAXLINE];
    uint8_t JDPInst;
    uint32_t target_id;
    uint32_t address;
    uint32_t num_of_words;
    uint32_t *EDMData;
    int result = 0;
    unsigned int value_edmsw = 0;

    JDPInst      = input[1];
    target_id    = get_u32( input+2 );
    address      = get_u32( input+6 );
    num_of_words = get_u32( input+10 );

    aice_log_add (AICE_LOG_DEBUG, "\t target_id=0x%08X, cmd=0x%02X, addr=0x%08X, len=0x%08X \n", \
                                      target_id, JDPInst, address, num_of_words);


    EDMData = malloc( sizeof(uint32_t) * (num_of_words+1) );
    if( EDMData == NULL ) {
        response[0] = AICE_ERROR;
		pipe_write (response, 1);

		aice_log_add (AICE_LOG_INFO, "\t Allocate EDMData buffer Failed!!\n");
		return ERROR_FAIL;
    }


    switch ( JDPInst ) {
        case JDP_R_DBG_SR:
            result = aice_access_cmmd(AICE_CMDIDX_READ_EDMSR, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_R_DTR:
           	value_edmsw = 0;
            if (aice_access_cmmd(AICE_CMDIDX_READ_EDMSR, target_id, NDS_EDM_SR_EDMSW, (unsigned char *)&value_edmsw, 1) != ERROR_OK) {
                result = ERROR_FAIL;
                break;
            }
            if ((value_edmsw & NDS_EDMSW_WDV) == 0) {
                result = ERROR_FAIL;
                break;
            }
            result = aice_access_cmmd(AICE_CMDIDX_READ_DTR, target_id, 0, (unsigned char *)EDMData, 1);
            break;

        case JDP_R_MEM_W:
            address = ((address >> 2) & 0x3FFFFFFF);
            result = aice_access_cmmd(AICE_CMDIDX_READ_MEM, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_R_MISC_REG:
            result = aice_access_cmmd(AICE_CMDIDX_READ_MISC, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_R_FAST_MEM:
            result = aice_access_cmmd(AICE_CMDIDX_FASTREAD_MEM, target_id, 0, (unsigned char *)EDMData, num_of_words);
            break;

        case JDP_R_MEM_H:
            address = ((address >> 1) & 0x7FFFFFFF);
            result = aice_access_cmmd(AICE_CMDIDX_READ_MEM_H, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_R_MEM_B:
            result = aice_access_cmmd(AICE_CMDIDX_READ_MEM_B, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_R_DIM:
        case JDP_R_DBG_EVENT:
        case JDP_R_IDCODE:
        default:
            aice_log_add (AICE_LOG_INFO, "AICE Read EDM JDPInst Error: inst error, inst code: 0x%02X", JDPInst );
            result = ERROR_FAIL;
            break;
    };


    if( result == ERROR_OK ) {
        response[0] = AICE_OK;

        aice_log_add (AICE_LOG_INFO, "\t recv:\n");

		for (int i = 0 ; i < num_of_words ; i++)
		{
			set_u32 (response + 1 + i*4, EDMData[i]);

			aice_log_add( AICE_LOG_INFO, "\t\t0x%08X \n", EDMData[i]);
		}

        pipe_write (response, 1 + num_of_words * 4);
    }
    else {
        response[0] = AICE_ERROR;
		pipe_write (response, 1);

        aice_log_add (AICE_LOG_INFO, "\t Read EDM Failed!!\n");
    }


    free(EDMData);

    return result;
}


static int aice_write_edm( const char *input )
{
	aice_log_add (AICE_LOG_DEBUG, "<aice_write_edm>: \n");

	char response[MAXLINE];
    uint8_t JDPInst;
    uint32_t target_id;
    uint32_t address;
    uint32_t num_of_words;
    uint32_t *EDMData;
    int result = 0;
    unsigned int value_edmsw = 0;
    unsigned int write_data  = 0;


    JDPInst      = input[1];
    target_id    = get_u32( input+2 );
    address      = get_u32( input+6 );
    num_of_words = get_u32( input+10 );

    aice_log_add (AICE_LOG_DEBUG, "\t target_id=0x%08X, cmd=0x%02X, addr=0x%08X, len=0x%08X \n", \
                                      target_id, JDPInst, address, num_of_words);

    EDMData = malloc( sizeof(uint32_t) * (num_of_words+1) );
    if( EDMData == NULL ) {
        response[0] = AICE_ERROR;
		pipe_write (response, 1);

		aice_log_add (AICE_LOG_ERROR, "\t Allocate EDMData buffer Failed!!\n");
		return ERROR_FAIL;
    }

    aice_log_add (AICE_LOG_DEBUG, "\t data:\n");
    for (int i = 0 ; i < num_of_words ; i++)
    {
		EDMData[i] = get_u32( input + 14 + i*4 );

		aice_log_add( AICE_LOG_INFO, "\t\t0x%08X \n", EDMData[i]);
	}


    switch ( JDPInst ) {
        case JDP_W_DIM:
            usb_cmmd_pack_info.access_little_endian = 0;  // AICE_BIG_ENDIAN

            result =  aice_access_cmmd(AICE_CMDIDX_WRITE_DIM, target_id, 0, (unsigned char *)EDMData, num_of_words);
            usb_cmmd_pack_info.access_little_endian = 1;
            break;


        case JDP_W_DBG_SR:
            result = aice_access_cmmd(AICE_CMDIDX_WRITE_EDMSR, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_W_DTR:
            result = aice_access_cmmd(AICE_CMDIDX_WRITE_DTR, target_id, 0, (unsigned char *)EDMData, 1);
            if (result != ERROR_OK) {
                result = ERROR_FAIL;
                break;
            }

            if (aice_access_cmmd(AICE_CMDIDX_READ_EDMSR, target_id, NDS_EDM_SR_EDMSW, (unsigned char *)&value_edmsw, 1) != ERROR_OK) {
                result = ERROR_FAIL;
                break;
            }

            if ((value_edmsw & NDS_EDMSW_RDV) == 0) {
                aice_log_add( AICE_LOG_INFO, "<-- TARGET ERROR! AICE failed to write to the DTR register. -->");
                result = ERROR_FAIL;
                break;
            }
            result = ERROR_OK;
            break;

        case JDP_W_MEM_W:
            address = ((address >> 2) & 0x3FFFFFFF);
	        result = aice_access_cmmd(AICE_CMDIDX_WRITE_MEM, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_W_MISC_REG:
            result = aice_access_cmmd(AICE_CMDIDX_WRITE_MISC, target_id, address, (unsigned char *)EDMData, 1);
            break;

        case JDP_W_FAST_MEM:
            result = aice_access_cmmd(AICE_CMDIDX_FASTWRITE_MEM, target_id, 0, (unsigned char *)EDMData, num_of_words);
            break;

        case JDP_W_EXECUTE:
            write_data = 0;
            result = aice_access_cmmd(AICE_CMDIDX_EXECUTE, target_id, 0, (unsigned char *)&write_data, 1);
            break;

        case JDP_W_MEM_H:
            write_data = (EDMData[0] & 0x0000FFFF);
            address = ((address >> 1) & 0x7FFFFFFF);
            result = aice_access_cmmd(AICE_CMDIDX_WRITE_MEM_H, target_id, address, (unsigned char *)&write_data, 1);
            break;

        case JDP_W_MEM_B:
            write_data = (EDMData[0] & 0x000000FF);
            result = aice_access_cmmd(AICE_CMDIDX_WRITE_MEM_B, target_id, address, (unsigned char *)&write_data, 1);
            break;

        default:
            aice_log_add (AICE_LOG_INFO, "AICE Write EDM JDPInst Error: inst error, inst code: 0x%02X", JDPInst );
            result = ERROR_FAIL;
            break;
    };


    if( result == ERROR_OK ) {
        response[0] = AICE_OK;
        pipe_write (response, 1);
    }
    else {
        response[0] = AICE_ERROR;
		pipe_write (response, 1);

        aice_log_add (AICE_LOG_ERROR, "\t Read EDM Failed!!\n");
    }

    return result;
}

static int aice_write_ctrls( const char *input )
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_write_ctrl>: \n");
    
    char response[MAXLINE];
    unsigned int address;
    unsigned int WriteData;
	int result;
    
    address   = get_u32 (input + 1);
	WriteData = get_u32 (input + 5);
    
    aice_log_add (AICE_LOG_DEBUG, "\t addr=0x%08X, data=0x%08X \n", address, WriteData);
    result = aice_access_cmmd(AICE_CMDIDX_WRITE_CTRL, 0, address, (unsigned char *)&WriteData, 1);
    
    if( result == ERROR_OK )
        response[0] = AICE_OK;
    else {
        aice_log_add (AICE_LOG_ERROR, "\t Write ICE box ctrl Failed!!\n");
        response[0] = AICE_ERROR;
    }
    pipe_write (response, 1);
    
    return result;
}

static int aice_read_ctrls( const char *input )
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_read_ctrl>: \n");
    
    char response[MAXLINE];
    unsigned int address;
    unsigned int pReadData;
	int result;
    
    address   = get_u32 (input + 1);

    aice_log_add (AICE_LOG_DEBUG, "\t addr=0x%08X \n", address);

	result = aice_access_cmmd(AICE_CMDIDX_READ_CTRL, 0, address, (unsigned char *)&pReadData, 1);

    if( result == ERROR_OK ) {
        aice_log_add (AICE_LOG_DEBUG, "\t recv: 0x%08X\n", pReadData);
        
        response[0] = AICE_OK;
        set_u32(response+1, pReadData);
        pipe_write (response, 5);
    }
    else {
        aice_log_add (AICE_LOG_ERROR, "\t Read ICE box ctrl Failed!!\n");
        
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
    }


    return result;
}


int main ()
{
	char line[MAXLINE];
	int n;

	signal(SIGINT, SIG_IGN);
	atexit (aice_log_finalize);

	aice_log_init (512*1024, AICE_LOG_DEBUG, true);
	usb_command_directly = 1;

	while ((n = pipe_read (line, MAXLINE)) > 0)
	{
		switch (line[0])
		{
			case AICE_OPEN:
				aice_open (line);
				break;
			case AICE_CLOSE:
				aice_close (line);
				break;
			case AICE_RESET:
				aice_reset (line);
				break;
			case AICE_IDCODE:
				aice_idcode (line);
				break;
			case AICE_SET_JTAG_CLOCK:
				aice_set_jtag_clock (line);
				break;
            case AICE_READ_EDM:
                aice_read_edm(line);
                break;
            case AICE_WRITE_EDM:
                aice_write_edm(line);
                break;
                
            case AICE_WRITE_CTRL:
                aice_write_ctrls(line);
                break;
            
            case AICE_READ_CTRL:
                aice_read_ctrls(line);
                break;
            
            case AICE_CUSTOM_SCRIPT:
            case AICE_DIAGNOSTIC:
            case AICE_GET_ICE_STATE:
				//aice_state (line);
				//break;
            case AICE_CUSTOM_MONITOR_CMD:
            default:
                aice_log_add (AICE_LOG_INFO, "Error command: %c\n", line[0] );
                break;
		}
	}

	return 0;
}
