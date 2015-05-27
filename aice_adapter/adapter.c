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
#include <jtag/drivers/libusb_common.h>
#include "aice_usb_pack_format.h"
#include "aice_usb_command.h"
#include "aice_pipe_command.h"
#include "aice_log.h"
#include "aice_jdp.h"
#include "adapter.h"


#define MAXLINE 8192

#ifdef __MINGW32__
    #include <windows.h>
#endif // __MINGW32__


#define AICE_VID 0x1CFC
#define AICE_PID 0x0000


static uint32_t jtag_clock;
static unsigned int aice_clk_setting = 16;
unsigned int aice_num_of_target_id_codes = 0;
/***************************************************************************/
#define AICE_MAX_VID_NUM       (0xFF)
struct vid_pid_s {
  	/** */
    uint16_t vid;
	/** */
	uint16_t pid;
};
struct vid_pid_s vid_pid_array[AICE_MAX_VID_NUM];
int vid_pid_array_top = -1;
int nds32_reset_aice_as_startup = 0;
int debug_level   = AICE_LOG_ERROR;
int log_file_size = MINIMUM_DEBUG_LOG_SIZE; 

/***************************************************************************/


/********************************************************************************************/
static int pipe_read(void *buffer, int length)
{
#ifdef __MINGW32__
    BOOL success;
    DWORD has_read;

    success = ReadFile (GetStdHandle(STD_INPUT_HANDLE), buffer, length, &has_read, NULL);
    if (!success)
    {
        aice_log_add (AICE_LOG_ERROR, "read pipe failed");
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
        aice_log_add (AICE_LOG_ERROR, "write pipe failed");
        return -1;
    }

    return written;
#else
    return write (STDOUT_FILENO, buffer, length);
#endif
}
/********************************************************************************************/

/********************************************************************************************/
int aice_write_ctrl(unsigned int address, unsigned int WriteData)
{
    unsigned int ctrl_data = WriteData;
    int result;
    result = aice_access_cmmd(AICE_CMDIDX_WRITE_CTRL, 0, address, (unsigned char *)&ctrl_data, 1);
    return result;
}

int aice_read_ctrl(unsigned int address, unsigned int *pReadData)
{
    int result;
    result = aice_access_cmmd(AICE_CMDIDX_READ_CTRL, 0, address, (unsigned char *)pReadData, 1);
    return result;
}

int aice_read_misc(unsigned char target_id, unsigned int address, unsigned int *pReadData)
{
    return aice_access_cmmd(AICE_CMDIDX_READ_MISC, target_id, address, (unsigned char *)pReadData, 1);
}

int aice_write_misc(unsigned char target_id, unsigned int address, unsigned int WriteData)
{
    unsigned int misc_data = WriteData;
    return aice_access_cmmd(AICE_CMDIDX_WRITE_MISC, target_id, address, (unsigned char *)&misc_data, 1);
}


