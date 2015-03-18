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
#include <aice_usb_pack_format.h>
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
int aice_is_open = 0;

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

    aice_log_add(AICE_LOG_DEBUG, "\t AICE version: hw_ver = 0x%x, fw_ver = 0x%x, fpga_ver = 0x%x",
        hardware_version, firmware_version, fpga_version);

    //hardware features info
    if (aice_read_ctrl(AICE_READ_CTRL_ICE_CONFIG, &ice_config) != ERROR_OK)
        return ERROR_FAIL;
    aice_log_add(AICE_LOG_DEBUG,"\t AICE hardware features: 0x%08x", ice_config);

    //batch buffer size info
    if (aice_read_ctrl(AICE_REG_BATCH_DATA_BUFFER_1_STATE, &batch_data_buf1_size) != ERROR_OK)
        return ERROR_FAIL;
    batch_data_buf1_size &= 0xffffu;
    aice_log_add(AICE_LOG_DEBUG,"\t AICE batch data buffer size: %d words", batch_data_buf1_size);

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

//int aice_usb_state(uint32_t coreid, enum aice_target_state_s *state)
//{
    //uint32_t dbger_value;
    //uint32_t ice_state;
    //int result = aice_read_misc(coreid, NDS_EDM_MISC_DBGER, &dbger_value);

    //if (ERROR_AICE_TIMEOUT == result) {
        //if (aice_read_ctrl(AICE_READ_CTRL_GET_ICE_STATE, &ice_state) != ERROR_OK) {
            //aice_log_add(AICE_LOG_ERROR, "<-- AICE ERROR! AICE is unplugged. -->");
            //return ERROR_FAIL;
        //}

        //if ((ice_state & 0x20) == 0) {
            //aice_log_add(AICE_LOG_ERROR, "<-- TARGET ERROR! Target is disconnected with AICE. -->");
            //return ERROR_FAIL;
        //} else {
            //return ERROR_FAIL;
        //}
    //} else if (ERROR_AICE_DISCONNECT == result) {
        //aice_log_add(AICE_LOG_ERROR, "<-- AICE ERROR! AICE is unplugged. -->");
        //return ERROR_FAIL;
    //}

    //if ((dbger_value & NDS_DBGER_ILL_SEC_ACC) == NDS_DBGER_ILL_SEC_ACC) {
        //aice_log_add(AICE_LOG_ERROR, "<-- TARGET ERROR! Insufficient security privilege. -->");

        ///* Clear ILL_SEC_ACC */
        //aice_write_misc(coreid, NDS_EDM_MISC_DBGER, NDS_DBGER_ILL_SEC_ACC);

        //*state = AICE_TARGET_RUNNING;
    //} else if ((dbger_value & NDS_DBGER_AT_MAX) == NDS_DBGER_AT_MAX) {
        //aice_log_add(AICE_LOG_ERROR, "<-- TARGET ERROR! Reaching the max interrupt stack level; -->");

        //*state = AICE_TARGET_HALTED;
    //} else if (((dbger_value & NDS_DBGER_CRST) == NDS_DBGER_CRST)) {
        //aice_log_add(AICE_LOG_ERROR, "DBGER.CRST is on.");

        //*state = AICE_TARGET_RESET;

        ///* Clear CRST */
        //aice_write_misc(coreid, NDS_EDM_MISC_DBGER, NDS_DBGER_CRST);
    //} else if ((dbger_value & NDS_DBGER_DEX) == NDS_DBGER_DEX) {
        //*state = AICE_TARGET_HALTED;
    //} else {
        //*state = AICE_TARGET_RUNNING;
    //}

    //return ERROR_OK;
//}


/********************************************************************************************/
/// Modified from aice_usb.c:aice_open_device()
static void aice_open (const char *input)
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_open>: ");

    char response[MAXLINE];
    uint16_t vid;
    uint16_t pid;

    vid = AICE_VID;
    pid = AICE_PID;

    aice_log_add (AICE_LOG_DEBUG, "\t VID: 0x%04x, PID: 0x%04x", vid, pid);

    if (ERROR_OK != aice_usb_open(vid, pid)) {
        aice_log_add(AICE_LOG_ERROR, "\t <-- Can not open usb -->");
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
        return;
    }

    if(ERROR_OK != aice_reset_box()) {
        aice_log_add(AICE_LOG_ERROR, "\t Cannot initial AICE box!");
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
        return;
    }

    if (ERROR_FAIL == aice_get_version_info()) {
        aice_log_add(AICE_LOG_ERROR, "\t Cannot get AICE info!");
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
        return;
    }

    aice_is_open = 1;
    response[0] = AICE_OK;
    pipe_write (response, 1);
}


