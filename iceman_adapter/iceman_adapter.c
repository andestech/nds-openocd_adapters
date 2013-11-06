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

const char *opt_string = "hBvDHKgjJGkd:S:R:p:b:c:T:r:C:P:F:O:l:L:N:Z:t:z:x::";
struct option long_option[] = {
	{"help", no_argument, 0, 'h'},
	{"boot", no_argument, 0, 'B'},
	{"version", no_argument, 0, 'v'},
	{"unlimited-log", no_argument, 0, 'D'},
	{"reset-hold", no_argument, 0, 'H'},
	{"soft-reset-hold", no_argument, 0, 'K'},
	{"force-debug", no_argument, 0, 'g'},
	{"enable-virtual-hosting", no_argument, 0, 'j'},
	{"disable-virtual-hosting", no_argument, 0, 'J'},
	{"enable-global-stop", no_argument, 0, 'G'},
	{"word-access-mem", no_argument, 0, 'k'},
	{"debug", required_argument, 0, 'd'},
	{"stop-seq", required_argument, 0, 'S'},
	{"resume-seq", required_argument, 0, 'R'},
	{"port", required_argument, 0, 'p'},
	{"bport", required_argument, 0, 'b'},
	{"tport", required_argument, 0, 't'},
	{"clock", required_argument, 0, 'c'},
	{"boot-time", required_argument, 0, 'T'},
	{"ice-retry", required_argument, 0, 'r'},
	{"check-times", required_argument, 0, 'C'},
	{"passcode", required_argument, 0, 'P'},
	{"edm-port-file", required_argument, 0, 'F'},
	{"edm-port-operation", required_argument, 0, 'O'},
	{"custom-srst", required_argument, 0, 'l'},
	{"custom-trst", required_argument, 0, 'L'},
	{"custom-restart", required_argument, 0, 'N'},
	{"target", required_argument, 0, 'Z'},
	{"aie-conf", required_argument, 0, 'z'},
	{"diagnosis", optional_argument, 0, 'x'},
	{0, 0, 0, 0}
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
static int clock_setting = 9;
static int debug_level = 2;
static int boot_code_debug;
static int gdb_port = 1111;
static int burner_port = 2354;
static int telnet_port = 4444;
static int virtual_hosting = 0;
static int startup_reset_halt;
static int soft_reset_halt;
static int force_debug;
static int unlimited_log;
static int boot_time = 2500;
static unsigned int count_to_check_dbger = 30;
static int global_stop;
static int word_access_mem;
static enum TARGET_TYPE target_type = TARGET_INVALID;
static char *edm_passcode = NULL;
static const char *custom_srst_script = NULL;
static const char *custom_trst_script = NULL;
static const char *custom_restart_script = NULL;
static const char *aieconf_desc_list = NULL;
static int diagnosis = 0;
static int diagnosis_memory = 0;
static unsigned int diagnosis_address = 0;

static void show_version(void) {
	printf ("Andes ICEman v3.0.0 (openocd-0.7.0)\n");
	printf ("Copyright (C) 2007-2013 Andes Technology Corporation\n");
}

static void show_usage(void) {
	printf("Usage:\niceman --port start_port_number [--help]\n");

	printf("-h, --help:\t\tThe usage is for ICEman\n");
	printf("-p, --port:\t\tSocket port number for gdb connection\n");
	printf("-b, --bport:\t\tSocket port number for Burner connection\n");
	printf("\t\t\t(default: 2354)\n");
	printf("-t, --tport:\t\tSocket port number for Telnet connection\n");
	printf("\t\t\t(default: 6666)\n");
	printf("-v, --version:\t\tVersion of ICEman\n");
	printf("-j, --enable-virtual-hosting:\tEnable virtual hosting\n");
	printf("-J, --disable-virtual-hosting:\tDisable virtual hosting\n");
	printf("-B, --boot:\t\tReset-and-hold while connecting to target\n");
	printf("-D, --unlimited-log:\tDo not limit log file size to 512 KB\n");
	printf("-H, --reset-hold:\tReset-and-hold while ICEman startup\n");
	printf("-K, --soft-reset-hold:\tUse soft reset-and-hold\n");
	printf("-c, --clock:\t\tSpecific JTAG clock setting\n");
	printf("\t\t\t0: 30 MHz\n");
	printf("\t\t\t1: 15 MHz\n");
	printf("\t\t\t2: 7.5 MHz\n");
	printf("\t\t\t3: 3.75 MHz\n");
	printf("\t\t\t4: 1.875 MHz\n");
	printf("\t\t\t5: 937.5 KHz\n");
	printf("\t\t\t6: 468.75 KHz\n");
	printf("\t\t\t7: 234.375 KHz\n");
	printf("\t\t\t8: 48 MHz\n");
	printf("\t\t\t9: 24 MHz\n");
	printf("\t\t\t10: 12 MHz\n");
	printf("\t\t\t11: 6 MHz\n");
	printf("\t\t\t12: 3 MHz\n");
	printf("\t\t\t13: 1.5 MHz\n");
	printf("\t\t\t14: 750 KHz\n");
	printf("\t\t\t15: 375 KHz\n");
	printf("\t\t\tAICE2 only supports 8 ~ 15\n");
	printf("-r, --ice-retry:\tRetry count when AICE command timeout\n");
	printf("-T, --boot-time:\tBoot time of target board (milliseconds)\n");
	printf("-S, --stop-seq:\t\tSpecify the SOC device operation sequence while CPU stop\n");
	printf("-R, --resume-seq:\tSpecify the SOC device operation sequence before CPU resume\n\n");
	printf("\t\tUsage: --stop-seq \"address:data\"\n");
	printf("\t\t\t--resume-seq \"address:{data|rst}\"\n");
	printf("\t\tExample: --stop-seq \"0x200800:0x80\"\n");
	printf("\t\t\t--resume-seq \"0x200800:rst\"\n\n");
	printf("-C, --check-times:\tCount to check DBGER\n");
	printf("-P, --passcode:\t\tPASSCODE of secure MPU\n");
	printf("-O, --edm-port-operation: EDM port0/1 operations\n");
	printf("\t\tUsage: -O \"write_edm 6:0x1234,7:0x5678;\"\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n");
	printf("-F, --edm-port-file:\tEDM port0/1 operations file name\n");
	printf("\t\tFile format:\n");
	printf("\t\t\twrite_edm 6:0x1234,7:0x1234);\n");
	printf("\t\t\twrite_edm 6:0x1111);\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n");
	printf("-G, --enable-global-stop: Enable 'global stop'.  As users use up hardware watchpoints, target stops at every load/store instructions. \n");
	printf("-k, --word-access-mem:\tAlways use word-aligned address to access memory device\n");
	printf("-l, --custom-srst:\tUse custom script to do SRST\n");
	printf("-L, --custom-trst:\tUse custom script to do TRST\n");
	printf("-N, --custom-restart:\tUse custom script to do RESET-HOLD\n");
	printf("-Z, --target:\t\tSpecify target type (v2/v3/v3m)\n");
	printf("-z, --aie-conf :\t\tSpecify aie file on each core\n");
	printf("\t\tUsage: --aie-conf <core#id>=<aie_conf>[,<core#id>=<aie_conf>]*\n");
	printf("\t\t\tExample: --aie-conf core0=core0.aieconf,core1=core1.aieconf\n");
	printf("-x, --diagnosis:\tDiagnose connectivity issue\n");
	printf("\t\tUsage: --diagnosis[=address]\n");
}

static void parse_param(int a_argc, char **a_argv) {
	while(1) {
		int c = 0;
		int option_index;
		int optarg_len;

		c = getopt_long(a_argc, a_argv, opt_string, long_option, &option_index);
		if (c == EOF)
			break;

		switch (c) {
			case 'S':
				optarg_len = strlen(optarg);
				memory_stop_sequence = malloc(optarg_len + 1 + 9);  /* +9 for stop-seq */
				memcpy(memory_stop_sequence, "stop-seq ", 9);
				memcpy(memory_stop_sequence + 9, optarg, optarg_len + 1);
				break;
			case 'R':
				optarg_len = strlen(optarg);
				memory_resume_sequence = malloc(optarg_len + 1 + 11); /* +11 for resume-seq */
				memcpy(memory_resume_sequence, "resume-seq ", 11);
				memcpy(memory_resume_sequence + 11, optarg, optarg_len + 1);
				break;
			case 'r':
				aice_retry_time = strtol(optarg, NULL, 0);
				break;
			case 'c':
				clock_setting = strtol(optarg, NULL, 0);
				if (clock_setting < 0)
					clock_setting = 0;
				if (15 < clock_setting)
					clock_setting = 15;
				break;
			case 'd':
				debug_level = strtol(optarg, NULL, 0);
				break;
			case 'B':
				boot_code_debug = 1;
				break;
			case 'b':
				burner_port = strtol(optarg, NULL, 0);
				break;
			case 't':
				telnet_port = strtol(optarg, NULL, 0);
				break;
			case 'p':
				gdb_port = strtol(optarg, NULL, 0);
				break;
			case 'j':
				virtual_hosting = 1;
				break;
			case 'J':
				virtual_hosting = 0;
				break;
			case 'H':
				startup_reset_halt = 1;
				break;
			case 'K':
				soft_reset_halt = 1;
				break;
			case 'g':
				force_debug = 1;
				break;
			case 'D':
				unlimited_log = 1;
				debug_level = 3;
				break;
			case 'T':
				boot_time = strtol(optarg, NULL, 0);
				break;
			case 'G':
				global_stop = 1;
				break;
			case 'k':
				word_access_mem = 1;
				break;
			case 'Z':
				optarg_len = strlen(optarg);
				if (strncmp(optarg, "v2", optarg_len) == 0) {
					target_type = TARGET_V2;
				} else if (strncmp(optarg, "v3", optarg_len) == 0) {
					target_type = TARGET_V3;
				} else if (strncmp(optarg, "v3m", optarg_len) == 0) {
					target_type = TARGET_V3m;
				} else {
					target_type = TARGET_INVALID;
				}
				break;
			case 'P':
				optarg_len = strlen(optarg);
				edm_passcode = malloc(optarg_len + 1);
				memcpy(edm_passcode, optarg, optarg_len + 1);
				break;
			case 'O':
				optarg_len = strlen(optarg);
				edm_port_operations = malloc(optarg_len + 1);
				memcpy(edm_port_operations, optarg, optarg_len + 1);
				break;
			case 'F':
				edm_port_op_file = optarg;
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
			case 'C':
				count_to_check_dbger = strtoul(optarg, NULL, 0);
				break;
			case 'z':
				aieconf_desc_list = optarg;
				break;
			case 'x':
				diagnosis = 1;
				if (optarg != NULL){
					diagnosis_memory = 1;
					sscanf(optarg, "0x%x", &diagnosis_address);
				}else
					diagnosis_memory = 0;
				break;
			case 'v':
					show_version();
					exit(0);
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

static void target_probe(void)
{
	uint16_t vid = 0x1CFC;
	uint16_t pid = 0x0;
	uint32_t value_edmcfg, value_cr4;
	uint32_t version = 0;
	uint32_t coreid = 0;

	if (target_type != TARGET_INVALID)
		return;

	nds32_reg_init ();

	if (ERROR_OK != aice_usb_open(vid, pid)) {
		printf("<-- Can not open usb -->\n");
		return;
	}

	if (ERROR_OK != aice_edm_init(coreid)) {
		printf("<-- Can not init EDM -->\n");
		return;
	}

	if (ERROR_OK != aice_read_edmsr(coreid, NDS_EDM_SR_EDM_CFG, &value_edmcfg)) {
		printf("<-- Can not read EDMSR -->\n");
		return;
	}
	version = (value_edmcfg >> 16) & 0xFFFF;
	if ((version & 0x1000) == 0) {
		/* edm v2 */
		target_type = TARGET_V2;
	} else {
		/* edm v3 */
		/* halt cpu to check whether cpu belongs to mcu not */
		if (ERROR_OK != aice_usb_halt(coreid)) {
			printf("<-- Can not halt cpu -->\n");
			return;
		}
		if (ERROR_OK != aice_usb_read_reg(coreid, CR4, &value_cr4)) {
			printf("<-- Can not read register -->\n");
			return;
		}
		if ((value_cr4 & 0x100000) == 0x100000) {
			target_type = TARGET_V3m;
		} else {
			target_type = TARGET_V3;
		}
		if (ERROR_OK != aice_usb_run(coreid)) {
			printf("<-- Can not release cpu -->\n");
			return;
		}
	}
	aice_usb_close();
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
	uint32_t id_codes, selected_core;
	uint32_t hardware_version, firmware_version, fpga_version;
	uint32_t scan_clock, scan_base_freq, scan_freq;
	uint8_t num_of_ids, test_memory_value, memory_value, backup_memory_value;
	extern struct aice_nds32_info core_info[];

	enum{
		CONFIRM_USB_CONNECTION,
		CONFIRM_AICE_VERSIONS,
		CONFIRM_JTAG_FREQUENCY_DETECTED,
		CONFIRM_SET_JTAG_FREQUENCY,
		CONFIRM_JTAG_CONNECTIVITY,
		CONFIRM_JTAG_DOMAIN,
		CONFIRM_TRST_WORKING,
		CONFIRM_SRST_NOT_AFFECT_JTAG,
		CONFIRM_SELECT_FIRST_CORE,
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
		[CONFIRM_SELECT_FIRST_CORE] = "confirm Select first core success ...",
		[CONFIRM_RESET_HOLD] = "confirm Reset and debug success ...",
		[CONFIRM_DIM_AND_CPU_DOMAIN] = "confirm DIM and CPU domain operational ...",
		[CONFIRM_MEMORY_ON_BUS] = "confirm memory access on BUS mode ...",
		[CONFIRM_MEMORY_ON_CPU] = "confirm memory access on CPU mode ..."
	};

	if (target_type != TARGET_INVALID)
		return;

	nds32_reg_init();

	printf("********************\n");
	printf("Diagnostic Report\n");
	printf("********************\n");

	/* 1. confirm USB connection */
	last_fail_item = CONFIRM_USB_CONNECTION;
	if (ERROR_OK != aice_usb_open(vid, pid))
		goto report;

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
		return;

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

	/* 4. Report JTAG scan chain (confirm JTAG connectivity) */
	last_fail_item = CONFIRM_JTAG_CONNECTIVITY;
	if (ERROR_OK != aice_scan_chain(&id_codes, &num_of_ids))
		goto report;
	printf("There is %u core in target\n", num_of_ids);
	selected_core = 0;

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
	last_fail_item = CONFIRM_SELECT_FIRST_CORE;
	aice_core_init(selected_core);
	if (ERROR_OK != aice_edm_init(selected_core))
		goto report;
	printf("Core #%u: EDM version 0x%x\n", selected_core+1, core_info[selected_core].edm_version);

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
		/* 9. Test direct memory access (bus view) confirm BUS working */
		last_fail_item = CONFIRM_MEMORY_ON_BUS;
		test_memory_value = rand();
		aice_usb_memory_access(selected_core, NDS_MEMORY_ACC_BUS);
		aice_usb_read_memory_unit(selected_core, diagnosis_address, 1, 1, &backup_memory_value);
		aice_usb_write_memory_unit(selected_core, diagnosis_address, 1, 1, &test_memory_value);
		aice_usb_read_memory_unit(selected_core, diagnosis_address, 1, 1, &memory_value);
		if (memory_value != test_memory_value)
			goto report;

		/* 10. Test memory access through DIM (CPU view) reconfirm CPU working */
		last_fail_item = CONFIRM_MEMORY_ON_CPU;
		test_memory_value = rand();
		aice_usb_memory_access(selected_core, NDS_MEMORY_ACC_CPU);
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
	aice_usb_close();
}

#define LINE_BUFFER_SIZE 2048

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
}

static void close_config_files(void) {
	fclose(openocd_cfg_tpl);
	fclose(openocd_cfg);
	fclose(interface_cfg_tpl);
	fclose(interface_cfg);
	fclose(board_cfg_tpl);
	fclose(board_cfg);
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
	char line_buffer[LINE_BUFFER_SIZE];
	char *search_str;
	FILE *debug_log = NULL;
	int is_ready = 0;
	unsigned int line_buffer_len, log_buffer_remain;

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
	printf("The core #0 listens on %d.\n", gdb_port);
	printf("Burner listens on %d\n", burner_port);
	printf("Telnet port: %d\n", telnet_port);
#ifdef __MINGW32__
	/* ReadFile from pipe is not LINE_BUFFERED. So, we process newline ourselves. */
	DWORD had_read;
	char buffer[LINE_BUFFER_SIZE];
	char *newline_pos;
	DWORD line_buffer_index;
	long long unprocessed_len;
	char *unprocessed_buf;

	/* init unprocessed buffer */
	if (ReadFile(adapter_pipe_input[0], buffer, LINE_BUFFER_SIZE, &had_read, NULL) == 0)
		return;
	buffer[had_read] = '\0';
	unprocessed_buf = buffer;
	unprocessed_len = had_read;
	line_buffer_index = 0;

	while (unprocessed_len >= 0) {
		/* find '\n' */
		newline_pos = strchr(unprocessed_buf, '\n');
		if (newline_pos == NULL) {
			/* cannot find '\n', copy whole buffer to line_buffer */
			strcpy(line_buffer + line_buffer_index, unprocessed_buf);
			line_buffer_index += strlen(unprocessed_buf);
			if (ReadFile(adapter_pipe_input[0], buffer, LINE_BUFFER_SIZE, &had_read, NULL) == 0)
				return;
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
	while (fgets(line_buffer, LINE_BUFFER_SIZE, stdin) != NULL) {
#endif
		if (is_ready == 0) {
			if ((search_str = strstr(line_buffer, "clock speed")) != NULL) {
				printf("JTAG frequency %s", search_str + 12);
				fflush(stdout);
			} else if ((search_str = strstr(line_buffer, "EDM version")) != NULL) {
				printf("Core #0: EDM version %s", search_str + 12);
				printf("ICEman is ready to use.\n");
				fflush(stdout);
				is_ready = 1;
			}
		} else {
			 if ((search_str = strstr(line_buffer, "<--")) != NULL) {
				printf("%s", search_str);
				fflush(stdout);
			} else {
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
			}
		}
#ifdef __MINGW32__
		line_buffer_index = 0;
#endif
		/* TODO: use pin-pon buffer to log debug messages */
	}
}

static void update_openocd_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];

	/* update openocd.cfg */
	while (fgets(line_buffer, LINE_BUFFER_SIZE, openocd_cfg_tpl) != NULL)
		fputs(line_buffer, openocd_cfg);

	fprintf(openocd_cfg, "gdb_port %d\n", gdb_port);
	fprintf(openocd_cfg, "telnet_port %d\n", telnet_port);
	fprintf(openocd_cfg, "debug_level %d\n", debug_level);

	if (virtual_hosting)
		fprintf(openocd_cfg, "nds virtual_hosting on\n");
	else
		fprintf(openocd_cfg, "nds virtual_hosting off\n");

	if (global_stop)
		fprintf(openocd_cfg, "nds global_stop on\n");
	else
		fprintf(openocd_cfg, "nds global_stop off\n");

	if (startup_reset_halt)
		fprintf(openocd_cfg, "nds reset_halt_as_init on\n");

	fprintf(openocd_cfg, "nds boot_time %d\n", boot_time);

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
	fprintf(interface_cfg, "aice count_to_check_dbger %d\n", count_to_check_dbger);

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
}

#define MAX_LEN_AIECONF_NAME 2048
#define TOSTR(x)	#x
#define XTOSTR(x)	TOSTR(x)
static void update_board_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];
	const char *aieconf_desc;
	unsigned int core_id =0, read_byte = 0;
	char aieconf[MAX_LEN_AIECONF_NAME + 1];
	int ret;

	/* update nds32_xc5.cfg */
	char *find_pos;
	while (fgets(line_buffer, LINE_BUFFER_SIZE, board_cfg_tpl) != NULL) {
		if ((find_pos = strstr(line_buffer, "--target")) != NULL) {
			if (target_type == TARGET_V3) {
				strcpy(line_buffer, "source [find target/nds32v3.cfg]\n");
			} else if (target_type == TARGET_V2) {
				strcpy(line_buffer, "source [find target/nds32v2.cfg]\n");
			} else if (target_type == TARGET_V3m) {
				strcpy(line_buffer, "source [find target/nds32v3m.cfg]\n");
			} else {
				fprintf(stderr, "No target specified\n");
				exit(0);
			}
		} else if ((find_pos = strstr(line_buffer, "--boot")) != NULL) {
			if (boot_code_debug)
				strcpy(find_pos - 1, "reset halt\n");
			else
				strcpy(find_pos - 1, "\n");
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
		} else if ((find_pos = strstr(line_buffer, "--aie-conf")) != NULL) {
			strcpy(line_buffer, "set _AIE_CONF \"\"\n");
			if (aieconf_desc_list) {
				aieconf_desc = aieconf_desc_list;
				while (1) {
					aieconf[0] = '\0';
					core_id = 0;
					ret = 0;

					ret = sscanf(aieconf_desc, "core%u=%" XTOSTR(MAX_LEN_AIECONF_NAME) "[^,]%n",
									&core_id, aieconf, &read_byte);
					if (ret != 2) {
						printf("<-- Can not parse --aie-conf argument '%s'\n. -->", aieconf_desc);
						break;
					}

					/* TODO: support multi core */
					sprintf(line_buffer, "set _AIE_CONF %s\n", aieconf);
					aieconf_desc += read_byte;	/* aieconf points to ',' or '\0' */
					if (*aieconf_desc == '\0')
						break;
					else
						aieconf_desc += 1;	/* point to the one next to ',' */
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
			fprintf(board_cfg, "nds32.cpu nds login_edm_operation 0x%x 0x%x\n", edm_operations[i].reg_no, edm_operations[i].data);
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

	if (stop_sequences_num > 0) {
		fputs("$_TARGETNAME configure -event halted {\n", board_cfg);
		fputs("\tnds32.cpu nds mem_access bus\n", board_cfg);
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
		fputs("\tnds32.cpu nds mem_access cpu\n", board_cfg);
		fputs("}\n", board_cfg);
	}

	if (resume_sequences_num > 0) {
		fputs("$_TARGETNAME configure -event resumed {\n", board_cfg);
		fputs("\tnds32.cpu nds mem_access bus\n", board_cfg);
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
		fputs("\tnds32.cpu nds mem_access cpu\n", board_cfg);
		fputs("}\n", board_cfg);
	}

}

int create_burner_adapter(void) {
#ifdef __MINGW32__
	BOOL success;
	char cmd_string[256];

	sprintf(cmd_string, "./burner_adapter.exe -p %d", burner_port);

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
	char *burner_adapter_argv[4] = {0, 0, 0, 0};
	burner_adapter_pid = fork();
	if (burner_adapter_pid < 0) {
		fprintf(stderr, "invoke burner_adapter failed\n");
		return -1;
	} else if (burner_adapter_pid == 0) {
		/* invoke burner adapter */
		char burner_port_num[12];
		burner_adapter_argv[0] = "burner_adapter";
		burner_adapter_argv[1] = "-p";
		sprintf(burner_port_num, "%d", burner_port);
		burner_adapter_argv[2] = burner_port_num;

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
	parse_param(argc, argv);

	if(diagnosis){
		do_diagnosis();
		return 0;
	}

	target_probe();
	open_config_files();

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
	TerminateProcess(burner_proc_info.hProcess, 0);
#else
	kill(burner_adapter_pid, SIGTERM);
#endif

	return 0;
}