/********************************************************************************************/
///Modified from aice_usb.c:aice_get_info()
static int aice_get_version_info(struct aice_port_s *aice)
{
    //version info
    if (aice_read_ctrl(AICE_READ_CTRL_GET_HARDWARE_VERSION, &aice->hardware_version) != ERROR_OK)
        return ERROR_FAIL;

    if (aice_read_ctrl(AICE_READ_CTRL_GET_FIRMWARE_VERSION, &aice->firmware_version) != ERROR_OK)
        return ERROR_FAIL;

    if (aice_read_ctrl(AICE_READ_CTRL_GET_FPGA_VERSION, &aice->fpga_version) != ERROR_OK)
        return ERROR_FAIL;

    aice_log_add(AICE_LOG_DEBUG, "\t AICE version: hw_ver = 0x%x, fw_ver = 0x%x, fpga_ver = 0x%x",
        aice->hardware_version, aice->firmware_version, aice->fpga_version);

    //hardware features info
    if (aice_read_ctrl(AICE_READ_CTRL_ICE_CONFIG, &aice->ice_config) != ERROR_OK)
        return ERROR_FAIL;
    aice_log_add(AICE_LOG_DEBUG,"\t AICE hardware features: 0x%08x", aice->ice_config);

    //batch buffer size info
    if (aice_read_ctrl(AICE_REG_BATCH_DATA_BUFFER_1_STATE, &aice->batch_data_buf1_size) != ERROR_OK)
        return ERROR_FAIL;
    aice->batch_data_buf1_size &= 0xffffu;
    aice_log_add(AICE_LOG_DEBUG,"\t AICE batch data buffer size: %d words", aice->batch_data_buf1_size);

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



///Modified from aice_usb.c:aice_usb_execute_custom_script()
#define LINE_BUFFER_SIZE 1024

static enum AICE_CUSTOM_CMMD {
    AICE_CUSTOM_CMMD_SET_SRST = 0,
    AICE_CUSTOM_CMMD_CLEAR_SRST,
    AICE_CUSTOM_CMMD_SET_DBGI,
    AICE_CUSTOM_CMMD_CLEAR_DBGI,
    AICE_CUSTOM_CMMD_SET_TRST,
    AICE_CUSTOM_CMMD_CLEAR_TRST,
    AICE_CUSTOM_CMMD_DELAY,
    AICE_CUSTOM_CMMD_WRITE_PINS,
    AICE_CUSTOM_CMMD_T_WRITE_MISC,
    AICE_CUSTOM_CMMD_MAX,
};

static char *custom_script_cmmd[AICE_CUSTOM_CMMD_MAX]={
    "set srst",
    "clear srst",
    "set dbgi",
    "clear dbgi",
    "set trst",
    "clear trst",
    "delay",
    "write_pins",
    "t_write_misc",
};

static int execute_custom_script(const char *script)
{
    FILE *script_fd;
    uint32_t word_buffer[LINE_BUFFER_SIZE/4];
    char tmp_buffer[LINE_BUFFER_SIZE];
    char *line_buffer = (char *)&word_buffer[0];
    char *curr_str, *compare_str;
    uint32_t delay, i, num_of_words;
    uint32_t Nibble1 = 0, Nibble2 = 0, write_pins_num = 0;
    uint32_t write_ctrl_value = 0, idx = 0;
    uint32_t target_id = 0, write_misc_addr = 0, write_misc_data = 0;
    int result = ERROR_OK;

    script_fd = fopen(script, "r");
    if (script_fd == NULL) {
        return ERROR_FAIL;
    }
    while (fgets(line_buffer, LINE_BUFFER_SIZE, script_fd) != NULL) {
        for (i = 0; i < AICE_CUSTOM_CMMD_MAX; i ++) {
            compare_str = custom_script_cmmd[i];
            curr_str = strstr(line_buffer, compare_str);
            if (curr_str != NULL)
                break;
        }
        if (i < AICE_CUSTOM_CMMD_MAX) {
            aice_log_add( AICE_LOG_DEBUG, "\t custom_script %s, %s, %d, %d", curr_str, compare_str, (int)i, (int)strlen(compare_str));
        }
        if (i <= AICE_CUSTOM_CMMD_DELAY) {
            sscanf(curr_str + strlen(compare_str), " %d", &delay);
            if (i == AICE_CUSTOM_CMMD_SET_SRST)
                write_ctrl_value = AICE_CUSTOM_DELAY_SET_SRST;
            else if (i == AICE_CUSTOM_CMMD_CLEAR_SRST)
                write_ctrl_value = AICE_CUSTOM_DELAY_CLEAN_SRST;
            else if (i == AICE_CUSTOM_CMMD_SET_DBGI)
                write_ctrl_value = AICE_CUSTOM_DELAY_SET_DBGI;
            else if (i == AICE_CUSTOM_CMMD_CLEAR_DBGI)
                write_ctrl_value = AICE_CUSTOM_DELAY_CLEAN_DBGI;
            else if (i == AICE_CUSTOM_CMMD_SET_TRST)
                write_ctrl_value = AICE_CUSTOM_DELAY_SET_TRST;
            else if (i == AICE_CUSTOM_CMMD_CLEAR_TRST)
                write_ctrl_value = AICE_CUSTOM_DELAY_CLEAN_TRST;
            else if (i == AICE_CUSTOM_CMMD_DELAY)
                write_ctrl_value = 0;

            write_ctrl_value |= (delay << 16);
            aice_log_add( AICE_LOG_DEBUG, "\t custom_script aice_write_ctrl, 0x%x", write_ctrl_value);
            result = aice_write_ctrl(AICE_WRITE_CTRL_CUSTOM_DELAY, write_ctrl_value);
            if (result != ERROR_OK)
                goto aice_execute_custom_script_error;
        }
        else if (i == AICE_CUSTOM_CMMD_WRITE_PINS) {
            sscanf(curr_str + strlen(compare_str), " %s", &tmp_buffer[0]);
            write_pins_num = strlen(&tmp_buffer[0]);
            aice_log_add( AICE_LOG_DEBUG, "\t custom_script write_pins, %d %s", write_pins_num, &tmp_buffer[0]);
            for (i = 0; i < ((write_pins_num + 1) >> 1); i ++) {
                Nibble1 = 0;
                Nibble2 = 0;
                sscanf(&tmp_buffer[idx++], "%01x", &Nibble1);
                sscanf(&tmp_buffer[idx++], "%01x", &Nibble2);
                aice_log_add( AICE_LOG_DEBUG, "\t custom_script write_pins, %x %x", Nibble1, Nibble2);
                line_buffer[2 + i] = (char)(Nibble1 | (Nibble2 << 4));
            }
            num_of_words = 2 + ((write_pins_num + 1) >> 1);
            num_of_words = (num_of_words + 3) >> 2;
            line_buffer[0] = (char)(write_pins_num >> 8);
            line_buffer[1] = (char)(write_pins_num & 0xFF);

            result = aice_write_pins(num_of_words, &word_buffer[0]);
            if (result != ERROR_OK)
                goto aice_execute_custom_script_error;
        }
        else if (i == AICE_CUSTOM_CMMD_T_WRITE_MISC) {
            sscanf(curr_str + strlen(compare_str), " %d %d %d", &target_id, &write_misc_addr, &write_misc_data);
            aice_log_add( AICE_LOG_DEBUG, "\t custom_script aice_write_misc, 0x%x, 0x%x, 0x%x", target_id, write_misc_addr, write_misc_data);
            result = aice_write_misc(target_id, write_misc_addr, write_misc_data);
            if (result != ERROR_OK)
                goto aice_execute_custom_script_error;
        }
    }

    fclose(script_fd);
    return result;

aice_execute_custom_script_error:
    aice_log_add( AICE_LOG_DEBUG, "\t Issue custom_script '%s' failed, abandon continue...", curr_str);
    fclose(script_fd);
    return result;
}

extern struct jtag_libusb_device_handle *aice_usb_handle;
// reset c51 through ep0
// The reset sequence is as following.
// 1. send ep0 packet: bmRequestType 0x40, bRequest 0xA0, wValue 0xe600, index 0x0, byte 1, length 1
// 2. delay several milliseconds
// 3. send ep0 packet: bmRequestType 0x40, bRequest 0xA0, wValue 0xe600, index 0x0, byte 0, length 1
// 4. wait c51 reload FPGA
// The reset sequence can work on AICE 1.6.6 or above.
// Bug 7568 - AICE could not be reset by ICEman
int aice_reset_aice_as_startup(void)
{
	int ret;
	unsigned char reset_bit;
	jtag_libusb_device_handle *dev = aice_usb_handle;

	reset_bit = 1;
	ret = jtag_libusb_control_transfer(dev, 0x40, 0xa0, 0xe600, 0x00, (char *)&reset_bit, 1, 5000);
	alive_sleep(100);
	reset_bit = 0;
	ret = jtag_libusb_control_transfer(dev, 0x40, 0xa0, 0xe600, 0x00, (char *)&reset_bit, 1, 5000);
	alive_sleep(1500);
	/* usb_control_msg() returns the number of bytes transferred during the
	 * DATA stage of the control transfer - must be exactly 1 in this case! */
	if (ret != 1)
		return ERROR_FAIL;
	return ERROR_OK;
}

/********************************************************************************************/
/// Modified from aice_usb.c:aice_open_device()
char AndesName[] = {"Andes"};
char *pAICEName[] = {
    "AICE",
    "AICE-MCU",
    "AICE-MINI"
};
extern char *pdescp_Manufacturer;
extern char *pdescp_Product;
extern unsigned int descp_bcdDevice;
static void aice_open (const char *input)
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_open>: ");

    char response[MAXLINE];
    uint16_t vid;
    uint16_t pid;
    int retval = ERROR_FAIL;
    int success_idx = -1;
    struct aice_port_s aice;

    if( vid_pid_array_top == -1 ) {
        aice_log_add(AICE_LOG_ERROR, "There is no vid_pid in config files!!");
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
        exit(-1);
        return;
    }

    for( int i = 0; i <= vid_pid_array_top; i++ ) {
        vid = vid_pid_array[i].vid;
        pid = vid_pid_array[i].pid;

        aice_log_add (AICE_LOG_DEBUG, "\t VID: 0x%04x, PID: 0x%04x", vid, pid);

        retval = aice_usb_open(vid, pid);

        if (ERROR_OK == retval) {
            success_idx = i;
            break;
        }
    }

    if( retval != ERROR_OK ) {
        aice_log_add(AICE_LOG_ERROR, "<-- Can not open usb -->");
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
        exit(-1);
        return;
    }

    if (nds32_reset_aice_as_startup == 1) {
		aice_reset_aice_as_startup();
	}

    response[0] = AICE_OK;

    char buffer[1000] = {0};
    if( vid_pid_array[success_idx].vid == 0x1CFC &&
        vid_pid_array[success_idx].pid == 0x0000 ) {     // OLD Version of AICE, AICE-MCU, AICE-MINI
        
        if (ERROR_FAIL == aice_get_version_info(&aice)) {
            aice_log_add(AICE_LOG_ERROR, "Cannot get AICE info!");
            response[0] = AICE_ERROR;
            pipe_write (response, 1);
            return;
        }           

        char *vid_str, *pid_str;
        unsigned int vid_idx, pid_idx;

        vid_idx = (aice.hardware_version & 0xFF000000) >> 24;
        pid_idx = (aice.hardware_version & 0x00FF0000) >> 16;
        vid_str = (char *)&AndesName[0];
        pid_str = (char *)pAICEName[pid_idx];

        if ((vid_idx == 0) && (pid_idx <= 2)) {
            sprintf( buffer, "%s %s v%d.%d.%d", 
                    vid_str, 
                    pid_str,
                    (aice.hardware_version & 0xFFFF), 
                    aice.firmware_version,
                    aice.fpga_version);
        }
        else {
            sprintf( buffer, "3rd-party ICE-box: ice_ver1 = 0x%08x, ice_ver2 = 0x%08x, ice_ver3 = 0x%08x", 
                aice.hardware_version, aice.firmware_version, aice.fpga_version);
        }
    }
    else {
        sprintf ( buffer, "%s %s bcdDevice=0x%x", 
                          pdescp_Manufacturer, 
                          pdescp_Product, 
                          descp_bcdDevice );
    }


    set_u32(response+1, strlen(buffer) );
    strncpy(response+5, buffer, strlen(buffer));
    pipe_write (response, 5+strlen(buffer));    // byte 0: STATUS, byte 1~4: LENGTH, others: DES_STRING
}