static void aice_set_jtag_clock (const char *input)
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_set_jtag_clock>: ");

    char response[MAXLINE];

    jtag_clock = get_u32 (input + 1);

    aice_log_add (AICE_LOG_DEBUG, "\t CLOCK: %d", jtag_clock);

    if (ERROR_OK != aice_usb_set_clock (jtag_clock)) {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);
        aice_log_add(AICE_LOG_ERROR, "\t Failed to set jtag clock!!");
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

    aice_is_open = 0;
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
    aice_log_add (AICE_LOG_DEBUG, "<aice_idcode>: ");

    char response[MAXLINE];
    uint8_t num_of_idcode;
    uint32_t idcode[MAX_ID_CODE];
    uint8_t i;
    int retval;

    retval = aice_scan_chain(idcode, &num_of_idcode);
    if( (retval!=ERROR_OK) || (num_of_idcode==0xFF) ) {
        aice_log_add (AICE_LOG_ERROR, "\t No target connected");
        response[0] = 0xFF;
        pipe_write (response, 1);
        return;
    }
    else if( num_of_idcode >= MAX_ID_CODE ) {
        aice_log_add (AICE_LOG_ERROR, "\t The ice chain over 16 targets");
        response[0] = 0;
        pipe_write (response, 1);
        return;
    }

    num_of_idcode++;
    response[0] = num_of_idcode;

    aice_log_add (AICE_LOG_DEBUG, "\t # of target: %d", num_of_idcode);
    for (i = 0 ; i < num_of_idcode ; i++)
    {
        set_u32 (response + 1 + i*4, idcode[i]);
        aice_log_add (AICE_LOG_DEBUG, "\t\t IDCode%d: 0x%08X", i, idcode[i]);
    }

    pipe_write (response, 1 + num_of_idcode * 4);
}

//static void aice_state (const char *input)
//{
    //aice_log_add (AICE_LOG_DEBUG, "<aice_state>: ");

    //enum aice_target_state_s state;
    //uint32_t coreid = get_u32 (input + 1);

    //char response[MAXLINE];
    //int result = aice_usb_state (coreid, &state);

    //if(result == ERROR_OK){
        //aice_log_add(AICE_LOG_DEBUG, "Read coreid #%d state OK!", coreid);

        //response[0] = AICE_OK;
        //response[1] = state;
        //pipe_write (response, 2);
    //}
    //else {
        //aice_log_add(AICE_LOG_ERROR, "Read coreid #%d state Failed!", coreid);

        //response[0] = AICE_ERROR;
        //pipe_write (response, 1);
    //}
//}

static int aice_read_edm (const char *input)
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_read_edm>: ");

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

    aice_log_add (AICE_LOG_DEBUG, "\t target_id=0x%08X, cmd=0x%02X, addr=0x%08X, len=0x%08X ", \
                                      target_id, JDPInst, address, num_of_words);


    EDMData = malloc( sizeof(uint32_t) * (num_of_words+1) );
    if( EDMData == NULL ) {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);

        aice_log_add (AICE_LOG_INFO, "\t Allocate EDMData buffer Failed!!");
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

        //aice_log_add (AICE_LOG_INFO, "\t recv:");

        for (int i = 0 ; i < num_of_words ; i++)
        {
            set_u32 (response + 1 + i*4, EDMData[i]);

            //aice_log_add( AICE_LOG_INFO, "\t\t0x%08X", EDMData[i]);
        }

        pipe_write (response, 1 + num_of_words * 4);

        aice_log_add (AICE_LOG_INFO, "\t Read EDM Finish!");
    }
    else {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);

        aice_log_add (AICE_LOG_INFO, "\t Read EDM Failed!!");
    }


    free(EDMData);

    return result;
}


