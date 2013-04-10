#ifndef __BURNER_ACTION_H__
#define __BURNER_ACTION_H__

#define WRITE_WORD 	0x1a
#define READ_WORD 	0x1b
#define WRITE_BYTE	0x2a
#define READ_BYTE	0x2b
#define WRITE_HALF	0x4a
#define READ_HALF	0x4b
#define FAST_READ 	0x1c
#define WRITE_IO 	0x1F
#define HOLD_CORE 	0x1d
#define FAST_WRITE 	0x20
#define RESET_TARGET 	0x3a
#define RESET_HOLD 	0x3b
#define RESET_AICE 	0x3c
#define BURNER_QUIT 	0x04
#define MULTIPLE_WRITE 	0x5a
#define READ_EDM_SR 	0x60
#define WRITE_EDM_SR 	0x61
#define BURNER_INIT 	0x01

#define MAX_COMMAND_LEN 	4096
#define MAX_RESPONSE_LEN 	4096

#define MAX_MULTI_WRITE_PAIR	256

#define EDMSR_EDM_CFG 	(0x28)
#define EDMSR_EDMSW 	(0x30)
#define EDMSR_EDM_CTL 	(0x38)
#define EDMSR_EDM_DTR 	(0x40)
#define EDMSR_BPMTC 	(0x48)
#define EDMSR_DIMBR 	(0x50)
#define EDMSR_TECR0 	(0x70)
#define EDMSR_TECR1 	(0x71)


enum _target_command {
	TARGET_CMD_NO_COMMAND = 0,
	TARGET_CMD_RESET_TARGET,
	TARGET_CMD_RESET_HOLD,
	TARGET_CMD_HALT,
	TARGET_CMD_READ_WORD,
	TARGET_CMD_WRITE_WORD,
	TARGET_CMD_BUS_MODE,
	TARGET_CMD_CPU_MODE,
	TARGET_CMD_POLL_OFF,
	TARGET_CMD_POLL_ON,
	TARGET_CMD_BULK_WRITE,
	TARGET_CMD_BULK_READ,
	TARGET_CMD_READ_EDM_REG,
	TARGET_CMD_WRITE_EDM_REG,
	TARGET_CMD_MULTI_WRITE,
	TARGET_CMD_NUMBER,
};

int send_cmd(enum _target_command a_command);
int handle_packet(const char *packet, int length);

#endif
