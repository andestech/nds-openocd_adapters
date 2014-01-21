#include <stdio.h>
#include <string.h>
#include "burner_socket.h"
#include "burner_action.h"

static const char *command_string[TARGET_CMD_NUMBER] =
{
	"",
	"reset run\x1a",
	"reset halt\x1a",
	"halt\x1a",
	"mem2array tcl_data 32 0x%08x 1\x1a",
	"mem2array tcl_data 16 0x%04x 1\x1a",
	"mem2array tcl_data 8  0x%02x 1\x1a",
	"mww 0x%08x 0x%08x 1\x1a",
	"mwh 0x%08x 0x%04hx 1\x1a",
	"mwb 0x%08x 0x%02hhx 1\x1a",
	"nds32.cpu0 nds mem_access bus\x1a",
	"nds32.cpu0 nds mem_access cpu\x1a",
	"poll off\x1a",
	"poll on\x1a",
	"nds32.cpu0 nds bulk_write 0x%08x %d ",
	"nds32.cpu0 nds bulk_read 0x%08x %d\x1a",
	"nds32.cpu0 nds read_edmsr %s\x1a",
	"nds32.cpu0 nds write_edmsr %s 0x%08x\x1a",
	"nds32.cpu0 nds multi_write %s %d ",
};

static int issue_command(const char *command, char *response)
{
	int command_len = strlen(command);
	int received_len;
	char *res_ptr;
	int success = 0;

	tcl_send(command, command_len);
	//fprintf(stderr, "issue command: %s\n", command);
	res_ptr = response;
	while ((received_len = tcl_receive(res_ptr, MAX_RESPONSE_LEN)) > 0)
	{
		if (res_ptr[received_len-1] == '\x1a')
		{
			success = 1;
			res_ptr[received_len-1] = '\0';
			break;
		}
		res_ptr += received_len;
	}

	if (success)
		return 0;

	return -1;
}

void write_word (const char *packet, char *packet_response, int *response_len) {
	unsigned int address, data;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];

	address = ((((unsigned int)packet[5]<< 24) & 0xFF000000) |
			(((unsigned int)packet[4]<< 16) & 0x00FF0000) |
			(((unsigned int)packet[3]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[2]<< 0) & 0x000000FF));
	data =    ((((unsigned int)packet[9]<< 24) & 0xFF000000) |
			(((unsigned int)packet[8]<< 16) & 0x00FF0000) |
			(((unsigned int)packet[7]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[6]<< 0) & 0x000000FF));

	sprintf (command, command_string[TARGET_CMD_WRITE_WORD], address, data);
	if (issue_command(command, response) == 0)
		packet_response[0] = WRITE_WORD;
	else
		packet_response[0] = WRITE_WORD | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void write_half (const char *packet, char *packet_response, int *response_len) {
	unsigned int address;
	unsigned short data;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];

	address = ((((unsigned int)packet[5]<< 24) & 0xFF000000) |
			(((unsigned int)packet[4]<< 16) & 0x00FF0000) |
			(((unsigned int)packet[3]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[2]<< 0) & 0x000000FF));
	data =((((unsigned int)packet[7]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[6]<< 0) & 0x000000FF));

	sprintf (command, command_string[TARGET_CMD_WRITE_HALF], address, data);
	if (issue_command(command, response) == 0)
		packet_response[0] = WRITE_HALF;
	else
		packet_response[0] = WRITE_HALF | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void write_byte (const char *packet, char *packet_response, int *response_len) {
	unsigned int address;
	unsigned char data;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];

	address = ((((unsigned int)packet[5]<< 24) & 0xFF000000) |
			(((unsigned int)packet[4]<< 16) & 0x00FF0000) |
			(((unsigned int)packet[3]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[2]<< 0) & 0x000000FF));
	data =(((unsigned int)packet[6]<< 0) & 0x000000FF);

	sprintf (command, command_string[TARGET_CMD_WRITE_BYTE], address, data);
	if (issue_command(command, response) == 0)
		packet_response[0] = WRITE_BYTE;
	else
		packet_response[0] = WRITE_BYTE | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void multiple_write (const char *packet, char *packet_response, int *response_len) {
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];
	char data_str[16];
	int command_length;
	unsigned int multiple_write_type = (unsigned int)packet[0];
	unsigned int num_of_pairs = (unsigned int)packet[1];
	const unsigned int *data_ptr = (unsigned int *)(packet + 2);
	unsigned int i;

	if (num_of_pairs > MAX_MULTI_WRITE_PAIR)
	{
		packet_response[0] = multiple_write_type | 0x80;
		packet_response[1] = 0;
		*response_len = 2;

		return;
	}

	char *PRTYPE;
	switch(multiple_write_type){
		case MULTIPLE_WRITE_W:
			PRTYPE = "0x%08x ";
			sprintf(command, command_string[TARGET_CMD_MULTI_WRITE], "w", num_of_pairs);
			break;
		case MULTIPLE_WRITE_H:
			PRTYPE = "0x%04hx ";
			sprintf(command, command_string[TARGET_CMD_MULTI_WRITE], "h", num_of_pairs);
			break;
		case MULTIPLE_WRITE_B:
			PRTYPE = "0x%02hhx ";
			sprintf(command, command_string[TARGET_CMD_MULTI_WRITE], "b", num_of_pairs);
			break;
	}

	for (i = 0 ; i < num_of_pairs ; i++)
	{
		/* append address */
		sprintf(data_str, "0x%08x ", data_ptr[2*i]);
		strcat(command, data_str);
		/* append data */
		sprintf(data_str, PRTYPE, data_ptr[2*i+1]);
		strcat(command, data_str);
	}
	command_length = strlen(command);
	command[command_length - 1] = '\x1a';

	if (issue_command(command, response) == 0)
		packet_response[0] = multiple_write_type;
	else
		packet_response[0] = multiple_write_type | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void write_io (const char *command, char *response, int *response_len) {
	unsigned int address, size;

	address = ((((unsigned int)command[5]<< 24) & 0xFF000000) |
			(((unsigned int)command[4]<< 16) & 0x00FF0000) |
			(((unsigned int)command[3]<< 8) & 0x0000FF00) |
			(((unsigned int)command[2]<< 0) & 0x000000FF));
	size =    ((((unsigned int)command[9]<< 24) & 0xFF000000) |
			(((unsigned int)command[8]<< 16) & 0x00FF0000) |
			(((unsigned int)command[7]<< 8) & 0x0000FF00) |
			(((unsigned int)command[6]<< 0) & 0x000000FF));

	/*
	   AccessMem *memory = board_->memory (core_id_);
	   if (memory->write_mem_io (address, (unsigned char *)(command + 12), size) < 0)
	   response[0] = WRITE_IO | 0x80;
	   else
	   */
	response[0] = WRITE_IO;

	response[1] = 0;
	*response_len = 2;
}