static int aice_write_edm( const char *input )
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_write_edm>: ");

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

    aice_log_add (AICE_LOG_DEBUG, "\t target_id=0x%08X, cmd=0x%02X, addr=0x%08X, len=0x%08X ", \
                                      target_id, JDPInst, address, num_of_words);

    EDMData = malloc( sizeof(uint32_t) * (num_of_words+1) );
    if( EDMData == NULL ) {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);

        aice_log_add (AICE_LOG_ERROR, "\t Allocate EDMData buffer Failed!!");
        return ERROR_FAIL;
    }

    //aice_log_add (AICE_LOG_DEBUG, "\t data:");
    for (int i = 0 ; i < num_of_words ; i++)
    {
        EDMData[i] = get_u32( input + 14 + i*4 );

        //aice_log_add( AICE_LOG_INFO, "\t\t0x%08X", EDMData[i]);
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

        aice_log_add (AICE_LOG_ERROR, "\t Write EDM Finish!");
    }
    else {
        response[0] = AICE_ERROR;
        pipe_write (response, 1);

        aice_log_add (AICE_LOG_ERROR, "\t Write EDM Failed!!");
    }

    return result;
}

static int aice_write_ctrls( const char *input )
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_write_ctrl>: ");

    char response[MAXLINE];
    unsigned int address;
    unsigned int WriteData;
    int result;

    address   = get_u32 (input + 1);
    WriteData = get_u32 (input + 5);

    aice_log_add (AICE_LOG_DEBUG, "\t addr=0x%08X, data=0x%08X ", address, WriteData);
    result = aice_access_cmmd(AICE_CMDIDX_WRITE_CTRL, 0, address, (unsigned char *)&WriteData, 1);

    if( result == ERROR_OK )
        response[0] = AICE_OK;
    else {
        aice_log_add (AICE_LOG_ERROR, "\t Write ICE box ctrl Failed!!");
        response[0] = AICE_ERROR;
    }
    pipe_write (response, 1);

    return result;
}

static int aice_read_ctrls( const char *input )
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_read_ctrl>: ");

    char response[MAXLINE];
    unsigned int address;
    unsigned int pReadData;
    int result;

    address   = get_u32 (input + 1);

    aice_log_add (AICE_LOG_DEBUG, "\t addr=0x%08X", address);

    result = aice_access_cmmd(AICE_CMDIDX_READ_CTRL, 0, address, (unsigned char *)&pReadData, 1);

    if( result == ERROR_OK ) {
        aice_log_add (AICE_LOG_DEBUG, "\t recv: 0x%08X", pReadData);

        response[0] = AICE_OK;
        set_u32(response+1, pReadData);
        pipe_write (response, 5);
    }
    else {
        aice_log_add (AICE_LOG_ERROR, "\t Read ICE box ctrl Failed!!");

        response[0] = AICE_ERROR;
        pipe_write (response, 1);
    }


    return result;
}

static int aice_custom_script( const char *input )
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_custom_script>: ");

    char response[MAXLINE];
    char filename[MAXLINE];
    int result;

    strcpy( filename, input+1 );
    aice_log_add (AICE_LOG_DEBUG, "\t filename: %s", filename);

    result = execute_custom_script( filename );

    if( result == ERROR_OK )
        response[0] = AICE_OK;
    else {
        aice_log_add (AICE_LOG_ERROR, "\t Execute custom script Failed!!");
        response[0] = AICE_ERROR;
    }
    pipe_write (response, 1);

    return result;
}

static enum{
        CONFIRM_USB_CONNECTION = 0,
        CONFIRM_AICE_VERSIONS,
        CONFIRM_JTAG_FREQUENCY_DETECTED,
        CONFIRM_SET_JTAG_FREQUENCY,
        CONFIRM_JTAG_CONNECTIVITY,
        CONFIRM_JTAG_DOMAIN,
        CONFIRM_TRST_WORKING,
        CONFIRM_SRST_NOT_AFFECT_JTAG,
        //CONFIRM_SELECT_CORE,
        //CONFIRM_RESET_HOLD,
        //CONFIRM_DIM_AND_CPU_DOMAIN,
        //CONFIRM_MEMORY_ON_BUS,
        //CONFIRM_MEMORY_ON_CPU,
        CONFIRM_END,
};

