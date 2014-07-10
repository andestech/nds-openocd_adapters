#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include "aice_port.h"
#include "aice_usb.h"
#include "nds32_edm.h"
#include "nds32_reg.h"
#include "nds32_insn.h"
#include "log.h"

#define PORTNUM_BURNER     2354
#define PORTNUM_TELNET     4444
#define PORTNUM_TCL        6666
#define PORTNUM_GDB        1111

#define LINE_BUFFER_SIZE 2048

#ifdef __MINGW32__
# include <windows.h>
# include <process.h>
#else
# include <sys/types.h>
# include <sys/wait.h>
#endif


#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#  define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
#  define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

const char *opt_string = "aAb:Bc:C:d:DF:gGhHkKl:L:N:o:O:p:P:r:R:sS:t:T:vx:Xz:Z::";
struct option long_option[] = {
	{"reset-aice", no_argument, 0, 'a'},
	{"no-crst-detect", no_argument, 0, 'A'},
	{"bport", required_argument, 0, 'b'},
	{"boot", no_argument, 0, 'B'},
	{"clock", required_argument, 0, 'c'},
	{"check-times", required_argument, 0, 'C'},
	{"debug", required_argument, 0, 'd'},
	{"unlimited-log", no_argument, 0, 'D'},
	//{"edmv2", no_argument, 0, 'E'},
	{"edm-port-file", required_argument, 0, 'F'},
	{"force-debug", no_argument, 0, 'g'},
	{"enable-global-stop", no_argument, 0, 'G'},
	{"help", no_argument, 0, 'h'},
	{"reset-hold", no_argument, 0, 'H'},
	//{"enable-virtual-hosting", no_argument, 0, 'j'},
	//{"disable-virtual-hosting", no_argument, 0, 'J'},
	{"word-access-mem", no_argument, 0, 'k'},
	{"soft-reset-hold", no_argument, 0, 'K'},
	{"custom-srst", required_argument, 0, 'l'},
	{"custom-trst", required_argument, 0, 'L'},
	{"custom-restart", required_argument, 0, 'N'},
	{"reset-time", required_argument, 0, 'o'},
	{"edm-port-operation", required_argument, 0, 'O'},
	{"port", required_argument, 0, 'p'},
	{"passcode", required_argument, 0, 'P'},
	{"ice-retry", required_argument, 0, 'r'},
	{"resume-seq", required_argument, 0, 'R'},
	{"source", no_argument, 0, 's'},
	{"stop-seq", required_argument, 0, 'S'},
	{"tport", required_argument, 0, 't'},
	{"boot-time", required_argument, 0, 'T'},
	{"version", no_argument, 0, 'v'},
	{"diagnosis", optional_argument, 0, 'x'},
	{"uncnd-reset-hold", no_argument, 0, 'X'},
	{"ace-conf", required_argument, 0, 'z'},
	{"target", required_argument, 0, 'Z'},
	{0, 0, 0, 0}
};

const char *aice_clk_string[] = {
	"30 MHz",
	"15 MHz",
	"7.5 MHz",
	"3.75 MHz",
	"1.875 MHz",
	"937.5 KHz",
	"468.75 KHz",
	"234.375 KHz",
	"48 MHz",
	"24 MHz",
	"12 MHz",
	"6 MHz",
	"3 MHz",
	"1.5 MHz",
	"750 KHz",
	"375 KHz",
	""
};

enum TARGET_TYPE {
	TARGET_V2 = 0,
	TARGET_V3,
	TARGET_V3m,
	TARGET_INVALID,
};

struct MEM_OPERATIONS {
	int address;
	int data;
	int mask;
	int restore;
};

struct EDM_OPERATIONS {
	int reg_no;
	int data;
};

#define MAX_MEM_OPERATIONS_NUM 32

struct MEM_OPERATIONS stop_sequences[MAX_MEM_OPERATIONS_NUM];
struct MEM_OPERATIONS resume_sequences[MAX_MEM_OPERATIONS_NUM];
int stop_sequences_num = 0;
int resume_sequences_num = 0;

#define MAX_EDM_OPERATIONS_NUM 32

struct EDM_OPERATIONS edm_operations[MAX_EDM_OPERATIONS_NUM];
int edm_operations_num = 0;

static char *memory_stop_sequence = NULL;
static char *memory_resume_sequence = NULL;
static char *edm_port_operations = NULL;
static const char *edm_port_op_file = NULL;
static int aice_retry_time = 50;
static int aice_no_crst_detect = 0;
static int clock_setting = 9;
static int debug_level = 3;
static int boot_code_debug;
static int gdb_port[AICE_MAX_NUM_CORE];
static char *gdb_port_str = NULL;
static int burner_port = PORTNUM_BURNER;
static int telnet_port = PORTNUM_TELNET;
static int tcl_port = PORTNUM_TCL;
static int startup_reset_halt = 0;
static int soft_reset_halt;
static int force_debug;
static int unlimited_log;
static int boot_time = 3000;
static int reset_time = 1000;
static int reset_aice_as_startup = 0;
static char *count_to_check_dbger = NULL;
static int global_stop;
static int word_access_mem;
static enum TARGET_TYPE target_type[AICE_MAX_NUM_CORE] = {TARGET_INVALID};
static char *edm_passcode = NULL;
static const char *custom_srst_script = NULL;
static const char *custom_trst_script = NULL;
static const char *custom_restart_script = NULL;
static const char *aceconf_desc_list = NULL;
static int diagnosis = 0;
static int diagnosis_memory = 0;
static unsigned int diagnosis_address = 0;
static uint8_t total_num_of_core = 1;
static unsigned int id_codes[AICE_MAX_NUM_CORE];
extern struct aice_nds32_info core_info[];
extern int nds32_registry_portnum(int port_num);
extern int aice_usb_idcode(uint32_t *idcode, uint8_t *num_of_idcode);
extern int aice_usb_set_edm_passcode(uint32_t coreid, char *edm_passcode);
extern int aice_select_target(uint32_t address, uint32_t data);
extern int aice_reset_aice_as_startup(void);
extern int aice_usb_set_count_to_check_dbger(uint32_t count_to_check);
int aice_usb_set_edm_operations_passcode(uint32_t coreid);
static void parse_edm_operation(const char *edm_operation);
//extern int force_turnon_V3_EDM;
//extern char *BRANCH_NAME, *COMMIT_ID;
extern int nds32_select_memory_mode(uint32_t aice, uint32_t address, uint32_t length);
static void show_version(void) {
	printf("Andes ICEman v3.0.0 (openocd)\n");
	printf("Copyright (C) 2007-2013 Andes Technology Corporation\n");
}

static void show_srccode_ver(void) {
	printf("Branch: %s\n", BRANCH_NAME);
	printf("Commit: %s\n", COMMIT_ID);
}