void read_word(const char *packet, char *packet_response, int *response_len) {
	unsigned int address, data;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];

	address = ((((unsigned int)packet[5]<< 24) & 0xFF000000) |
			(((unsigned int)packet[4]<< 16) & 0x00FF0000) |
			(((unsigned int)packet[3]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[2]<< 0) & 0x000000FF));

	sprintf (command, command_string[TARGET_CMD_READ_WORD], address);
	issue_command(command, response);

	sprintf (command, "expr $tcl_data(0)\x1a");
	if (0 == issue_command(command, response))
	{
		data = strtoul(response, NULL, 0);
		packet_response[0] = READ_WORD;
		packet_response[1] = 0;
		packet_response[2] = (char)((data & 0xFF000000) >> 24);
		packet_response[3] = (char)((data & 0x00FF0000) >> 16);
		packet_response[4] = (char)((data & 0x0000FF00) >> 8);
		packet_response[5] = (char)((data & 0x000000FF) >> 0);
		*response_len = 6;
	}
	else
	{
		packet_response[0] = READ_WORD | 0x80;
		packet_response[1] = 0;
		packet_response[2] = 0xFF;
		packet_response[3] = 0xFF;
		packet_response[4] = 0xFF;
		packet_response[5] = 0xFF;
		*response_len = 6;
	}
}

void read_half(const char *packet, char *packet_response, int *response_len) {
	unsigned int address;
	unsigned short data;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];

	address = ((((unsigned int)packet[5]<< 24) & 0xFF000000) |
			(((unsigned int)packet[4]<< 16) & 0x00FF0000) |
			(((unsigned int)packet[3]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[2]<< 0) & 0x000000FF));

	sprintf (command, command_string[TARGET_CMD_READ_HALF], address);
	issue_command(command, response);

	sprintf (command, "expr $tcl_data(0)\x1a");
	if (0 == issue_command(command, response))
	{
		data = strtoul(response, NULL, 0);
		packet_response[0] = READ_HALF;
		packet_response[1] = 0;
		packet_response[2] = (char)((data & 0x0000FF00) >> 8);
		packet_response[3] = (char)((data & 0x000000FF) >> 0);
		*response_len = 4;
	}
	else
	{
		packet_response[0] = READ_HALF | 0x80;
		packet_response[1] = 0;
		packet_response[2] = 0xFF;
		packet_response[3] = 0xFF;
		*response_len = 4;
	}
}