static void aice_set_jtag_clock (const char *input)
{
    char response[MAXLINE];

    jtag_clock = get_u32 (input + 1);

    aice_log_add (AICE_LOG_DEBUG, "<aice_set_jtag_clock>: CLOCK: %d", jtag_clock);

    if (ERROR_OK != aice_usb_set_clock (jtag_clock)) {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
        aice_log_add(AICE_LOG_ERROR, "Failed to set jtag clock!!");
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
    aice_log_add (AICE_LOG_DEBUG, "<aice_close> ");

    char response[MAXLINE];

    aice_usb_close ();

    response[0] = AICE_OK;
    pipe_write (response, 1);

    exit (0);
}

/// Modified from aice_usb.c:aice_usb_reset()
static void aice_reset (const char *input)
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_reset> ");

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
    char response[MAXLINE];
    uint8_t num_of_idcode;
    uint32_t idcode[MAX_ID_CODE];
    uint8_t i;
    int retval;

    retval = aice_scan_chain(idcode, &num_of_idcode);
    if( (retval!=ERROR_OK) || (num_of_idcode==0xFF) ) {
        aice_log_add (AICE_LOG_ERROR, "No target connected");
        response[0] = 0xFF;
        pipe_write (response, 1);
        return;
    }
    else if( num_of_idcode >= MAX_ID_CODE ) {
        aice_log_add (AICE_LOG_ERROR, "The ice chain over 16 targets");
        response[0] = 0;
        pipe_write (response, 1);
        return;
    }

    num_of_idcode++;
    response[0] = num_of_idcode;

    aice_log_add (AICE_LOG_DEBUG, "<aice_idcode>: # of target: %d", num_of_idcode);
    for (i = 0 ; i < num_of_idcode ; i++)
    {
        set_u32 (response + 1 + i*4, idcode[i]);
        aice_log_add (AICE_LOG_DEBUG, "\t IDCode%d: 0x%08X", i, idcode[i]);
    }

    pipe_write (response, 1 + num_of_idcode * 4);
}