static void show_usage(void) {
	uint32_t i;
	printf("Usage:\nICEman --port start_port_number[:end_port_number] [--help]\n");
	printf("-a, --reset-aice:\tReset AICE as ICEman startup\n");
	printf("-A, --no-crst-detect:\tNo CRST detection in debug session\n");
	printf("-b, --bport:\t\tSocket port number for Burner connection\n");
	printf("\t\t\t(default: 2354)\n");
	printf("-B, --boot:\t\tReset-and-hold while connecting to target\n");
	printf("-c, --clock:\t\tSpecific JTAG clock setting\n");
	printf("\t\tUsage: -c num\n");
	printf("\t\t\tnum should be the following:\n");
	for (i=0; i<=15; i++)
		printf("\t\t\t%d: %s\n", i, aice_clk_string[i]);

	printf("\t\t\tAICE-MCU only supports 8 ~ 15\n\n");
	printf("-C, --check-times:\tCount/Second to check DBGER\n");
	printf("\t\t\t(default: 30 times)\n");
	printf("\t\tExample:\n");
	printf("\t\t\t1. -C 100 to check 100 times\n");
	printf("\t\t\t2. -C 100s or -C 100S to check 100 seconds\n\n");
	printf("-D, --unlimited-log:\tDo not limit log file size to 512 KB\n");
	//printf("-e, --edm-retry:\tRetry count of getting EDM version as ICEman startup\n");
	printf("-F, --edm-port-file (Only for Secure MPU):\tEDM port0/1 operations file name\n");
	printf("\t\tFile format:\n");
	printf("\t\t\twrite_edm 6:0x1234,7:0x1234;\n");
	printf("\t\t\twrite_edm 6:0x1111;\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n\n");
	//printf("-g, --force-debug: \n");
	printf("-G, --enable-global-stop: Enable 'global stop'.  As users use up hardware watchpoints, target stops at every load/store instructions. \n");
	printf("-h, --help:\t\tThe usage is for ICEman\n");
	printf("-H, --reset-hold:\tReset-and-hold while ICEman startup\n");
	//printf("-j, --enable-virtual-hosting:\tEnable virtual hosting\n");
	//printf("-J, --disable-virtual-hosting:\tDisable virtual hosting\n");
	printf("-k, --word-access-mem:\tAlways use word-aligned address to access device\n");
	printf("-K, --soft-reset-hold:\tUse soft reset-and-hold\n");
	printf("-l, --custom-srst:\tUse custom script to do SRST\n");
	printf("-L, --custom-trst:\tUse custom script to do TRST\n");
	printf("-N, --custom-restart:\tUse custom script to do RESET-HOLD\n");
	//printf("-M, --Mode:\t\tSMP\\AMP Mode(Default: AMP Mode)\n");
	printf("-o, --reset-time:\tReset time of reset-and-hold (milliseconds)\n");
	printf("\t\t\t(default: 1000 milliseconds)\n");
	printf("-O, --edm-port-operation (Only for Secure MPU): EDM port0/1 operations\n");
	printf("\t\tUsage: -O \"write_edm 6:0x1234,7:0x5678;\"\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n\n");
	printf("-p, --port:\t\tSocket port number for gdb connection\n");
	printf("-P, --passcode (Only for Secure MPU):\t\tPASSCODE of secure MPU\n");
	printf("-r, --ice-retry:\tRetry count when AICE command timeout\n");
	printf("\t\t\t(default: 50 times)\n");
	printf("-s, --source:\t\tShow commit ID of this version\n");
	printf("-S, --stop-seq:\t\tSpecify the SOC device operation sequence while CPU stop\n");
	printf("-R, --resume-seq:\tSpecify the SOC device operation sequence before CPU resume\n\n");
	printf("\t\tUsage: --stop-seq A1:D1[:M1],A2:D2[:M2]\n");
	printf("\t\t\t--resume-seq A3:D3[:M3],A2:rst\n");
	printf("\t\t\tA*:address, D*:data, [:M*]:optional mask, rst:restore\n");
	printf("\t\t\tA*,D*,[M*] should be hex as following example\n\n");
	printf("\t\tExample: --stop-seq 0x500000:0x80,0x600000:0x20\n");
	printf("\t\t\t--resume-seq 0x500000:0x80,0x600000:rst\n\n");
	printf("-t, --tport:\t\tSocket port number for Telnet connection\n");
	printf("-T, --boot-time:\tBoot time of target board (milliseconds)\n");
	printf("\t\t\t(default: 3000 milliseconds)\n");
	//printf("-u, --update-fw:\tUpdate AICE F/W\n");
	//printf("-U, --update-fpga:\tUpdate AICE FPGA\n");
	printf("-v, --version:\t\tVersion of ICEman\n");
	//printf("-w, --backup-fw:\tBackup AICE F/W\n");
	//printf("-W, --backup-fpga:\tBackup AICE FPGA\n");
	printf("-x, --diagnosis:\tDiagnose connectivity issue\n");
	printf("\t\tUsage: --diagnosis[=address]\n\n");
	printf("-X, --uncnd-reset-hold:\tUnconditional Reset-and-hold while ICEman startup (This implies -H)\n");
	printf("-z, --ace-conf :\t\tSpecify ACE file on each core\n");
	printf("\t\tUsage: --ace-conf <core#id>=<ace_conf>[,<core#id>=<ace_conf>]*\n");
	printf("\t\t\tExample: --ace-conf core0=core0.aceconf,core1=core1.aceconf\n");
	printf("-Z, --target:\t\tSpecify target type (v2/v3/v3m)\n");
}

static void parse_param(int a_argc, char **a_argv) {
	while(1) {
		int c = 0;
		int option_index;
		int optarg_len;
		char tmpchar = 0;
		uint32_t ms_check_dbger = 0;

		c = getopt_long(a_argc, a_argv, opt_string, long_option, &option_index);
		if (c == EOF)
			break;

		switch (c) {
			case 'a': /* reset-aice */
				reset_aice_as_startup = 1;
				break;
			case 'A': /* no-crst-detect */
				aice_no_crst_detect = 1;
				break;
			case 'b':
				burner_port = strtol(optarg, NULL, 0);
				break;
			case 'B':
				boot_code_debug = 1;
				break;
			case 'c':
				clock_setting = strtol(optarg, NULL, 0);
				if (clock_setting < 0)
					clock_setting = 0;
				if (15 < clock_setting)
					clock_setting = 15;
				break;
			case 'C':
				optarg_len = strlen(optarg);
				count_to_check_dbger = malloc(optarg_len + 1);
				memcpy(count_to_check_dbger, optarg, optarg_len + 1);
				sscanf(count_to_check_dbger, "%u%c", &ms_check_dbger, &tmpchar);
				if (tmpchar == 's' || tmpchar == 'S')
					ms_check_dbger *= 1000;
				aice_usb_set_count_to_check_dbger(ms_check_dbger);
				//printf("aice_count_to_check_dbger %d \n", ms_check_dbger);
				break;
			case 'd':
				debug_level = strtol(optarg, NULL, 0);
				break;
			case 'D':
				unlimited_log = 1;
				debug_level = 3;
				break;
			/*case 'E':
				printf("force_turnon_V3_EDM off \n");
				force_turnon_V3_EDM = 0;
				break;*/
			case 'F':
				edm_port_op_file = optarg;
				break;
			case 'g':
				force_debug = 1;
				break;
			case 'G':
				global_stop = 1;
				break;
			case 'H':
				startup_reset_halt = 1;
				break;
			case 'k':
				word_access_mem = 1;
				break;
			case 'K':
				soft_reset_halt = 1;
				break;
			case 'l': /* customer-srst */
				custom_srst_script = optarg;
				break;
			case 'L': /* customer-trst */
				custom_trst_script = optarg;
				break;
			case 'N': /* customer-restart */
				custom_restart_script = optarg;
				break;
			case 'o': /* reset-time */
				reset_time = strtol(optarg, NULL, 0);
				break;
			case 'O':
				optarg_len = strlen(optarg);
				edm_port_operations = malloc(optarg_len + 1);
				memcpy(edm_port_operations, optarg, optarg_len + 1);
				break;
			case 'p':
				optarg_len = strlen(optarg);
				gdb_port_str = malloc(AICE_MAX_NUM_CORE * 6);
				memcpy(gdb_port_str, optarg, optarg_len + 1);
				break;
			case 'P':
				optarg_len = strlen(optarg);
				edm_passcode = malloc(optarg_len + 1);
				memcpy(edm_passcode, optarg, optarg_len + 1);
				break;
			case 'r':
				aice_retry_time = strtol(optarg, NULL, 0);
				break;
			case 'R':
				optarg_len = strlen(optarg);
				memory_resume_sequence = malloc(optarg_len + 1 + 11); /* +11 for resume-seq */
				memcpy(memory_resume_sequence, "resume-seq ", 11);
				memcpy(memory_resume_sequence + 11, optarg, optarg_len + 1);
				break;
			case 's': /* source */
				show_srccode_ver();
				exit(0);
			case 'S':
				optarg_len = strlen(optarg);
				memory_stop_sequence = malloc(optarg_len + 1 + 9);  /* +9 for stop-seq */
				memcpy(memory_stop_sequence, "stop-seq ", 9);
				memcpy(memory_stop_sequence + 9, optarg, optarg_len + 1);
				break;
			case 't':
				telnet_port = strtol(optarg, NULL, 0);
				break;
			case 'T':
				boot_time = strtol(optarg, NULL, 0);
				break;
			case 'v':
					show_version();
					exit(0);
			case 'x':
				diagnosis = 1;
				if (optarg != NULL){
					diagnosis_memory = 1;
					sscanf(optarg, "0x%x", &diagnosis_address);
				}else
					diagnosis_memory = 0;
				break;
			case 'X': /* uncnd-reset-hold */
				startup_reset_halt = 2;
				break;
			case 'z':
				aceconf_desc_list = optarg;
				break;
			case 'Z':
				optarg_len = strlen(optarg);
				if (strncmp(optarg, "v2", optarg_len) == 0) {
					target_type[0] = TARGET_V2;
				} else if (strncmp(optarg, "v3", optarg_len) == 0) {
					target_type[0] = TARGET_V3;
				} else if (strncmp(optarg, "v3m", optarg_len) == 0) {
					target_type[0] = TARGET_V3m;
				} else {
					target_type[0] = TARGET_INVALID;
				}
				break;
			case 'h':
			case '?':
			default:
					show_usage ();
					exit(0);
		}
	}

	if (optind < a_argc) {
		printf("<-- Unknown argument: %s -->\n", a_argv[optind]);
	}
}