void read_byte(const char *packet, char *packet_response, int *response_len) {
	unsigned int address;
	unsigned char data;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];

	address = ((((unsigned int)packet[5]<< 24) & 0xFF000000) |
			(((unsigned int)packet[4]<< 16) & 0x00FF0000) |
			(((unsigned int)packet[3]<< 8) & 0x0000FF00) |
			(((unsigned int)packet[2]<< 0) & 0x000000FF));

	sprintf (command, command_string[TARGET_CMD_READ_BYTE], address);
	issue_command(command, response);

	sprintf (command, "expr $tcl_data(0)\x1a");
	if (0 == issue_command(command, response))
	{
		data = strtoul(response, NULL, 0);
		packet_response[0] = READ_BYTE;
		packet_response[1] = 0;
		packet_response[2] = (char)((data & 0x000000FF) >> 0);
		*response_len = 3;
	}
	else
	{
		packet_response[0] = READ_BYTE | 0x80;
		packet_response[1] = 0;
		packet_response[2] = 0xFF;
		*response_len = 3;
	}
}

void read_edm_sr(const char *packet, char *packet_response, int *response_len) {
	unsigned int address;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];
	unsigned int reg_value;

	address = *(int *)(packet+2);

	const char *edm_sr_name;
	switch (address) {
		case EDMSR_EDM_CFG:
			edm_sr_name = "edm_cfg";
			break;
		case EDMSR_EDMSW:
			edm_sr_name = "edmsw";
			break;
		case EDMSR_EDM_CTL:
			edm_sr_name = "edm_ctl";
			break;
		case EDMSR_EDM_DTR:
			edm_sr_name = "edm_dtr";
			break;
		case EDMSR_BPMTC:
			edm_sr_name = "bpmtc";
			break;
		case EDMSR_DIMBR:
			edm_sr_name = "dimbr";
			break;
		case EDMSR_TECR0:
			edm_sr_name = "tecr0";
			break;
		case EDMSR_TECR1:
			edm_sr_name = "tecr1";
			break;
		default:
			packet_response[0] = READ_EDM_SR | 0x80;
			packet_response[1] = 0;
			return;
	}

	sprintf (command, command_string[TARGET_CMD_READ_EDM_REG], edm_sr_name);
	if (0 == issue_command(command, response))
	{
		reg_value = strtoul(response, NULL, 0);
		packet_response[0] = READ_EDM_SR;
		packet_response[1] = 0;
		packet_response[2] = (char)((reg_value & 0xFF000000) >> 24);
		packet_response[3] = (char)((reg_value & 0x00FF0000) >> 16);
		packet_response[4] = (char)((reg_value & 0x0000FF00) >> 8);
		packet_response[5] = (char)((reg_value & 0x000000FF) >> 0);
	}
	else
	{
		packet_response[0] = READ_EDM_SR | 0x80;
		packet_response[1] = 0;
	}
	*response_len = 6;
}

void write_edm_sr (const char *packet, char *packet_response, int *response_len) {
	unsigned int address, data;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];

	address = *(int *)(packet+2);
	data = *(int *)(packet+6);

	const char *edm_sr_name;
	switch (address) {
		case EDMSR_EDM_CFG:
			edm_sr_name = "edm_cfg";
			break;
		case EDMSR_EDMSW:
			edm_sr_name = "edmsw";
			break;
		case EDMSR_EDM_CTL:
			edm_sr_name = "edm_ctl";
			break;
		case EDMSR_EDM_DTR:
			edm_sr_name = "edm_dtr";
			break;
		case EDMSR_BPMTC:
			edm_sr_name = "bpmtc";
			break;
		case EDMSR_DIMBR:
			edm_sr_name = "dimbr";
			break;
		case EDMSR_TECR0:
			edm_sr_name = "tecr0";
			break;
		case EDMSR_TECR1:
			edm_sr_name = "tecr1";
			break;
		default:
			packet_response[0] = WRITE_EDM_SR | 0x80;
			packet_response[1] = 0;
			return;
	}

	sprintf (command, command_string[TARGET_CMD_WRITE_EDM_REG], edm_sr_name, data);
	if (issue_command(command, response) == 0)
		packet_response[0] = WRITE_EDM_SR;
	else
		packet_response[0] = WRITE_EDM_SR | 0x80;
	packet_response[1] = 0;
	*response_len = 2;
}

void fast_read (const char *packet, char *packet_response, int *response_len) {
	unsigned int address;
	int count;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];
	int i;
	int *data = (int *)(packet_response + 2);
	char *data_ptr;
	char *next_data_ptr;
	int num_of_word;

	address = *(int *)(packet+2);
	count = *(int *)(packet+6);

	if (count % 4)
		num_of_word = count / 4 + 1;
	else
		num_of_word = count / 4;

	sprintf (command, command_string[TARGET_CMD_BULK_READ], address, num_of_word);
	if (0 == issue_command(command, response))
	{
		data_ptr = response;
		for (i = 0 ; i < num_of_word; i++)
		{
			*data = strtoul(data_ptr, &next_data_ptr, 0);
			data_ptr = next_data_ptr;
			data++;
		}
	}
	else
	{
		packet_response[0] = FAST_READ | 0x80;
		packet_response[1] = 0;
		*response_len = 2;
		return;
	}

	packet_response[0] = FAST_READ;
	packet_response[1] = 0;
	*response_len = count + 2;
}