static int aice_read_edm (const char *input)
{
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

    aice_log_add (AICE_LOG_DEBUG, "<aice_read_edm>: target_id=0x%08X, cmd=0x%02X, addr=0x%08X, len=0x%08X ", \
                                      target_id, JDPInst, address, num_of_words);


    EDMData = malloc( sizeof(uint32_t) * (num_of_words+1) );
    if( EDMData == NULL ) {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);

        aice_log_add (AICE_LOG_INFO, "Allocate Read EDMData buffer Failed!!");
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

        for (int i = 0 ; i < num_of_words ; i++)
        {
            set_u32 (response + 1 + i*4, EDMData[i]);
        }

        pipe_write (response, 1 + num_of_words * 4);
    }
    else {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);

        aice_log_add (AICE_LOG_INFO, "Read EDM Failed!!");
    }


    free(EDMData);

    return result;
}


static int aice_write_edm( const char *input )
{
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

    aice_log_add (AICE_LOG_DEBUG, "<aice_write_edm>: target_id=0x%08X, cmd=0x%02X, addr=0x%08X, len=0x%08X ", \
                                      target_id, JDPInst, address, num_of_words);

    EDMData = malloc( sizeof(uint32_t) * (num_of_words+1) );
    if( EDMData == NULL ) {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);

        aice_log_add (AICE_LOG_ERROR, "Allocate Write EDMData buffer Failed!!");
        return ERROR_FAIL;
    }

    for (int i = 0 ; i < num_of_words ; i++)
    {
        EDMData[i] = get_u32( input + 14 + i*4 );
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

        aice_log_add (AICE_LOG_ERROR, "Write EDM Failed!!");
    }

    return result;
}