int aice_usb_read_reg(uint32_t coreid, uint32_t num, uint32_t *val);
int aice_edm_init(uint32_t coreid);
int aice_usb_halt(uint32_t coreid);
int aice_usb_run(uint32_t coreid);

static int target_probe(void)
{
	uint16_t vid = 0x1CFC;
	uint16_t pid = 0x0;
	uint32_t value_edmcfg=0, value_cr4=0;
	uint32_t version = 0;
	uint32_t coreid = 0;

	if (target_type[0] != TARGET_INVALID)
		return ERROR_FAIL;

	nds32_reg_init ();

	if (ERROR_OK != aice_usb_open(vid, pid)) {
		printf("<-- Can not open usb -->\n");
		return ERROR_FAIL;
	}
	if (reset_aice_as_startup == 1) {
		aice_reset_aice_as_startup();
	}

	if (ERROR_OK != aice_usb_idcode(&id_codes[0], &total_num_of_core)){
		printf("<-- scan target error -->\n");
		return ERROR_FAIL;
	}

	/* clear timeout status */
	aice_write_ctrl(AICE_WRITE_CTRL_CLEAR_TIMEOUT_STATUS, 0x1);

	if(edm_passcode)
		aice_usb_set_edm_passcode(0, edm_passcode);

	/* login edm operations */
	char line_buffer[LINE_BUFFER_SIZE];
	FILE *edm_operation_fd = NULL;
	if (edm_port_op_file != NULL)
		edm_operation_fd = fopen(edm_port_op_file, "r");
	if (edm_operation_fd != NULL) {
		while (fgets(line_buffer, LINE_BUFFER_SIZE, edm_operation_fd) != NULL) {
			parse_edm_operation(line_buffer);
		}
		fclose(edm_operation_fd);
	}
	if (edm_port_operations) {
		parse_edm_operation(edm_port_operations);
	}
	if (edm_operations_num)
		aice_usb_set_edm_operations_passcode(0);

	for (coreid = 0; coreid < total_num_of_core; coreid++){
		if (ERROR_OK != aice_read_edmsr(coreid, NDS_EDM_SR_EDM_CFG, &value_edmcfg)) {
			printf("<-- Can not read EDMSR -->\n");
			return ERROR_FAIL;
		}

		version = (value_edmcfg >> 16) & 0xFFFF;
		if ((version & 0x1000) == 0) {
			/* edm v2 */
			target_type[coreid] = TARGET_V2;
		} else {
			/* edm v3 */
			/* halt cpu to check whether cpu belongs to mcu not */

			if (ERROR_OK != aice_usb_halt(coreid)) {
				if ((edm_passcode) || (edm_operations_num))
					printf("<-- Can not halt cpu or invalid passcode -->\n");
				else
					printf("<-- Can not halt cpu -->\n");
				return ERROR_FAIL;
			}
			if (ERROR_OK != aice_usb_read_reg(coreid, CR4, &value_cr4)) {
				printf("<-- Can not read register -->\n");
				return ERROR_FAIL;
			}
			if ((value_cr4 & 0x100000) == 0x100000) {
				target_type[coreid] = TARGET_V3m;
			} else {
				target_type[coreid] = TARGET_V3;
			}
			if (ERROR_OK != aice_usb_run(coreid)) {
				printf("<-- Can not release cpu -->\n");
				return ERROR_FAIL;
			}
		}
	}
	aice_usb_close();
	return ERROR_OK;
}

int aice_usb_set_clock(int set_clock);
int aice_core_init(uint32_t coreid);
int aice_issue_reset_hold(uint32_t coreid);
int aice_read_reg(uint32_t coreid, uint32_t num, uint32_t *val);
int aice_write_reg(uint32_t coreid, uint32_t num, uint32_t val);
int aice_usb_memory_access(uint32_t coreid, enum nds_memory_access channel);
int aice_usb_read_memory_unit(uint32_t coreid, uint32_t addr, uint32_t size,
		uint32_t count, uint8_t *buffer);
int aice_usb_write_memory_unit(uint32_t coreid, uint32_t addr, uint32_t size,
		uint32_t count, const uint8_t *buffer);