static const char *confirm_messages[]={
    "check USB connectivity ...",
    "check AICE versions ...",
    "check the detected JTAG frequency ...",
    "check changing the JTAG frequency ...",
    "check JTAG connectivity ...",
    "check that JTAG domain is operational ...",
    "check that TRST resets the JTAG domain ...",
    "check that SRST does not resets JTAG domain ..."
    //"check selecting core ...",
    //"check reset-and-debug ...",
    //"check that DIM and CPU domain are operational ...",
    //"check accessing memory through BUS ...",
    //"check accessing memory through CPU ..."
};

static unsigned int confirm_result[CONFIRM_END];
static const char *confirm_result_msg[]={
    "[PASS]",
    "[FAIL]",
    "[NON-SUPPORTED]",
};

static void aice_diagnostic( const char *input )
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_diagnostic>: ");

    uint16_t vid = AICE_VID;
    uint16_t pid = AICE_PID;
    uint8_t total_num_of_core = aice_num_of_target_id_codes;

    unsigned int id_codes[MAX_ID_CODE];
    uint32_t i = 0;
    int last_fail_item, item;
    uint32_t value, backup_value, test_value;
    uint32_t selected_core;
    uint32_t hardware_version=0, firmware_version=0, fpga_version=0;
    uint32_t scan_clock;

    aice_log_add(AICE_LOG_INFO, "********************");
    aice_log_add(AICE_LOG_INFO, "Diagnostic Report");
    aice_log_add(AICE_LOG_INFO, "********************");

    for (i=0; i<CONFIRM_END; i++)
        confirm_result[i] = 1;

    /* 1. confirm USB connection */
    last_fail_item = CONFIRM_USB_CONNECTION;
    if ( (aice_is_open != 1) && (ERROR_OK != aice_usb_open(vid, pid)) ) {
        aice_log_add(AICE_LOG_INFO, "[FAIL] check USB connectivity ...");
        return;
    }
    confirm_result[last_fail_item] = 0;


    /* clear timeout status */
    if (aice_write_ctrl(AICE_WRITE_CTRL_CLEAR_TIMEOUT_STATUS, 0x1) != ERROR_OK)
        goto diagnosis_JTAG_frequency_detected;

    /* clear NO_DBGI_PIN */
    uint32_t pin_status=0;
    if (aice_read_ctrl(AICE_READ_CTRL_GET_JTAG_PIN_STATUS, &pin_status) != ERROR_OK)
        goto diagnosis_JTAG_frequency_detected;

    if (aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_STATUS, pin_status & (~0x02)) != ERROR_OK)
        goto diagnosis_JTAG_frequency_detected;

    /* get hardware and firmware version */
    last_fail_item = CONFIRM_AICE_VERSIONS;
    if (aice_read_ctrl(AICE_READ_CTRL_GET_HARDWARE_VERSION, &hardware_version) != ERROR_OK)
        goto diagnosis_JTAG_frequency_detected;

    if (aice_read_ctrl(AICE_READ_CTRL_GET_FIRMWARE_VERSION, &firmware_version) != ERROR_OK)
        goto diagnosis_JTAG_frequency_detected;

    if (aice_read_ctrl(AICE_READ_CTRL_GET_FPGA_VERSION, &fpga_version) != ERROR_OK)
        goto diagnosis_JTAG_frequency_detected;

    aice_log_add(AICE_LOG_INFO, "v%d.%d.%d", (hardware_version & 0xFFFF), firmware_version, fpga_version);
    confirm_result[last_fail_item] = 0;

diagnosis_JTAG_frequency_detected:
    /* 2. Report JTAG frequency detected */
    last_fail_item = CONFIRM_JTAG_FREQUENCY_DETECTED;
    if (aice_write_ctrl(AICE_WRITE_CTRL_TCK_CONTROL, AICE_TCK_CONTROL_TCK_SCAN) != ERROR_OK)
        goto diagnosis_set_JTAG_frequency;
    if (aice_read_ctrl(AICE_READ_CTRL_GET_ICE_STATE, &scan_clock) != ERROR_OK)
        goto diagnosis_set_JTAG_frequency;
    scan_clock &= 0x0F;
    // If scan-freq = 48MHz, use 24MHz by default
    if (scan_clock == 8)
        scan_clock = 9;
    aice_log_add(AICE_LOG_INFO, "JTAG frequency %d", scan_clock);
    confirm_result[last_fail_item] = 0;