static int aice_write_ctrls( const char *input )
{
    char response[MAXLINE];
    unsigned int address;
    unsigned int WriteData;
    int result;

    address   = get_u32 (input + 1);
    WriteData = get_u32 (input + 5);

    aice_log_add (AICE_LOG_DEBUG, "<aice_write_ctrl>: addr=0x%08X, data=0x%08X ", address, WriteData);
    result = aice_access_cmmd(AICE_CMDIDX_WRITE_CTRL, 0, address, (unsigned char *)&WriteData, 1);

    if( result == ERROR_OK )
        response[0] = AICE_OK;
    else {
        aice_log_add (AICE_LOG_ERROR, "Write ICE box ctrl Failed!!");
        response[0] = AICE_ERROR;
    }
    pipe_write (response, 1);

    return result;
}

static int aice_read_ctrls( const char *input )
{
    char response[MAXLINE];
    unsigned int address;
    unsigned int pReadData;
    int result;

    address   = get_u32 (input + 1);
    result = aice_access_cmmd(AICE_CMDIDX_READ_CTRL, 0, address, (unsigned char *)&pReadData, 1);

    if( result == ERROR_OK ) {
        aice_log_add (AICE_LOG_DEBUG, "<aice_read_ctrl>: addr=0x%08X, recv=0x%08X", address, pReadData);
        response[0] = AICE_OK;
        set_u32(response+1, pReadData);
        pipe_write (response, 5);
    }
    else {
        aice_log_add (AICE_LOG_ERROR, "Read ICE box ctrl Failed!!");

        response[0] = AICE_ERROR;
        pipe_write (response, 1);
    }

    return result;
}