static void do_diagnosis(void){
	uint16_t vid = 0x1CFC;
	uint16_t pid = 0x0;
	int last_fail_item, item;
	uint32_t value, backup_value, test_value;
	uint32_t selected_core;
	uint32_t hardware_version, firmware_version, fpga_version;
	uint32_t scan_clock, scan_base_freq, scan_freq;
	uint8_t test_memory_value, memory_value, backup_memory_value;
	int aice_is_open=0;
	
	enum{
		CONFIRM_USB_CONNECTION,
		CONFIRM_AICE_VERSIONS,
		CONFIRM_JTAG_FREQUENCY_DETECTED,
		CONFIRM_SET_JTAG_FREQUENCY,
		CONFIRM_JTAG_CONNECTIVITY,
		CONFIRM_JTAG_DOMAIN,
		CONFIRM_TRST_WORKING,
		CONFIRM_SRST_NOT_AFFECT_JTAG,
		CONFIRM_SELECT_CORE,
		CONFIRM_RESET_HOLD,
		CONFIRM_DIM_AND_CPU_DOMAIN,
		CONFIRM_MEMORY_ON_BUS,
		CONFIRM_MEMORY_ON_CPU,
		CONFIRM_END
	};

	char *confirm_messages[]={
		[CONFIRM_USB_CONNECTION] = "confirm USB connection ...",
		[CONFIRM_AICE_VERSIONS] = "confirm AICE versions ...",
		[CONFIRM_JTAG_FREQUENCY_DETECTED] = "confirm JTAG frequency detected ...",
		[CONFIRM_SET_JTAG_FREQUENCY] = "confirm Set JTAG frequency ...",
		[CONFIRM_JTAG_CONNECTIVITY] = "confirm JTAG connectivity ...",
		[CONFIRM_JTAG_DOMAIN] = "confirm JTAGS domain working ...",
		[CONFIRM_TRST_WORKING] = "confirm TRST working ...",
		[CONFIRM_SRST_NOT_AFFECT_JTAG] = "confirm SRST not affect JTAG domain ...",
		[CONFIRM_SELECT_CORE] = "confirm Select core success ...",
		[CONFIRM_RESET_HOLD] = "confirm Reset and debug success ...",
		[CONFIRM_DIM_AND_CPU_DOMAIN] = "confirm DIM and CPU domain operational ...",
		[CONFIRM_MEMORY_ON_BUS] = "confirm memory access on BUS mode ...",
		[CONFIRM_MEMORY_ON_CPU] = "confirm memory access on CPU mode ..."
	};

	if (target_type[0] != TARGET_INVALID)
		return;

	nds32_reg_init();

	printf("********************\n");
	printf("Diagnostic Report\n");
	printf("********************\n");

	/* 1. confirm USB connection */
	last_fail_item = CONFIRM_USB_CONNECTION;
	if (ERROR_OK != aice_usb_open(vid, pid))
		goto report;
	aice_is_open=1;

	/* clear timeout status */
	if (aice_write_ctrl(AICE_WRITE_CTRL_CLEAR_TIMEOUT_STATUS, 0x1) != ERROR_OK)
		goto report;

	/* clear NO_DBGI_PIN */
	uint32_t pin_status;
	if (aice_read_ctrl(AICE_READ_CTRL_GET_JTAG_PIN_STATUS, &pin_status) != ERROR_OK)
		goto report;

	if (aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_STATUS, pin_status & (~0x02)) != ERROR_OK)
		goto report;

	/* get hardware and firmware version */
	if (aice_read_ctrl(AICE_READ_CTRL_GET_HARDWARE_VERSION, &hardware_version) != ERROR_OK)
		goto report;

	if (aice_read_ctrl(AICE_READ_CTRL_GET_FIRMWARE_VERSION, &firmware_version) != ERROR_OK)
		goto report;

	if (aice_read_ctrl(AICE_READ_CTRL_GET_FPGA_VERSION, &fpga_version) != ERROR_OK)
		goto report;

	printf("AICE version: hw_ver = 0x%x, fw_ver = 0x%x, fpga_ver = 0x%x\n",
			hardware_version, firmware_version, fpga_version);

	/* 2. Report JTAG frequency detected */
	if (aice_read_ctrl(AICE_READ_CTRL_GET_ICE_STATE, &scan_clock) != ERROR_OK)
		goto report;

	scan_clock &= 0x0F;
	if (scan_clock & 0x8)
		scan_base_freq = 48000; /* 48 MHz */
	else
		scan_base_freq = 30000; /* 30 MHz */

	scan_freq = scan_base_freq >> (scan_clock & 0x7);
	printf("JTAG frequency %u Hz\n",scan_freq);

	/* 3. [Optional] set user specified JTAG frequency */
	last_fail_item = CONFIRM_SET_JTAG_FREQUENCY;
	if (ERROR_OK != aice_usb_set_clock(9))
		goto report;

	printf("ice_state = %08x\n", scan_clock);
	/* 4. Report JTAG scan chain (confirm JTAG connectivity) */
	last_fail_item = CONFIRM_JTAG_CONNECTIVITY;
	if (ERROR_OK != aice_usb_idcode(&id_codes[0], &total_num_of_core))
		goto report;
	printf("There %s %u %s in target\n", total_num_of_core > 1 ? "are":"is", total_num_of_core, total_num_of_core > 1 ? "cores":"core");
	int i=0;
	for(i = 0; i < total_num_of_core; i++){
		selected_core = i;
		aice_select_target(i,id_codes[i]);

		/* 5. Read/write SBAR: confirm JTAG domain working */
		last_fail_item = CONFIRM_JTAG_DOMAIN;
		test_value = rand() & 0xFFFFFD;

		aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &backup_value);
		aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, test_value);
		aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &value);
		if (value != test_value)
			goto report;

		/* 5.a */
		last_fail_item = CONFIRM_TRST_WORKING;
		aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, 0);
		aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_TRST);
		aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &value);
		if(value != 0x1)
			goto report;

		/* 5.b */
		last_fail_item = CONFIRM_SRST_NOT_AFFECT_JTAG;
		aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, 0);
		aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_SRST);
		aice_read_misc(selected_core, NDS_EDM_MISC_SBAR, &value);
		if(value != 0x0)
			goto report;

		/* restore SBAR */
		aice_write_misc(selected_core, NDS_EDM_MISC_SBAR, backup_value);

		/* 6. Select first Andes core based on scan chain */
		last_fail_item = CONFIRM_SELECT_CORE;
		aice_core_init(selected_core);
		if (ERROR_OK != aice_edm_init(selected_core))
			goto report;
		printf("Core #%u: EDM version 0x%x\n", selected_core, core_info[selected_core].edm_version);

		/* 7. Restart target (debug and reset)*/
		last_fail_item = CONFIRM_RESET_HOLD;
		/* clear DBGER */
		if (aice_write_misc(selected_core, NDS_EDM_MISC_DBGER, NDS_DBGER_CLEAR_ALL) != ERROR_OK)
			goto report;

		if (ERROR_OK != aice_issue_reset_hold(selected_core))
			goto report;
		printf("hardware reset-and-hold success\n");

		/* 8. Test read/write R0: confirm DIM and CPU domain operational */
		last_fail_item = CONFIRM_DIM_AND_CPU_DOMAIN;
		test_value = rand();
		aice_read_reg(selected_core, R0, &backup_value);
		aice_write_reg(selected_core, R0, test_value);
		aice_read_reg(selected_core, R0, &value);
		if (value != test_value)
			goto report;

		aice_write_reg(selected_core, R0, backup_value);

		if(diagnosis_memory){
			aice_read_reg(selected_core, CR1, &value);
			if ((value >> 10) & 0x7) { /* ILMB exists */
				aice_read_reg(selected_core, MR6, &backup_value);
				backup_value |= 0x01;
				aice_write_reg(selected_core, MR6, backup_value);
				//aice_read_reg(selected_core, MR6, &backup_value);
				//printf("MR6-value %x\n", backup_value);
			}

			aice_read_reg(selected_core, CR2, &value);
			if ((value >> 10) & 0x7) { /* DLMB exists */
				aice_read_reg(selected_core, MR7, &backup_value);
				backup_value |= 0x01;
				aice_write_reg(selected_core, MR7, backup_value);
				//aice_read_reg(selected_core, MR7, &backup_value);
				//printf("MR7-value %x\n", backup_value);
			}
			/* 9. Test direct memory access (bus view) confirm BUS working */
			last_fail_item = CONFIRM_MEMORY_ON_BUS;
			test_memory_value = rand();
			aice_usb_memory_access(selected_core, NDS_MEMORY_ACC_BUS);
			nds32_select_memory_mode(selected_core, diagnosis_address, 1);
			aice_usb_read_memory_unit(selected_core, diagnosis_address, 1, 1, &backup_memory_value);
			aice_usb_write_memory_unit(selected_core, diagnosis_address, 1, 1, &test_memory_value);
			aice_usb_read_memory_unit(selected_core, diagnosis_address, 1, 1, &memory_value);
			if (memory_value != test_memory_value)
				goto report;

			/* 10. Test memory access through DIM (CPU view) reconfirm CPU working */
			last_fail_item = CONFIRM_MEMORY_ON_CPU;
			test_memory_value = rand();
			aice_usb_memory_access(selected_core, NDS_MEMORY_ACC_CPU);
			nds32_select_memory_mode(selected_core, diagnosis_address, 1);
			aice_usb_read_memory_unit(selected_core, diagnosis_address, 1, 1, &backup_memory_value);
			aice_usb_write_memory_unit(selected_core, diagnosis_address, 1, 1, &test_memory_value);
			aice_usb_read_memory_unit(selected_core, diagnosis_address, 1, 1, &memory_value);
			if (memory_value != test_memory_value)
				goto report;

			aice_usb_write_memory_unit(selected_core, diagnosis_address, 1, 1, &backup_memory_value);
		}

		last_fail_item = CONFIRM_END;

	report:
		printf("********************\n");
		for (item = CONFIRM_USB_CONNECTION; item < last_fail_item; item++){
			if(!diagnosis_memory && (item == CONFIRM_MEMORY_ON_BUS || item == CONFIRM_MEMORY_ON_CPU))
				continue;
			printf("[PASS] %s\n", confirm_messages[item]);
		}
		if(last_fail_item != CONFIRM_END)
			printf("[FAIL] %s\n", confirm_messages[last_fail_item]);
		printf("********************\n");
	}
	if(aice_is_open)
	  aice_usb_close();
}


static char *clock_hz[] = {
	"30000",
	"15000",
	"7500",
	"3750",
	"1875",
	"937",
	"468",
	"234",
	"48000",
	"24000",
	"12000",
	"6000",
	"3000",
	"1500",
	"750",
	"375",
};

static FILE *openocd_cfg_tpl;
static FILE *openocd_cfg;
static FILE *interface_cfg_tpl;
static FILE *interface_cfg;
static FILE *board_cfg_tpl;
static FILE *board_cfg;
static FILE *target_cfg_tpl;
static FILE *target_cfg[AICE_MAX_NUM_CORE];