diagnosis_set_JTAG_frequency:
    /* 3. [Optional] set user specified JTAG frequency */
    last_fail_item = CONFIRM_SET_JTAG_FREQUENCY;

    aice_log_add(AICE_LOG_INFO, "set JTAG frequency %d ...", scan_clock+1);
    if (ERROR_OK != aice_usb_set_clock(scan_clock + 1))
        goto diagnosis_JTAG_connect;
    aice_log_add(AICE_LOG_INFO, "set JTAG frequency %d ...", scan_clock);
    if (ERROR_OK != aice_usb_set_clock(scan_clock))
        goto diagnosis_JTAG_connect;

    aice_log_add(AICE_LOG_INFO, "ice_state = %08x", scan_clock);
    confirm_result[last_fail_item] = 0;

diagnosis_JTAG_connect:
    /* 4. Report JTAG scan chain (confirm JTAG connectivity) */
    last_fail_item = CONFIRM_JTAG_CONNECTIVITY;
    if (ERROR_OK != aice_scan_chain(&id_codes[0], &total_num_of_core))
        goto diagnosis_JTAG_domain;
    total_num_of_core++;
    aice_log_add(AICE_LOG_INFO, "There %s %u %s in target", total_num_of_core > 1 ? "are":"is", total_num_of_core, total_num_of_core > 1 ? "cores":"core");
    confirm_result[last_fail_item] = 0;

diagnosis_JTAG_domain:
    for(i = 0; i < total_num_of_core; i++){
        selected_core = i;

        /* 5. Read/write SBAR: confirm JTAG domain working */
        last_fail_item = CONFIRM_JTAG_DOMAIN;
        test_value = rand() & 0xFFFFFD;

        aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &backup_value);
        aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, test_value);
        aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &value);
        if(value == test_value)
            confirm_result[last_fail_item] = 0;

        /* 5.a */
        last_fail_item = CONFIRM_TRST_WORKING;
        aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, 0);
        aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_TRST);
        aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &value);
        if(value == 0x1)
            confirm_result[last_fail_item] = 0;

        /* 5.b */
        last_fail_item = CONFIRM_SRST_NOT_AFFECT_JTAG;
        aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, 0);
        aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_SRST);
        aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &value);
        if(value == 0x0)
            confirm_result[last_fail_item] = 0;
        /* restore SBAR */
        aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, backup_value);

    report:
        aice_log_add(AICE_LOG_INFO, "********************");
        aice_log_add(AICE_LOG_INFO, "CODE #%d Report:", i);
        for (item = CONFIRM_AICE_VERSIONS; item <= last_fail_item; item++){
            aice_log_add(AICE_LOG_INFO, "%s %s", confirm_result_msg[confirm_result[item]], confirm_messages[item]);
        }
        aice_log_add(AICE_LOG_INFO, "********************");
    }

    char response[MAXLINE];
    response[0] = AICE_OK;
    pipe_write (response, 1);
}


static int aice_custom_monitor_cmd( const char *input ) 
{
    aice_log_add (AICE_LOG_DEBUG, "<aice_custom_monitor_cmd>: ");

    char response[MAXLINE];
    int result;

    aice_log_add( AICE_LOG_DEBUG, "\t recv: %s", input+1);

    result = ERROR_OK;
    if( result == ERROR_OK )
        response[0] = AICE_OK;
    else {
        aice_log_add (AICE_LOG_ERROR, "\t Custom monitor cmd Failed!!");
        response[0] = AICE_ERROR;
    }
    pipe_write (response, 1);

    return result;
}


int main ()
{
    char line[MAXLINE];
    int n;

    signal(SIGINT, SIG_IGN);
    atexit (aice_log_finalize);

    aice_log_init (512*1024, AICE_LOG_DEBUG, true);

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
            case AICE_DIAGNOSTIC:
                aice_diagnostic(line);
                break;
            case AICE_GET_ICE_STATE:
            //    aice_state(line);
            //    break;
            case AICE_CUSTOM_MONITOR_CMD:
                aice_custom_monitor_cmd(line);
                break;
            default:
                aice_log_add (AICE_LOG_INFO, "Error command: %c", line[0] );
                break;
        }
    }

    return 0;
}