static int aice_custom_script( const char *input )
{
    char response[MAXLINE];
    char filename[MAXLINE];
    int result;

    strcpy( filename, input+1 );
    aice_log_add (AICE_LOG_DEBUG, "<aice_custom_script>: filename=%s", filename);

    result = execute_custom_script( filename );

    if( result == ERROR_OK )
        response[0] = AICE_OK;
    else {
        aice_log_add (AICE_LOG_ERROR, "Execute custom script Failed!!");
        response[0] = AICE_ERROR;
    }
    pipe_write (response, 1);

    return result;
}

static int aice_custom_monitor_cmd( const char *input )
{
    char response[MAXLINE];
    int result;
    int len;
    char *command;

    len = get_u32(input+1);
    command = (char *)malloc((len+1)*sizeof(char));
    memcpy(command, input+5, len);
    command[len] = '\0';
    aice_log_add( AICE_LOG_DEBUG, "<aice_custom_monitor_cmd>: recv: len=%d, %s", len, command);

    result = ERROR_OK;
    if( result == ERROR_OK )
        response[0] = AICE_OK;
    else {
        aice_log_add (AICE_LOG_ERROR, "Custom monitor cmd Failed!!");
        response[0] = AICE_ERROR;
    }
    pipe_write (response, 1);

    return result;
}

static int aice_set_cmd_mode( const char *input )
{
	char response[MAXLINE];
	unsigned int cmd_mode;
	int result = AICE_OK;

	cmd_mode   = get_u32 (input + 1);
	aice_log_add (AICE_LOG_DEBUG, "<aice_set_cmd_mode>: cmd_mode=0x%08X", cmd_mode);
	aice_usb_set_command_mode(cmd_mode);
	response[0] = AICE_OK;
	pipe_write (response, 1);

	return result;
}

static int aice_set_read_dtr_to_buf( const char *input )
{
	char response[MAXLINE];
	unsigned int target_id;
	unsigned int buffer_idx;
	int result;

	target_id   = get_u32 (input + 1);
	buffer_idx   = get_u32 (input + 5);
	aice_log_add (AICE_LOG_DEBUG, "<read_dtr_to_buf>: target_id=0x%08X, target_id=0x%08X", target_id, buffer_idx);
	result = aice_read_dtr_to_buffer(target_id, buffer_idx);
	if( result == ERROR_OK )
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;
	pipe_write (response, 1);

	return result;
}