static void open_config_files(void) {
	openocd_cfg_tpl = fopen("openocd.cfg.tpl", "r");
	openocd_cfg = fopen("openocd.cfg", "w");
	if ((openocd_cfg_tpl == NULL) || (openocd_cfg == NULL)) {
		fprintf(stderr, "ERROR: No config file, openocd.cfg\n");
		exit(-1);
	}

	interface_cfg_tpl = fopen("interface/nds32-aice.cfg.tpl", "r");
	interface_cfg = fopen("interface/nds32-aice.cfg", "w");
	if ((interface_cfg_tpl == NULL) || (interface_cfg == NULL)) {
		fprintf(stderr, "ERROR: No interface config file, nds32-aice.cfg\n");
		exit(-1);
	}

	board_cfg_tpl = fopen("board/nds32_xc5.cfg.tpl", "r");
	board_cfg = fopen("board/nds32_xc5.cfg", "w");
	if ((board_cfg_tpl == NULL) || (board_cfg == NULL)) {
		fprintf(stderr, "ERROR: No board config file, nds32_xc5.cfg\n");
		exit(-1);
	}

	target_cfg_tpl = fopen("target/nds32.cfg.tpl", "r");

	int coreid;
	char *target_str = NULL;
	char line_buffer[LINE_BUFFER_SIZE];

	for (coreid = 0; coreid < total_num_of_core; coreid++){
		if (target_type[coreid] == TARGET_V3) {
			target_str = "target/nds32v3_%d.cfg";
		} else if (target_type[coreid] == TARGET_V2) {
			target_str = "target/nds32v2_%d.cfg";
		} else if (target_type[coreid] == TARGET_V3m) {
			target_str = "target/nds32v3m_%d.cfg";
		} else {
			fprintf(stderr, "No target specified\n");
			exit(0);
		}
		sprintf(line_buffer, target_str, coreid);
		target_cfg[coreid] = fopen(line_buffer, "w");
	}
}

static void close_config_files(void) {
	int coreid;

	fclose(openocd_cfg_tpl);
	fclose(openocd_cfg);
	fclose(interface_cfg_tpl);
	fclose(interface_cfg);
	fclose(board_cfg_tpl);
	fclose(board_cfg);
	for (coreid = 0; coreid < total_num_of_core; coreid ++)
		fclose(target_cfg[coreid]);
}

static void parse_mem_operation(const char *mem_operation) {
	char *next_data;

	if (strncmp(mem_operation, "stop-seq ", 9) == 0) {
		stop_sequences[stop_sequences_num].address = strtoll(mem_operation+9, &next_data, 0);
		stop_sequences[stop_sequences_num].data = strtoll(next_data+1, &next_data, 0);
		if (*next_data == ':')
			stop_sequences[stop_sequences_num].mask = strtoll(next_data+1, &next_data, 0);
		stop_sequences_num++;
	} else if (strncmp(mem_operation, "resume-seq ", 11) == 0) {
		resume_sequences[resume_sequences_num].address = strtoll(mem_operation+11, &next_data, 0);
		if (*(next_data+1) == 'r')
			resume_sequences[resume_sequences_num].restore = 1;
		else {
			resume_sequences[resume_sequences_num].restore = 0;
			resume_sequences[resume_sequences_num].data = strtoll(next_data+1, &next_data, 0);
			if (*next_data == ':')
				resume_sequences[resume_sequences_num].mask = strtoll(next_data+1, &next_data, 0);
		}
		resume_sequences_num++;
	}
}

/* edm_operation format: write_edm 6:0x1234,7:0x5678; */
static void parse_edm_operation(const char *edm_operation) {
	char *processing_str;
	char *end_ptr;
	int reg_no;
	int value;

	processing_str = strstr(edm_operation, "write_edm ");
	if (processing_str == NULL)
		return;
	processing_str += 10;

	while (1) {
		reg_no = strtoll(processing_str, &end_ptr, 0);
		if (end_ptr[0] != ':')
			return;
		processing_str = end_ptr + 1;
		value = strtoll(processing_str, &end_ptr, 0);

		edm_operations[edm_operations_num].reg_no = reg_no;
		edm_operations[edm_operations_num].data = value;
		edm_operations_num++;

		if (end_ptr[0] == ';')
			break;
		else if (end_ptr[0] == ',')
			processing_str = end_ptr + 1;
	}
}


#ifdef __MINGW32__
static HANDLE adapter_pipe_input[2];
static PROCESS_INFORMATION burner_proc_info;
static PROCESS_INFORMATION openocd_proc_info;
#else
static pid_t burner_adapter_pid;
static pid_t openocd_pid;
static int adapter_pipe_input[2];
#endif

#define LOG_BUFFER_SIZE 512*1024
static char *log_buffer;
static int is_rewind = 0;
static unsigned int log_buffer_start = 0;

static void dump_debug_log ()
{
	FILE *debug_log =  fopen("iceman_debug.log", "w+");
	if (debug_log == NULL) {
		printf("ERROR! Cannot create iceman debug log file.\n");
		debug_log = stderr;
	}
	if (is_rewind == 0) {
		fwrite(log_buffer, 1, log_buffer_start, debug_log);
	} else {
		fwrite(log_buffer + log_buffer_start, 1,  LOG_BUFFER_SIZE - log_buffer_start, debug_log);
		fwrite(log_buffer, 1,  log_buffer_start, debug_log);
	}
	fclose(debug_log);

}

#ifdef __MINGW32__
static void sig_int(int UNUSED(signo))
{
	WaitForSingleObject(burner_proc_info.hProcess, INFINITE);
	WaitForSingleObject(openocd_proc_info.hProcess, INFINITE);

	CloseHandle(burner_proc_info.hProcess);
	CloseHandle(burner_proc_info.hThread);
	CloseHandle(openocd_proc_info.hProcess);
	CloseHandle(openocd_proc_info.hThread);

	if (!unlimited_log)
		dump_debug_log();

	exit(0);
}
#else
static void sig_pipe(int UNUSED(signo))
{
	kill(burner_adapter_pid, SIGTERM);
	exit(1);
}

static void sig_int(int UNUSED(signo))
{
	int burner_adapter_status;
	int openocd_status;

	if (waitpid(burner_adapter_pid, &burner_adapter_status, 0) < 0)
		fprintf(stderr, "wait burner_adapter failed\n");

	if (waitpid(openocd_pid, &openocd_status, 0) < 0)
		fprintf(stderr, "wait openocd failed\n");

	if (!unlimited_log)
		dump_debug_log();

	exit(0);
}
#endif