void fast_write (const char *packet, char *packet_response, int *response_len) {
	unsigned int address, count;
	char command[MAX_COMMAND_LEN];
	char response[MAX_RESPONSE_LEN];
	char data_str[16];
	int command_length;
	int i;
	const int *data = (int *)(packet + 12);

	address = *(int *)(packet+2);
	count = *(int *)(packet+6);

	sprintf(command, command_string[TARGET_CMD_BULK_WRITE], address, count / 4);
	for (i = 0; i < count / 4; i++)
	{
		sprintf(data_str, "0x%08x ", data[i]);
		strcat(command, data_str);
	}
	command_length = strlen(command);
	command[command_length - 1] = '\x1a';

	if (issue_command(command, response) == 0)
		packet_response[0] = FAST_WRITE;
	else
		packet_response[0] = FAST_WRITE | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void hold_core(const char *packet, char *packet_response, int *response_len) {
	char response[MAX_RESPONSE_LEN];

	if (issue_command(command_string[TARGET_CMD_HALT], response) == 0)
		packet_response[0] = HOLD_CORE;
	else
		packet_response[0] = HOLD_CORE | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void burner_init (const char *command, char *response, int *response_len) {
	response[0] = BURNER_INIT;
	response[1] = 0;
	*response_len = 2;
}

void reset_target (const char *packet, char *packet_response, int *response_len) {
	char response[MAX_RESPONSE_LEN];

	if (issue_command(command_string[TARGET_CMD_RESET_TARGET], response) == 0)
		packet_response[0] = RESET_TARGET;
	else
		packet_response[0] = RESET_TARGET | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void reset_hold (const char *packet, char *packet_response, int *response_len) {
	char response[MAX_RESPONSE_LEN];

	if (issue_command(command_string[TARGET_CMD_RESET_HOLD], response) == 0)
		packet_response[0] = RESET_HOLD;
	else
		packet_response[0] = RESET_HOLD | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

void reset_aice (const char *packet, char *packet_response, int *response_len) {
	char response[MAX_RESPONSE_LEN];

	// TODO
	//if (issue_command(command_string[TARGET_CMD_NO_COMMAND], response) == 0)
	if (1)
		packet_response[0] = RESET_AICE;
	else
		packet_response[0] = RESET_AICE | 0x80;

	packet_response[1] = 0;
	*response_len = 2;
}

int send_cmd(enum _target_command a_command) {
	char response[MAX_RESPONSE_LEN];

	return issue_command(command_string[a_command], response);
}

int handle_packet(const char *packet, int length) {
	char res_buf[9000];
	int res_length = 0;

	switch (packet[0])
	{
		case WRITE_WORD:
			write_word(packet, res_buf, &res_length);
			break; 
		case WRITE_HALF:
			write_half(packet, res_buf, &res_length);
			break;
		case WRITE_BYTE:
			write_byte(packet, res_buf, &res_length);
			break;
		case READ_WORD:
			read_word(packet, res_buf, &res_length);
			break;
		case READ_HALF:
			read_half(packet, res_buf, &res_length);
			break;
		case READ_BYTE:
			read_byte(packet, res_buf, &res_length);
			break; 
		case FAST_READ:
			fast_read(packet, res_buf, &res_length);
			break;
		case FAST_WRITE:
			fast_write(packet, res_buf, &res_length);
			break;
		case WRITE_IO:
			write_io(packet, res_buf, &res_length);
			break;
		case HOLD_CORE:
			hold_core(packet, res_buf, &res_length);
			break; 
		case RESET_TARGET:
			reset_target(packet, res_buf, &res_length);
			break;
		case RESET_HOLD:
			reset_hold(packet, res_buf, &res_length);
			break; 
		case RESET_AICE:
			reset_aice(packet, res_buf, &res_length);
			break;
		case BURNER_INIT:
			burner_init(packet, res_buf, &res_length);
			break;
		case BURNER_QUIT:
			return -1;
		case MULTIPLE_WRITE_W:
		case MULTIPLE_WRITE_H:
		case MULTIPLE_WRITE_B:
			multiple_write(packet, res_buf, &res_length);
			break;
		case READ_EDM_SR:
			read_edm_sr(packet, res_buf, &res_length);
			break;
		case WRITE_EDM_SR:
			write_edm_sr(packet, res_buf, &res_length);
			break;
		default:
			break;
	}

	burner_send(res_buf, res_length);

	return 0;
}