static int aice_set_write_dtr_from_buf( const char *input )
{
	char response[MAXLINE];
	unsigned int target_id;
	unsigned int buffer_idx;
	int result;

	target_id   = get_u32 (input + 1);
	buffer_idx   = get_u32 (input + 5);
	aice_log_add (AICE_LOG_DEBUG, "<write_dtr_from_buf>: target_id=0x%08X, target_id=0x%08X", target_id, buffer_idx);
	result = aice_write_dtr_from_buffer(target_id, buffer_idx);
	if( result == ERROR_OK )
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;
	pipe_write (response, 1);

	return result;
}

static int aice_set_batch_buffer_read (const char *input)
{
	char response[MAXLINE];
	uint32_t buf_index;
	uint32_t num_of_words;
	int result = 0;
	unsigned char *pReadData = (unsigned char *)&response[1];

	buf_index    = get_u32( input+1 );
	num_of_words = get_u32( input+5 );

	aice_log_add (AICE_LOG_DEBUG, "<set_batch_buffer_read>: buf_index=0x%08X, num_of_words=0x%08X ", \
	                                buf_index, num_of_words);
	result = aice_batch_buffer_read(buf_index, pReadData, num_of_words);
	if( result != ERROR_OK ) {
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return result;
	}

	response[0] = AICE_OK;
	pipe_write (response, 1 + num_of_words * 4);
	return result;
}

static int aice_set_batch_buffer_write (const char *input)
{
	char response[MAXLINE];
	uint32_t buf_index;
	int result = 0;

	buf_index = get_u32( input+1 );
	aice_log_add (AICE_LOG_DEBUG, "<set_batch_buffer_write>: buf_index=0x%08X ", \
	                                buf_index);
	result = aice_batch_buffer_write(buf_index);
	if( result != ERROR_OK )
		response[0] = AICE_ERROR;
	else
		response[0] = AICE_OK;
	pipe_write (response, 1);
	return result;
}

static int aice_set_pack_buffer_read (const char *input)
{
	char response[MAXLINE];
	uint32_t num_of_bytes;
	int result = 0;
	unsigned char *pReadData = (unsigned char *)&response[1];

	num_of_bytes = get_u32( input+1 );

	aice_log_add (AICE_LOG_DEBUG, "<set_pack_buffer_read>: num_of_bytes=0x%08X ", \
	                                num_of_bytes);
	result = aice_pack_buffer_read(pReadData, num_of_bytes);
	response[0] = AICE_OK;
	pipe_write (response, 1 + num_of_bytes);
	return result;
}