static void process_openocd_message(void)
{
	char line_buffer[LINE_BUFFER_SIZE+2];
	char *search_str;
	FILE *debug_log = NULL;
	int is_ready = 0;
	unsigned int line_buffer_len, log_buffer_remain;
	int coreid;
	int clock_index = 0;

	if (unlimited_log) {
		debug_log =  fopen("iceman_debug.log", "w+");
		if (debug_log == NULL) {
			printf("ERROR! Cannot create iceman debug log file.\n");
			debug_log = stderr;
		}
	} else {
		log_buffer = (char *)malloc(sizeof(char) * LOG_BUFFER_SIZE);
	}

	printf("Andes ICEman V3.0.0 (OpenOCD)\n");
	printf("There are %d cores in target\n", total_num_of_core);

	for(coreid = 0; coreid < total_num_of_core; coreid++){
		printf("Core #%u: EDM version 0x%x\n", coreid, core_info[coreid].edm_version);
	}

	for(coreid = 0; coreid < total_num_of_core; coreid++){
		printf("The core #%d listens on %d.\n", coreid, gdb_port[coreid]);
	}

	printf("Burner listens on %d\n", burner_port);
	printf("Telnet port: %d\n", telnet_port);
	printf("TCL port: %d\n", tcl_port);
//	printf("core_info[coreid].access_channel = %d\n", core_info[0].access_channel);
#ifdef __MINGW32__
	/* ReadFile from pipe is not LINE_BUFFERED. So, we process newline ourselves. */
	DWORD had_read;
	char buffer[LINE_BUFFER_SIZE+2];
	char *newline_pos;
	DWORD line_buffer_index;
	long long unprocessed_len;
	char *unprocessed_buf;

	/* init unprocessed buffer */
	while(1)
	{
	  if (ReadFile(adapter_pipe_input[0], buffer, LINE_BUFFER_SIZE, &had_read, NULL) != 0)
	  {
	  		if(had_read > 0)
	        break;
	  }
	}
	buffer[had_read] = '\0';
	unprocessed_buf = buffer;
	unprocessed_len = had_read;
	line_buffer_index = 0;

	/*while (unprocessed_len >= 0) {*/
	while (1) {
		/* find '\n' */
		newline_pos = strchr(unprocessed_buf, '\n');
		if (newline_pos == NULL) {
			/* if line_buffer is full, skip data */
			if ((line_buffer_index + strlen(unprocessed_buf)) > LINE_BUFFER_SIZE) {
			}
			else {
				/* cannot find '\n', copy whole buffer to line_buffer */
				strcpy(line_buffer + line_buffer_index, unprocessed_buf);
				line_buffer_index += strlen(unprocessed_buf);
			}
			while(1)
			{
			  if (ReadFile(adapter_pipe_input[0], buffer, LINE_BUFFER_SIZE, &had_read, NULL) != 0)
			  {
			    if(had_read > 0)
	          break;
	      }
			}
			buffer[had_read] = '\0';
			unprocessed_buf = buffer;
			unprocessed_len = had_read;
			continue;
		} else {
			/* found '\n', copy substring before '\n' to line_buffer */
			strncpy(line_buffer + line_buffer_index, unprocessed_buf, newline_pos - unprocessed_buf);
			line_buffer_index += (newline_pos - unprocessed_buf);
			line_buffer[line_buffer_index] = '\n';
			line_buffer[line_buffer_index + 1] = '\0';
			line_buffer_index++;
			/* line_buffer is ready */

			/* update unprocessed_buf */
			unprocessed_buf = newline_pos + 1;
			unprocessed_len = (buffer + had_read) - unprocessed_buf;
		}
#else
	/*while (fgets(line_buffer, LINE_BUFFER_SIZE, stdin) != NULL) {*/
	while (1) {
		while (fgets(line_buffer, LINE_BUFFER_SIZE, stdin) == NULL)
		{
			;
		}
#endif
		if (is_ready == 0) {
			if ((search_str = strstr(line_buffer, "AICE-clock-index")) != NULL) {
				sscanf(search_str + 17, "%d", &clock_index);
				if (clock_index < 0) {
					printf("JTAG frequency %s cannot be correctly set.\n", aice_clk_string[clock_setting]);
					fflush(stdout);
					return;
				}
				else {
					printf("JTAG frequency %s\n", aice_clk_string[clock_setting]);
					fflush(stdout);
				}
			}
			else if (startup_reset_halt) {
				if (((search_str = strstr(line_buffer, "software reset-and-hold success")) != NULL) ||
						((search_str = strstr(line_buffer, "hardware reset-and-hold success")) != NULL)) {
					printf("%s", search_str);
					fflush(stdout);
					is_ready = 1;
				}
			}
			else if ((search_str = strstr(line_buffer, "EDM version")) != NULL) {
				fflush(stdout);
				if (startup_reset_halt == 0)
					is_ready = 1;
			}
			if (is_ready == 1) {
				printf("ICEman is ready to use.\n");
				fflush(stdout);
			}
		} else {
			if ((search_str = strstr(line_buffer, "<--")) != NULL) {
				printf("%s", search_str);
				fflush(stdout);
			}
		}

		if (unlimited_log) {
			/* output to debug log */
			fprintf(debug_log, "%s", line_buffer);
			fflush(debug_log);
		} else {
			line_buffer_len = strlen(line_buffer);
			log_buffer_remain = LOG_BUFFER_SIZE - log_buffer_start;
			if (line_buffer_len <= log_buffer_remain) {
				/* there is enough space for this line */
				strncpy(log_buffer + log_buffer_start, line_buffer, line_buffer_len);
				log_buffer_start += line_buffer_len;
				if (log_buffer_start == LOG_BUFFER_SIZE)
				{
					log_buffer_start = 0;
					is_rewind = 1;
				}
			} else {
				/* split this line into two part */
				strncpy(log_buffer + log_buffer_start, line_buffer, log_buffer_remain);
				strncpy(log_buffer, line_buffer + log_buffer_remain, line_buffer_len - log_buffer_remain);
				log_buffer_start = 0 + line_buffer_len - log_buffer_remain;
				is_rewind = 1;
			}
		}

#ifdef __MINGW32__
		line_buffer_index = 0;
#endif
		/* TODO: use pin-pon buffer to log debug messages */
	}
}

char *str_replace ( const char *string, const char *substr, const char *replacement ){
	char *tok = NULL;
	char *newstr = NULL;
	char *oldstr = NULL;
	char *head = NULL;

	/* if either substr or replacement is NULL, duplicate string a let caller handle it */
	if ( substr == NULL || replacement == NULL ) return strdup (string);
	newstr = strdup (string);
	head = newstr;
	while ( (tok = strstr ( head, substr ))){
		oldstr = newstr;
		newstr = malloc ( strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) + 1 );
		/*failed to alloc mem, free old string and return NULL */
		if ( newstr == NULL ){
			free (oldstr);
			return NULL;
		}
		memcpy ( newstr, oldstr, tok - oldstr );
		memcpy ( newstr + (tok - oldstr), replacement, strlen ( replacement ) );
		memcpy ( newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ), strlen ( oldstr ) - strlen ( substr ) - ( tok - oldstr ) );
		memset ( newstr + strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) , 0, 1 );
		/* move back head right after the last replacement */
		head = newstr + (tok - oldstr) + strlen( replacement );
		free (oldstr);
	}
	return newstr;
}

static void update_gdb_port_num(void)
{
	int port_num = 0;

	if(gdb_port_str) {
		char *port = strtok(gdb_port_str, ":");
		while (port != NULL) {
			gdb_port[port_num] = strtol(port, NULL, 0);
			port = strtok (NULL, ":");
			port_num++;
		}
	}else{
		gdb_port[0] = PORTNUM_GDB;
	}
	if (port_num < total_num_of_core)
		total_num_of_core = port_num;
}

static void update_openocd_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];

	/* update openocd.cfg */
	while (fgets(line_buffer, LINE_BUFFER_SIZE, openocd_cfg_tpl) != NULL)
		fputs(line_buffer, openocd_cfg);

	fprintf(openocd_cfg, "gdb_port %d\n", gdb_port[0]);
	fprintf(openocd_cfg, "telnet_port %d\n", telnet_port);
	fprintf(openocd_cfg, "tcl_port %d\n", tcl_port);
	fprintf(openocd_cfg, "debug_level %d\n", debug_level);

	if (global_stop)
		fprintf(openocd_cfg, "nds global_stop on\n");
	else
		fprintf(openocd_cfg, "nds global_stop off\n");

	if (startup_reset_halt == 1)
		fprintf(openocd_cfg, "nds reset_halt_as_init on\n");
	else if (startup_reset_halt == 2)
		fprintf(openocd_cfg, "nds reset_halt_as_init uncnd\n");

	fprintf(openocd_cfg, "nds boot_time %d\n", boot_time);
	fprintf(openocd_cfg, "nds reset_time %d\n", reset_time);

	if (word_access_mem)
		fprintf(openocd_cfg, "nds word_access_mem on\n");
}

static void update_interface_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];

	/* update nds32-aice.cfg */
	while (fgets(line_buffer, LINE_BUFFER_SIZE, interface_cfg_tpl) != NULL)
		fputs(line_buffer, interface_cfg);

	fprintf(interface_cfg, "adapter_khz %s\n", clock_hz[clock_setting]);
	fprintf(interface_cfg, "aice retry_times %d\n", aice_retry_time);
	fprintf(interface_cfg, "aice no_crst_detect %d\n", aice_no_crst_detect);

	if (count_to_check_dbger)
		fprintf(interface_cfg, "aice count_to_check_dbger %s\n", count_to_check_dbger);
	else
		fprintf(interface_cfg, "aice count_to_check_dbger 30\n");

	/* custom srst/trst/restart scripts */
	if (custom_srst_script) {
		fprintf(interface_cfg, "aice custom_srst_script %s\n", custom_srst_script);
	}
	if (custom_trst_script) {
		fprintf(interface_cfg, "aice custom_trst_script %s\n", custom_trst_script);
	}
	if (custom_restart_script) {
		fprintf(interface_cfg, "aice custom_restart_script %s\n", custom_restart_script);
	}
	fputs("\n", interface_cfg);
}