#define MAX_FILELIST 10
void parsing_config_file( char* top_filename )
{
    int filelist_top = 0;
    int finish_parsing = 0;
    char* filelist[MAX_FILELIST];
    char str[MAXLINE];
    filelist[filelist_top++] = strdup(top_filename);
    int i;

    while( finish_parsing < filelist_top ) {
        FILE *fp;
        fp = fopen(filelist[finish_parsing++], "r");
        if( fp == NULL ) {
            aice_log_add(AICE_LOG_ERROR, "Can't open config file: %s", filelist[finish_parsing-1]);
            continue;
        }
        aice_log_add(AICE_LOG_DEBUG, "Now file: %s", filelist[finish_parsing-1]);


        /* readline */
        while( fgets(str, MAXLINE, fp) != NULL ) {

            /* find token */
            char *tok = strtok( str, " ");
            if( tok != NULL ) {
                /// for comment
                if( tok[0] == '#' )
                    continue;

                /// parsing command
                if( strncmp(tok, "source", 6) == 0 ) {          /// add file list
                                                                /// i.e. "source [find interface/nds32-aice.cfg]"
                    tok = strtok( NULL, " ");
                    tok = strtok( NULL, " ");
                    filelist[filelist_top] = strdup(tok);
                    filelist[filelist_top][strlen(filelist[filelist_top])-2] = '\0';
                    filelist_top++;
                    aice_log_add(AICE_LOG_DEBUG, "add file: %s",filelist[filelist_top-1]);
                    continue;
                }
                else if( strncmp(tok, "aice", 4) == 0 ) {       /// aice command
                    tok = strtok( NULL, " ");

                    if( strncmp(tok, "reset_aice_as_startup", 21) == 0 ) {  /// i.e. "aice reset_aice_as_startup"
                        nds32_reset_aice_as_startup = 1;

                        aice_log_add(AICE_LOG_DEBUG, "Detection reset_aice_as_startup command");
                    }
                    else if( strncmp(tok, "vid_pid", 7) == 0  ) {
                        vid_pid_array_top++;

                        if(vid_pid_array_top >= AICE_MAX_VID_NUM) {
                            aice_log_add(AICE_LOG_ERROR, "vid_pid array over AICE_MAX_VID_NUM error, ignore to add new vid_pid config!");
                            continue;
                        }

                        tok = strtok( NULL, " ");   //VID
                        char* vid_str = strdup(tok);
                        tok = strtok( NULL, " ");   //PID
                        char* pid_str = strdup(tok);

                        aice_log_add(AICE_LOG_DEBUG, "get vid:0x%s, pid:0x%s", vid_str, pid_str);

                        char *tmp;
                        tmp = vid_str;
                        vid_pid_array[vid_pid_array_top].vid = strtol(vid_str, &tmp, 16);
                        tmp = pid_str;
                        vid_pid_array[vid_pid_array_top].pid = strtol(pid_str, &tmp, 16);

                        free(vid_str);
                        free(pid_str);
                    }

                    continue;
                }
                else if( strncmp(tok, "nds", 3) == 0  ) {
                    tok = strtok( NULL, " ");

                    if( strncmp(tok, "log_file_size", 13) == 0  ) {     /// i.e "nds log_file_size #num"
                        tok = strtok( NULL, " "); //size
                        log_file_size = atoi(tok);

                        aice_log_add(AICE_LOG_DEBUG, "log_file_size: %d", log_file_size);
                    }

                }
                else if( strncmp(tok, "debug_level", 11) == 0 ) {       /// i.e "debug_level #level"
                    tok = strtok( NULL, " "); //level           
                    debug_level = atoi(tok);

                    aice_log_add(AICE_LOG_DEBUG, "debug_level: %d", debug_level);
                }
                else
                    continue;
            }
        }

        fclose(fp);
    }

    for( i=0; i < filelist_top; i++ )
        free(filelist[i]);

}

int main ()
{
    char line[MAXLINE];
    int n;

    signal(SIGINT, SIG_IGN);
    atexit (aice_log_finalize);

    parsing_config_file("openocd.cfg");
    aice_log_init( log_file_size, debug_level, true); 

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
                aice_custom_script(line);
                break;
            case AICE_CUSTOM_MONITOR_CMD:
                aice_custom_monitor_cmd(line);
                break;
            case AICE_SET_CMD_MODE:
                aice_set_cmd_mode(line);
                break;
            case AICE_READ_DTR_TO_BUFFER:
                aice_set_read_dtr_to_buf(line);
                break;
            case AICE_WRITE_DTR_FROM_BUFFER:
                aice_set_write_dtr_from_buf(line);
                break;
            case AICE_BATCH_BUFFER_WRITE:
            		aice_set_batch_buffer_write(line);
                break;
            case AICE_BATCH_BUFFER_READ:
            		aice_set_batch_buffer_read(line);
                break;
            case AICE_PACK_BUFFER_READ:
            		aice_set_pack_buffer_read(line);
                break;
            default:
                aice_log_add (AICE_LOG_INFO, "Error command: 0x%x", line[0] );
                break;
        }
    }

    return 0;
}