static void update_target_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];
	char *line_buffer_tmp;
	int coreid;
	char *arch_str[3] = {"v2", "v3", "v3m"};
	char coreid_str[3];
	for (coreid = 0; coreid < total_num_of_core; coreid ++){
		fseek(target_cfg_tpl, 0, SEEK_SET);
		sprintf(coreid_str, "%d", coreid);
		while (fgets(line_buffer, LINE_BUFFER_SIZE, target_cfg_tpl) != NULL){
			line_buffer_tmp = str_replace(line_buffer, "<_TARGETID>", coreid_str);
			line_buffer_tmp = str_replace(line_buffer_tmp, "<_TARGET_ARCH>", arch_str[target_type[coreid]]);
			if (boot_code_debug)
				line_buffer_tmp = str_replace(line_buffer_tmp, "<--boot>", "reset halt");
			fputs(line_buffer_tmp, target_cfg[coreid]);
		}
		fputs("\n", target_cfg[coreid]);
	}
}

#define MAX_LEN_ACECONF_NAME 2048
#define TOSTR(x)	#x
#define XTOSTR(x)	TOSTR(x)
static void update_board_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];
	const char *aceconf_desc;
	unsigned int core_id =0, read_byte = 0;
	char aceconf[MAX_LEN_ACECONF_NAME + 1];
	int ret;
	int coreid;

	/* update nds32_xc5.cfg */
	char *find_pos;
	char *target_str = NULL;
	while (fgets(line_buffer, LINE_BUFFER_SIZE, board_cfg_tpl) != NULL) {
		if ((find_pos = strstr(line_buffer, "--target")) != NULL) {
			for (coreid = 0; coreid < total_num_of_core; coreid++){
				if(target_str)
					fputs(line_buffer, board_cfg);
				if (target_type[coreid] == TARGET_V3) {
					target_str = "source [find target/nds32v3_%d.cfg]\n";
				} else if (target_type[coreid] == TARGET_V2) {
					target_str = "source [find target/nds32v2_%d.cfg]\n";
				} else if (target_type[coreid] == TARGET_V3m) {
					target_str = "source [find target/nds32v3m_%d.cfg]\n";
				} else {
					fprintf(stderr, "No target specified\n");
					exit(0);
				}
				sprintf(line_buffer, target_str, coreid);
			}
		} else if ((find_pos = strstr(line_buffer, "--edm-passcode")) != NULL) {
			if (edm_passcode != NULL) {
				sprintf(line_buffer, "nds login_edm_passcode %s\n", edm_passcode);
			} else {
				strcpy(line_buffer, "\n");
			}
		} else if ((find_pos = strstr(line_buffer, "--soft-reset-halt")) != NULL) {
			if (soft_reset_halt) {
				strcpy(line_buffer, "nds soft_reset_halt on\n");
			} else {
				strcpy(line_buffer, "nds soft_reset_halt off\n");
			}
		} else if ((find_pos = strstr(line_buffer, "--ace-conf")) != NULL) {
			strcpy(line_buffer, "set _ACE_CONF \"\"\n");
			if (aceconf_desc_list) {
				aceconf_desc = aceconf_desc_list;
				while (1) {
					aceconf[0] = '\0';
					core_id = 0;
					ret = 0;

					ret = sscanf(aceconf_desc, "core%u=%" XTOSTR(MAX_LEN_ACECONF_NAME) "[^,]%n",
									&core_id, aceconf, &read_byte);
					if (ret != 2) {
						printf("<-- Can not parse --ace-conf argument '%s'\n. -->", aceconf_desc);
						break;
					}

					/* TODO: support multi core */
					sprintf(line_buffer, "set _ACE_CONF %s\n", aceconf);
					aceconf_desc += read_byte;	/* aceconf points to ',' or '\0' */
					if (*aceconf_desc == '\0')
						break;
					else
						aceconf_desc += 1;	/* point to the one next to ',' */
				}
			}
		}

		fputs(line_buffer, board_cfg);
	}
	fputs("\n", board_cfg);

	/* login edm operations */
	FILE *edm_operation_fd = NULL;
	int i;
	if (edm_port_op_file != NULL)
		edm_operation_fd = fopen(edm_port_op_file, "r");
	if (edm_operation_fd != NULL) {
		while (fgets(line_buffer, LINE_BUFFER_SIZE, edm_operation_fd) != NULL) {
			parse_edm_operation(line_buffer);
		}
		fclose(edm_operation_fd);
	}
	if (edm_port_operations) {
		parse_edm_operation(edm_port_operations);
	}

	if (edm_operations_num > 0) {
		for (i = 0 ; i < edm_operations_num ; i++) {
			fprintf(board_cfg, "nds32.cpu0 nds login_edm_operation 0x%x 0x%x\n", edm_operations[i].reg_no, edm_operations[i].data);
		}
	}

	/* open sw-reset-seq.txt */
	FILE *sw_reset_fd = NULL;
	sw_reset_fd = fopen("sw-reset-seq.txt", "r");
	if (sw_reset_fd != NULL) {
		while (fgets(line_buffer, LINE_BUFFER_SIZE, sw_reset_fd) != NULL) {
			parse_mem_operation(line_buffer);
		}
		fclose(sw_reset_fd);
	}
	if (memory_stop_sequence) {
		parse_mem_operation(memory_stop_sequence);
	}
	if (memory_resume_sequence) {
		parse_mem_operation(memory_resume_sequence);
	}

	for (i = 0 ; i < stop_sequences_num ; i++) {
		fprintf(board_cfg, "set backup_value_%x \"\"\n", stop_sequences[i].address);
	}

	for (coreid = 0; coreid < total_num_of_core; coreid++) {
		if (stop_sequences_num > 0) {
			fprintf(board_cfg, "$_TARGETNAME%d configure -event halted {\n", coreid);
			fprintf(board_cfg, "\t$_TARGETNAME%d nds mem_access bus\n", coreid);
			for (i = 0 ; i < stop_sequences_num ; i++) {
				int stop_addr, stop_mask, stop_data;
				stop_addr = stop_sequences[i].address;
				stop_mask = stop_sequences[i].mask;
				stop_data = stop_sequences[i].data;
				fprintf(board_cfg, "\tglobal backup_value_%x\n", stop_addr);
				fprintf(board_cfg, "\tmem2array backup_value_%x 32 0x%x 1\n", stop_addr, stop_addr);
				fprintf(board_cfg, "\tset masked_value [expr $backup_value_%x(0) & 0x%x | 0x%x]\n", stop_addr, stop_mask, (stop_data & ~stop_mask));
				fprintf(board_cfg, "\tmww 0x%x $masked_value\n", stop_addr);
			}
			fprintf(board_cfg, "\t$_TARGETNAME%d nds mem_access cpu\n", coreid);
			fputs("}\n", board_cfg);
		}
		if (resume_sequences_num > 0) {
			fprintf(board_cfg, "$_TARGETNAME%d configure -event resumed {\n", coreid);
			fprintf(board_cfg, "\t$_TARGETNAME%d nds mem_access bus\n", coreid);
			for (i = 0 ; i < resume_sequences_num ; i++) {
				int resume_addr, resume_mask, resume_data;
				resume_addr = resume_sequences[i].address;
				resume_mask = resume_sequences[i].mask;
				resume_data = resume_sequences[i].data;
				if (resume_sequences[i].restore) {
					fprintf(board_cfg, "\tglobal backup_value_%x\n", resume_addr);
					fprintf(board_cfg, "\tmww 0x%x $backup_value_%x(0)\n", resume_addr, resume_addr);
				} else {
					fprintf(board_cfg, "\tmem2array backup_value_%x 32 0x%x 1\n", resume_addr, resume_addr);
					fprintf(board_cfg, "\tset masked_value [expr $backup_value_%x(0) & 0x%x | 0x%x]\n", resume_addr, resume_mask, (resume_data & ~resume_mask));
					fprintf(board_cfg, "\tmww 0x%x $masked_value\n", resume_addr);
				}
			}
			fprintf(board_cfg, "\t$_TARGETNAME%d nds mem_access cpu\n", coreid);
			fputs("}\n", board_cfg);
		}
	}
}

int create_burner_adapter(void) {
#ifdef __MINGW32__
	BOOL success;
	char cmd_string[256];

	sprintf(cmd_string, "./burner_adapter.exe -p %d -t %d", burner_port, tcl_port);

	STARTUPINFO burner_start_info;

	ZeroMemory(&burner_proc_info, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&burner_start_info, sizeof(STARTUPINFO));
	burner_start_info.cb = sizeof(STARTUPINFO);
	burner_start_info.dwFlags |= STARTF_USESTDHANDLES;

	success = CreateProcess(NULL,
			cmd_string,
			NULL,
			NULL,
			TRUE,
			0,
			NULL,
			NULL,
			&burner_start_info,
			&burner_proc_info);

	if (!success) {
		fprintf(stderr, "Create new process(burner) failed.\n");
		return -1;
	}
#else
	char *burner_adapter_argv[6] = {0, 0, 0, 0, 0, 0};
	burner_adapter_pid = fork();
	if (burner_adapter_pid < 0) {
		fprintf(stderr, "invoke burner_adapter failed\n");
		return -1;
	} else if (burner_adapter_pid == 0) {
		/* invoke burner adapter */
		char burner_port_num[12], tcl_port_num[12];
		burner_adapter_argv[0] = "burner_adapter";
		burner_adapter_argv[1] = "-p";
		sprintf(burner_port_num, "%d", burner_port);
		burner_adapter_argv[2] = burner_port_num;
		burner_adapter_argv[3] = "-t";
		sprintf(tcl_port_num, "%d", tcl_port);
		burner_adapter_argv[4] = tcl_port_num;
		execv("./burner_adapter", burner_adapter_argv);
	}
#endif
	return 0;
}

int create_openocd(void) {
#ifdef __MINGW32__
	BOOL success;
	char cmd_string[256];

	/* init pipe */
	SECURITY_ATTRIBUTES attribute;

	attribute.nLength = sizeof(SECURITY_ATTRIBUTES);
	attribute.bInheritHandle = TRUE;
	attribute.lpSecurityDescriptor = NULL;

	/* adapter_pipe_input[0] is the read end of pipe
	 * adapter_pipe_input[1] is the write end of pipe */
	if (!CreatePipe(&adapter_pipe_input[0], &adapter_pipe_input[1], &attribute, 0)) {
		fprintf(stderr, "Create pipes failed.\n");
		return -1;
	}

	/* Do not inherit read end of pipe to child process. */
	if (!SetHandleInformation(adapter_pipe_input[0], HANDLE_FLAG_INHERIT, 0))
		return -1;

	HANDLE stdout_handle;
	HANDLE stderr_handle;
	/* Backup the original standard output/error handle. */
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
	/* Set the standard output handler to write end of pipe. */
	SetStdHandle(STD_OUTPUT_HANDLE, adapter_pipe_input[1]);
	SetStdHandle(STD_ERROR_HANDLE, adapter_pipe_input[1]);

	STARTUPINFO openocd_start_info;

	ZeroMemory(&openocd_proc_info, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&openocd_start_info, sizeof(STARTUPINFO));
	openocd_start_info.cb = sizeof(STARTUPINFO);

	sprintf(cmd_string, "./openocd.exe -d");

	success = CreateProcess(NULL,
			cmd_string,
			NULL,
			NULL,
			TRUE,
			0,
			NULL,
			NULL,
			&openocd_start_info,
			&openocd_proc_info);

	if (!success) {
		fprintf(stderr, "Create new process(openocd) failed.\n");
		return -1;
	}

	/* Close write end of the pipe. After the child process inherits
	 * the write handle, the parent process no longer needs its copy. */
	CloseHandle(adapter_pipe_input[1]);

	/* Restore the original standard output handle. */
	SetStdHandle(STD_OUTPUT_HANDLE, stdout_handle);
	SetStdHandle(STD_ERROR_HANDLE, stderr_handle);
#else
	char *openocd_argv[4] = {0, 0, 0, 0};

	/* init pipe */
	if (signal(SIGPIPE, sig_pipe) == SIG_ERR) {
		fprintf(stderr, "Register SIGPIPE handler failed.\n");
		return -1;
	}

	if (pipe(adapter_pipe_input) < 0) {
		fprintf(stderr, "Create pipes failed.\n");
		return -1;
	}

	openocd_pid = fork();
	if (openocd_pid < 0) {
		fprintf(stderr, "invoke openocd failed\n");
		return -1;
	} else if (openocd_pid == 0) {
		/* child process */
		/* close read pipe, pipe write through STDOUT */
		close(adapter_pipe_input[0]);
		if ((adapter_pipe_input[1] != STDOUT_FILENO) &&
				(adapter_pipe_input[1] != STDERR_FILENO)) {
			dup2(adapter_pipe_input[1], STDOUT_FILENO);
			dup2(adapter_pipe_input[1], STDERR_FILENO);
			close(adapter_pipe_input[1]);
		}

		/* invoke openocd */
		openocd_argv[0] = "openocd";
		openocd_argv[1] = "-d";

		execv("./openocd", openocd_argv);
	}

	/* parent process */
	/* close write pipe, pipe read through STDIN */
	close(adapter_pipe_input[1]);
	if (adapter_pipe_input[0] != STDIN_FILENO) {
		dup2(adapter_pipe_input[0], STDIN_FILENO);
		close(adapter_pipe_input[0]);
	}
#endif
	return 0;
}

int main(int argc, char **argv) {
	int i;
	parse_param(argc, argv);

	if(diagnosis){
		do_diagnosis();
		return 0;
	}

	if (target_probe() != ERROR_OK)
		goto PROCESS_CLEANUP;
	open_config_files();

	/* prepare all valid port num */
	update_gdb_port_num();
	
	for(i = 0; i < total_num_of_core; i++)
	{
	  gdb_port[i] = nds32_registry_portnum(gdb_port[i]);
	  if(gdb_port[i] < 0)
		{
			printf("gdb port num error\n");
			goto PROCESS_CLEANUP;
		}
	}
	
	burner_port = nds32_registry_portnum(burner_port);
	if(burner_port < 0)
	{
		printf("burner port num error\n");
		goto PROCESS_CLEANUP;
	}

	telnet_port = nds32_registry_portnum(telnet_port);
	if(telnet_port < 0)
	{
		printf("telnet port num error\n");
		goto PROCESS_CLEANUP;
	}
	tcl_port = nds32_registry_portnum(tcl_port);
	if(tcl_port < 0)
	{
		printf("tcl port num error\n");
		goto PROCESS_CLEANUP;
	}
	//printf("gdb_port[0]=%d, burner_port=%d, telnet_port=%d, tcl_port=%d .\n", gdb_port[0], burner_port, telnet_port, tcl_port);
	update_openocd_cfg();
	update_interface_cfg();
	update_target_cfg();
	update_board_cfg();

	/* close config files */
	close_config_files();

	/* create processes */
	if (create_burner_adapter() < 0)
		return -1;

	if (create_openocd() < 0)
		goto PROCESS_CLEANUP;

	/* use SIGINT handler to wait children termination status */
	if (signal(SIGINT, sig_int) == SIG_ERR) {
		fprintf(stderr, "Register SIGINT handler failed.\n");
		goto PROCESS_CLEANUP;
	}

	/* main loop. process openocd output */
	process_openocd_message();

PROCESS_CLEANUP:
	/* openocd process exits. kill burner_adapter process */
#ifdef __MINGW32__
	TerminateProcess(openocd_proc_info.hProcess, 0);
	TerminateProcess(burner_proc_info.hProcess, 0);
#else
	kill(openocd_pid, SIGTERM);
	kill(burner_adapter_pid, SIGTERM);
#endif

	return 0;
}

int aice_usb_set_edm_operations_passcode(uint32_t coreid)
{
	int i;
	for (i = 0 ; i < edm_operations_num ; i++) {
		if (aice_write_misc(coreid,	edm_operations[i].reg_no, edm_operations[i].data) != ERROR_OK)
			return ERROR_FAIL;
	}
	return ERROR_OK;
}
