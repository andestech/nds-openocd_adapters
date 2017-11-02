#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>

#ifndef ERROR_OK
#define ERROR_OK           (0)
#define ERROR_FAIL         (-1)
#endif

#define AICE_MAX_NUM_CORE  (0x10)
#define AICE_MAX_NUM_PORTS 32

#define PORTNUM_BURNER     2354
#define PORTNUM_TELNET     4444
#define PORTNUM_TCL        6666
#define PORTNUM_GDB        1111

#define LINE_BUFFER_SIZE   2048
#define ICEMAN_VERSION     "v4.2.0"
#define NDS32_USER_CFG     "nds32_user.cfg"

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

#define LONGOPT_CP0     7
#define LONGOPT_CP1     8
#define LONGOPT_CP2     9
#define LONGOPT_CP3     10
#define LONGOPT_USE_SDM   11
int long_opt_flag = 0;
uint32_t cop_reg_nums[4] = {0,0,0,0};
const char *opt_string = "aAb:Bc:C:d:DeF:f:gGhHkK::l:L:M:N:o:O:p:P:r:R:sS:t:T:vx::Xy:z:Z:";
struct option long_option[] = {
	{"cp0reg", required_argument, &long_opt_flag, LONGOPT_CP0},
	{"cp1reg", required_argument, &long_opt_flag, LONGOPT_CP1},
	{"cp2reg", required_argument, &long_opt_flag, LONGOPT_CP2},
	{"cp3reg", required_argument, &long_opt_flag, LONGOPT_CP3},
	{"use-sdm", no_argument, &long_opt_flag, LONGOPT_USE_SDM},

	{"reset-aice", no_argument, 0, 'a'},
	{"no-crst-detect", no_argument, 0, 'A'},
	{"bport", required_argument, 0, 'b'},
	{"boot", no_argument, 0, 'B'},
	{"clock", required_argument, 0, 'c'},
	{"check-times", required_argument, 0, 'C'},
	{"debug", required_argument, 0, 'd'},
	{"larger-logfile", no_argument, 0, 'D'},
	{"unlimited-log", no_argument, 0, 'e'},
	//{"edmv2", no_argument, 0, 'E'},
	{"log-output", required_argument, 0, 'f'},
	{"edm-port-file", required_argument, 0, 'F'},
	{"force-debug", no_argument, 0, 'g'},
	{"enable-global-stop", no_argument, 0, 'G'},
	{"help", no_argument, 0, 'h'},
	{"reset-hold", no_argument, 0, 'H'},
	//{"enable-virtual-hosting", no_argument, 0, 'j'},
	//{"disable-virtual-hosting", no_argument, 0, 'J'},
	{"word-access-mem", no_argument, 0, 'k'},
	{"soft-reset-hold", optional_argument, 0, 'K'},
	{"custom-srst", required_argument, 0, 'l'},
	{"custom-trst", required_argument, 0, 'L'},
    {"edm-dimb", required_argument, 0, 'M'},
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
	{"idlm-base", required_argument, 0, 'y'},
	{"ace-conf", required_argument, 0, 'z'},
	{"target", required_argument, 0, 'Z'},
	{0, 0, 0, 0}
};
extern const char *aice_clk_string[];
/*
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
*/
const char *v5_clk_string[] = { 
	"30 MHz",
	"15 MHz",
	"7.5 MHz",
	"3.75 MHz",
	"1.875 MHz",
	"909.091 KHz",
	"461.538 KHz",
	"232.558 KHz",
	"10 MHz",
	"10 MHz",
	"10 MHz",
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
	TARGET_V5,
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

extern struct EDM_OPERATIONS nds32_edm_ops[];
extern uint32_t nds32_edm_ops_num;


static char *memory_stop_sequence = NULL;
static char *memory_resume_sequence = NULL;
static char *edm_port_operations = NULL;
static const char *edm_port_op_file = NULL;
static int aice_retry_time = 2;//50;
static int aice_no_crst_detect = 0;
static int clock_setting = 16;
static int debug_level = 3;
static int boot_code_debug;
static int gdb_port[AICE_MAX_NUM_PORTS];
static char *gdb_port_str = NULL;
static int burner_port = PORTNUM_BURNER;
static int telnet_port = PORTNUM_TELNET;
static int tcl_port = PORTNUM_TCL;
static int startup_reset_halt = 0;
static int soft_reset_halt = 0;
static int force_debug;
static unsigned int log_file_size = 0xA00000; // default: 10MB
static int boot_time = 5000;
static int reset_time = 1000;
static int reset_aice_as_startup = 0;
static int count_to_check_dbger = 0;
static int global_stop;
static int word_access_mem;
static enum TARGET_TYPE target_type[AICE_MAX_NUM_CORE] = {TARGET_V3};
static const char *custom_srst_script = NULL;
static const char *custom_trst_script = NULL;
static const char *custom_restart_script = NULL;
static const char *aceconf_desc_list = NULL;
static int diagnosis = 0;
static int diagnosis_memory = 0;
static unsigned int diagnosis_address = 0;
static uint8_t total_num_of_ports = 1;
static unsigned int custom_def_idlm_base=0, ilm_base=0, ilm_size=0, dlm_base=0, dlm_size=0;
extern int nds32_registry_portnum_without_bind(int port_num);
extern int nds32_registry_portnum(int port_num);
static void parse_edm_operation(const char *edm_operation);
//extern int force_turnon_V3_EDM;
//extern char *BRANCH_NAME, *COMMIT_ID;
extern int openocd_main(int argc, char *argv[]);
extern char *nds32_edm_passcode_init;
static const char *log_output = NULL;

#define DIMBR_DEFAULT (0xFFFF0000u)
static unsigned int edm_dimb = DIMBR_DEFAULT;
static unsigned int use_sdm = 0;

static void show_version(void) {
	printf("Andes ICEman %s (OpenOCD) BUILD_ID: %s\n", ICEMAN_VERSION, BUILD_ID);
	printf("Copyright (C) 2007-2017 Andes Technology Corporation\n");
}

static void show_srccode_ver(void) {
	printf("Branch: %s\n", BRANCH_NAME);
	printf("Commit: %s\n", COMMIT_ID);
}

static void show_usage(void) {
	uint32_t i;
	printf("Usage:\nICEman --port start_port_number[:end_port_number] [--help]\n");
	printf("-a, --reset-aice (For AICE only):\tReset AICE as ICEman startup\n");
	printf("-A, --no-crst-detect (Only for V3):\tNo CRST detection in debug session\n");
	printf("-b, --bport:\t\tSocket port number for Burner connection\n");
	printf("\t\t\t(default: 2354)\n");
	//printf("-B, --boot:\t\tReset-and-hold while connecting to target\n");

	// V3
	printf("-c, --clock (For V3):\t\tSpecify JTAG clock setting\n");
	printf("\t\tUsage: -c num\n");
	printf("\t\t\tnum should be the following:\n");
	for (i=0; i<=15; i++)
		printf("\t\t\t%d: %s\n", i, aice_clk_string[i]);
	printf("\t\t\tAICE-MCU, AICE2 and AICE2-T support 8 ~ 15\n");
	printf("\t\t\tAICE-MINI only supports 10 ~ 15\n\n");

	// V5
	printf("-c, --clock (For V5):\t\tSpecify JTAG clock setting\n");
	printf("\t\tUsage: -c num\n");
	printf("\t\t\tnum should be the following:\n");
	for (i=0; i<=15; i++)
		printf("\t\t\t%d: %s\n", i, v5_clk_string[i]);

	printf("-C, --check-times (For V3):\tCount/Second to check DBGER\n");
	printf("-C, --check-times (For V5):\tSecond to check DTM\n");
	printf("\t\t\t(default: 500 times)\n");
	printf("\t\tExample:\n");
	printf("\t\t\t1. -C 100 to check 100 times\n");
	printf("\t\t\t2. -C 100s or -C 100S to check 100 seconds\n\n");
	//printf("-D, --unlimited-log:\tDo not limit log file size to 512 KB\n");
	printf("-D, --larger-logfile:\tThe maximum size of the log file is 1MBx2. The size is increased to 512MBx2 with this option.\n");
	//printf("-e, --edm-retry:\tRetry count of getting EDM version as ICEman startup\n");
	printf("-f, --log-output:\toutput log file path\n");
	printf("-F, --edm-port-file (Only for Secure MPU):\tEDM port0/1 operations file name\n");
	printf("\t\tFile format:\n");
	printf("\t\t\twrite_edm 6:0x1234,7:0x1234;\n");
	printf("\t\t\twrite_edm 6:0x1111;\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n\n");
	//printf("-g, --force-debug: \n");
	printf("-G, --enable-global-stop (Only for V3): Enable 'global stop'.  As users use up hardware watchpoints, target stops at every load/store instructions. \n");
	printf("-h, --help:\t\tThe usage is for ICEman\n");
	printf("-H, --reset-hold:\tReset-and-hold while ICEman startup\n");
	//printf("-j, --enable-virtual-hosting:\tEnable virtual hosting\n");
	//printf("-J, --disable-virtual-hosting:\tDisable virtual hosting\n");
	printf("-k, --word-access-mem (Only for V3):\tAlways use word-aligned address to access device\n");
	printf("-K, --soft-reset-hold (Only for V3):\tUse soft reset-and-hold\n");
	printf("-l, --custom-srst (Only for V3):\tUse custom script to do SRST\n");
	printf("-L, --custom-trst (Only for V3):\tUse custom script to do TRST\n");
	printf("-M, --edm-dimb (Only for V3):\t\tSpecify the DIMBR (Debug Instruction Memory Base Register)\n");
	printf("\t\t\t(default: 0xFFFF0000)\n");
	printf("-N, --custom-restart (Only for V3):\tUse custom script to do RESET-HOLD\n");
	//printf("-M, --Mode:\t\tSMP\\AMP Mode(Default: AMP Mode)\n");
	printf("-o, --reset-time:\tReset time of reset-and-hold (milliseconds)\n");
	printf("\t\t\t(default: 1000 milliseconds)\n");
	printf("-O, --edm-port-operation (Only for Secure MPU): EDM port0/1 operations\n");
	printf("\t\tUsage: -O \"write_edm 6:0x1234,7:0x5678;\"\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n\n");
	printf("-p, --port:\t\tSocket port number for gdb connection\n");
	printf("-P, --passcode (Only for Secure MPU):\t\tPASSCODE of secure MPU\n");
	printf("-r, --ice-retry (Only for V3):\tRetry count when AICE command timeout\n");
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
	printf("\t\t\t(default: 5000 milliseconds)\n");
	//printf("-u, --update-fw:\tUpdate AICE F/W\n");
	//printf("-U, --update-fpga:\tUpdate AICE FPGA\n");
	printf("-v, --version:\t\tVersion of ICEman\n");
	//printf("-w, --backup-fw:\tBackup AICE F/W\n");
	//printf("-W, --backup-fpga:\tBackup AICE FPGA\n");
	printf("-x, --diagnosis:\tDiagnose connectivity issue\n");
	printf("\t\tUsage: --diagnosis[=address]\n\n");
	printf("-X, --uncnd-reset-hold:\tUnconditional Reset-and-hold while ICEman startup (This implies -H)\n");
	printf("-y, --idlm-base (Only for V3):\t\tDefine ILM&DLM base and size\n");
	printf("-z, --ace-conf (Only for V3):\t\tSpecify ACE file on each core\n");
	printf("\t\tUsage: --ace-conf <core#id>=<ace_conf>[,<core#id>=<ace_conf>]*\n");
	printf("\t\t\tExample: --ace-conf core0=core0.aceconf,core1=core1.aceconf\n");
	printf("-Z, --target:\t\tSpecify target type (v2/v3/v3m/v5)\n");
	printf("--cp0reg/cp1reg/cp2reg/cp3reg (Only for V3):\t\tSpecify coprocessor register numbers\n");
	printf("\t\t\tExample: --cp0reg 1024 --cp1reg 1024\n");
	printf("--use-sdm (Only for V3):\t\tUse System Debug Module\n");
}

static int parse_param(int a_argc, char **a_argv) {
	while(1) {
		int c = 0;
		int option_index = 0;
		int optarg_len;
		char tmpchar = 0;
		int long_opt = 0;
		uint32_t cop_nums = 0;
		//uint32_t ms_check_dbger = 0;

		c = getopt_long(a_argc, a_argv, opt_string, long_option, &option_index);

		if (c == EOF)
			break;

		switch (c) {
			case 0:
				long_opt = *long_option[option_index].flag;
				printf("option %s,", long_option[option_index].name);
				if ((long_opt >= LONGOPT_CP0) &&
					(long_opt <= LONGOPT_CP3)) {
						cop_nums = strtol(optarg, NULL, 0);
						if (cop_nums > 4096) {
							printf("cop_nums max is 4096 \n");
						} else {
							cop_reg_nums[long_opt - LONGOPT_CP0] = cop_nums;
							printf("cop_reg_nums[%d]=%d \n", long_opt - LONGOPT_CP0, cop_reg_nums[long_opt - LONGOPT_CP0]);
						}
				} else if (long_opt == LONGOPT_USE_SDM) {
					use_sdm = 1;
				}

				break;
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
				if ((clock_setting < 0) ||
					(clock_setting > 15)) {
					printf("-c %d is an invalid option, the valid range for '-c' is 0-15.\n", clock_setting);
					return ERROR_FAIL;
				}
				break;
			case 'C':
				sscanf(optarg, "%u%c", &count_to_check_dbger, &tmpchar);
				if (tmpchar == 's' || tmpchar == 'S')
					count_to_check_dbger *= 1000;
				break;
			case 'd':
				debug_level = strtol(optarg, NULL, 0);
				break;
			case 'D':
			case 'e':
				log_file_size = 0x20000000;
				debug_level = 3;
				break;
			/*case 'E':
				printf("force_turnon_V3_EDM off \n");
				force_turnon_V3_EDM = 0;
				break;*/
			case 'F':
				edm_port_op_file = optarg;
				break;
            case 'f':
                log_output = optarg;
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
				if (optarg != NULL){
					sscanf(optarg, "%x", &soft_reset_halt);
				}
				else {
					soft_reset_halt = 2;
				}
				break;
			case 'l': /* customer-srst */
				custom_srst_script = optarg;
				break;
			case 'L': /* customer-trst */
				custom_trst_script = optarg;
				break;
            case 'M':
                sscanf(optarg, "0x%x", &edm_dimb);
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
				//gdb_port_str = malloc(AICE_MAX_NUM_CORE * 6);
				gdb_port_str = malloc(optarg_len + 1);
				memcpy(gdb_port_str, optarg, optarg_len + 1);
				break;
			case 'P':
				optarg_len = strlen(optarg);
				nds32_edm_passcode_init = malloc(optarg_len + 1);
				memcpy(nds32_edm_passcode_init, optarg, optarg_len + 1);
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
			case 'X': /* uncnd-reset-hold, the same as 'H' */
				startup_reset_halt = 1;
				break;
			case 'z':
				aceconf_desc_list = optarg;
				break;
			case 'y':
				custom_def_idlm_base = 1;
				sscanf(optarg, "0x%x,0x%x,0x%x,0x%x", &ilm_base, &ilm_size, &dlm_base, &dlm_size);
				break;
			case 'Z':
				optarg_len = strlen(optarg);
				if (strncmp(optarg, "v2", optarg_len) == 0) {
					target_type[0] = TARGET_V2;
				} else if (strncmp(optarg, "v3", optarg_len) == 0) {
					target_type[0] = TARGET_V3;
				} else if (strncmp(optarg, "v3m", optarg_len) == 0) {
					target_type[0] = TARGET_V3m;
				} else if (strncmp(optarg, "v5", optarg_len) == 0) {
					target_type[0] = TARGET_V5;
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
	return ERROR_OK;
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
	"0",
};

static char *clock_v5_hz[] = {
	"30000",
	"15000",
	"7500",
	"3750",
	"1875",
	"909",
	"461",
	"232",
	"10000",
	"10000",
	"10000",
	"6000",
	"3000",
	"1500",
	"750",
	"375",
	"10000",	// Default
};

static FILE *openocd_cfg_tpl = NULL;
static FILE *openocd_cfg = NULL;
static FILE *interface_cfg_tpl = NULL;
static FILE *interface_cfg = NULL;
static FILE *board_cfg_tpl = NULL;
static FILE *board_cfg = NULL;
static FILE *target_cfg_tpl = NULL;
static FILE *target_cfg[AICE_MAX_NUM_CORE];
static FILE *openocd_cfg_usrdef = NULL;
char target_cfg_name[64];
char *target_cfg_name_str = (char *)&target_cfg_name[0];
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

	for (coreid = 0; coreid < 1; coreid++){
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
		sprintf(target_cfg_name_str, target_str, coreid);
		sprintf(line_buffer, target_str, coreid);
		target_cfg[coreid] = fopen(line_buffer, "w");
	}
	openocd_cfg_usrdef = fopen(NDS32_USER_CFG, "a");
}

static void close_config_files(void) {
	int coreid;

    if (openocd_cfg_tpl != NULL)
    	fclose(openocd_cfg_tpl);
	
    if(openocd_cfg != NULL)
        fclose(openocd_cfg);

    if(interface_cfg_tpl != NULL)
    	fclose(interface_cfg_tpl);
	
    if(interface_cfg != NULL)
        fclose(interface_cfg);
	
    if(board_cfg_tpl != NULL)
        fclose(board_cfg_tpl);
	
    if(board_cfg != NULL)
        fclose(board_cfg);
	
    if(target_cfg_tpl != NULL)
        fclose(target_cfg_tpl);
	
    for (coreid = 0; coreid < 1; coreid ++) {
	    if(target_cfg[coreid] != NULL)
            fclose(target_cfg[coreid]);
    }

    if(openocd_cfg_usrdef != NULL)
    	fclose(openocd_cfg_usrdef);
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

		nds32_edm_ops[nds32_edm_ops_num].reg_no = reg_no;
		nds32_edm_ops[nds32_edm_ops_num].data = value;
		nds32_edm_ops_num++;
		if (nds32_edm_ops_num >= 64)
			break;

		if (end_ptr[0] == ';')
			break;
		else if (end_ptr[0] == ',')
			processing_str = end_ptr + 1;
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
	int port_num=AICE_MAX_NUM_PORTS, port_id=PORTNUM_GDB;
	int port_id_start=0, port_id_end=0, i;
	char *port_str;

	if(gdb_port_str) {
		port_str = strtok(gdb_port_str, ":");
		if (port_str != NULL) {
			port_id_start = strtol(port_str, NULL, 0);
			port_id = port_id_start;
			port_str = strtok (NULL, ":");
			if (port_str != NULL)
				port_id_end = strtol(port_str, NULL, 0);

			if (port_id_end >= port_id_start) {
				port_num = (port_id_end - port_id_start) + 1;
				if (port_num > AICE_MAX_NUM_PORTS)
					port_num = AICE_MAX_NUM_PORTS;
			}
		}
	}
	total_num_of_ports = port_num;
	for (i=0; i<port_num; i++){
		gdb_port[i] = port_id;
		port_id++;
	}
	//printf("total_num_of_ports %x, gdb_port=%x\n", total_num_of_ports, gdb_port[0]);
}

static void update_openocd_cfg_v5(void)
{
	char line_buffer[LINE_BUFFER_SIZE];

	openocd_cfg_tpl = fopen("openocd.cfg.v5", "r");
	openocd_cfg = fopen("openocd.cfg", "w");
	if ((openocd_cfg_tpl == NULL) || (openocd_cfg == NULL)) {
		fprintf(stderr, "ERROR: No config file, openocd.cfg.v5\n");
		exit(-1);
	}
	/* update openocd.cfg */
	if( log_output == NULL )
		fprintf(openocd_cfg, "log_output iceman_debug0.log\n");
	else {
		memset(line_buffer, 0, LINE_BUFFER_SIZE);
		strncpy(line_buffer, log_output, strlen(log_output));
		strncat(line_buffer, "iceman_debug0.log", 17);
		fprintf(openocd_cfg, "log_output %s\n", line_buffer);
	}
	fprintf(openocd_cfg, "debug_level %d\n", debug_level);
	fprintf(openocd_cfg, "gdb_port %d\n", gdb_port[0]);
	fprintf(openocd_cfg, "telnet_port %d\n", telnet_port);
	fprintf(openocd_cfg, "tcl_port %d\n", tcl_port);
	fprintf(openocd_cfg, "adapter_khz %s\n", clock_v5_hz[clock_setting]);

	while (fgets(line_buffer, LINE_BUFFER_SIZE, openocd_cfg_tpl) != NULL)
		fputs(line_buffer, openocd_cfg);

	fprintf(openocd_cfg, "nds configure log_file_size %d\n", log_file_size);
	fprintf(openocd_cfg, "nds configure desc Andes_%s_BUILD_ID_%s\n", ICEMAN_VERSION, BUILD_ID);
	fprintf(openocd_cfg, "nds configure burn_port %d\n", burner_port);
	fprintf(openocd_cfg, "nds boot_time %d\n", boot_time);
	fprintf(openocd_cfg, "nds reset_time %d\n", reset_time);
	if (diagnosis)
		fprintf(openocd_cfg, "nds diagnosis 0x%x 0x%x\n", diagnosis_memory, diagnosis_address);
	if (count_to_check_dbger)
		fprintf(openocd_cfg, "nds count_to_check_dm %d\n", count_to_check_dbger);

	//interface_cfg_tpl = fopen("interface/olimex-arm-usb-tiny-h.cfg", "r");

	if (startup_reset_halt == 1)
		fprintf(openocd_cfg, "nds reset_halt_as_init on\n");
}

static void update_openocd_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];

	/* update openocd.cfg */
	if( log_output == NULL )
		fprintf(openocd_cfg, "log_output iceman_debug0.log\n");
	else {
		memset(line_buffer, 0, LINE_BUFFER_SIZE);
		strncpy(line_buffer, log_output, strlen(log_output));
		strncat(line_buffer, "iceman_debug0.log", 17);
		fprintf(openocd_cfg, "log_output %s\n", line_buffer);
	}
	fprintf(openocd_cfg, "debug_level %d\n", debug_level);
	fprintf(openocd_cfg, "gdb_port %d\n", gdb_port[0]);
	fprintf(openocd_cfg, "telnet_port %d\n", telnet_port);
	fprintf(openocd_cfg, "tcl_port %d\n", tcl_port);

	//fprintf(openocd_cfg, "source [find interface/nds32-aice.cfg] \n");
	//fprintf(openocd_cfg, "source [find board/nds32_xc5.cfg] \n");
	while (fgets(line_buffer, LINE_BUFFER_SIZE, openocd_cfg_tpl) != NULL)
		fputs(line_buffer, openocd_cfg);

	fprintf(openocd_cfg, "nds log_file_size %d\n", log_file_size);
	if (custom_def_idlm_base)
		fprintf(openocd_cfg, "nds idlm_base_size %d %d %d %d\n", ilm_base, ilm_size, dlm_base, dlm_size);

	if (global_stop)
		fprintf(openocd_cfg, "nds global_stop on\n");
	else
		fprintf(openocd_cfg, "nds global_stop off\n");

	if (startup_reset_halt == 1)
		fprintf(openocd_cfg, "nds reset_halt_as_init on\n");

	fprintf(openocd_cfg, "nds boot_time %d\n", boot_time);
	fprintf(openocd_cfg, "nds reset_time %d\n", reset_time);

	if (word_access_mem)
		fprintf(openocd_cfg, "nds word_access_mem on\n");
	fprintf(openocd_cfg, "source [find %s] \n", NDS32_USER_CFG);
}

static void update_interface_cfg(void)
{
	char line_buffer[LINE_BUFFER_SIZE];

	/* update nds32-aice.cfg */
	while (fgets(line_buffer, LINE_BUFFER_SIZE, interface_cfg_tpl) != NULL)
		fputs(line_buffer, interface_cfg);
	fprintf(interface_cfg, "aice desc Andes_%s_BUILD_ID_%s\n", ICEMAN_VERSION, BUILD_ID);
	if (diagnosis)
		fprintf(interface_cfg, "aice diagnosis 0x%x 0x%x\n", diagnosis_memory, diagnosis_address);
	fprintf(interface_cfg, "adapter_khz %s\n", clock_hz[clock_setting]);
	fprintf(interface_cfg, "aice retry_times %d\n", aice_retry_time);
	fprintf(interface_cfg, "aice no_crst_detect %d\n", aice_no_crst_detect);
	fprintf(interface_cfg, "aice port_config %d %d %s\n", burner_port, total_num_of_ports, target_cfg_name_str);

	if (count_to_check_dbger)
		fprintf(interface_cfg, "aice count_to_check_dbger %d %d\n", count_to_check_dbger, clock_setting);
	else
		fprintf(interface_cfg, "aice count_to_check_dbger 500 %d\n", clock_setting);

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
	if (startup_reset_halt) {
		if (soft_reset_halt)
			fprintf(interface_cfg, "aice reset_halt_as_init %d\n", soft_reset_halt);
		else
			fprintf(interface_cfg, "aice reset_halt_as_init 1\n");
	}
	if (reset_aice_as_startup) {
		fprintf(interface_cfg, "aice reset_aice_as_startup\n");
	}
	if (edm_dimb != DIMBR_DEFAULT) {
		fprintf(interface_cfg, "aice edm_dimb 0x%x\n", edm_dimb);
	}
	unsigned int i;

	for (i=0; i<4; i++) {
		if (cop_reg_nums[i] != 0) {
			fprintf(interface_cfg, "aice misc_config cop %d %d 1\n", i, cop_reg_nums[i]);
		}
	}
	if (use_sdm == 1) {
		fprintf(interface_cfg, "aice sdm use_sdm\n");
		//fprintf(interface_cfg, "aice misc_config usb_timeout 1000\n");
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

	for (coreid = 0; coreid < 1; coreid ++){
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
			for (coreid = 0; coreid < 1; coreid++){
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
/*
	if (nds32_edm_ops_num > 0) {
		for (i = 0 ; i < nds32_edm_ops_num ; i++) {
			fprintf(board_cfg, "nds32.cpu0 nds login_edm_operation 0x%x 0x%x\n", nds32_edm_ops[i].reg_no, nds32_edm_ops[i].data);
		}
	}
*/
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

	for (coreid = 0; coreid < 1; coreid++) {
		if (stop_sequences_num > 0) {
			fprintf(board_cfg, "nds32.cpu%d configure -event halted {\n", coreid);
			fprintf(board_cfg, "\tnds32.cpu%d nds mem_access bus\n", coreid);
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
			fprintf(board_cfg, "\tnds32.cpu%d nds mem_access cpu\n", coreid);
			fputs("}\n", board_cfg);
		}
		if (resume_sequences_num > 0) {
			fprintf(board_cfg, "nds32.cpu%d configure -event resumed {\n", coreid);
			fprintf(board_cfg, "\tnds32.cpu%d nds mem_access bus\n", coreid);
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
			fprintf(board_cfg, "\tnds32.cpu%d nds mem_access cpu\n", coreid);
			fputs("}\n", board_cfg);
		}
	}
}

int main(int argc, char **argv) {
	int i;
	char *openocd_argv[6] = {0, 0, 0, 0, 0, 0};

	for(i = 0; i < AICE_MAX_NUM_CORE; i++)
		target_type[i] = TARGET_V3;
	if (parse_param(argc, argv) != ERROR_OK)
		return 0;
#if 0
	if(diagnosis){
		do_diagnosis(diagnosis_memory, diagnosis_address);
		return 0;
		//printf("do_diagnosis %x, %x\n", diagnosis_memory, diagnosis_address);
	}
	if (target_probe() != ERROR_OK)
		return 0;
#endif

	/* prepare all valid port num */
	update_gdb_port_num();

	for(i = 0; i < total_num_of_ports; i++)
	{
	  nds32_registry_portnum_without_bind(gdb_port[i]);
	}

	burner_port = nds32_registry_portnum(burner_port);
	if(burner_port < 0)
	{
		printf("burner port num error\n");
		return 0;
	}

	telnet_port = nds32_registry_portnum(telnet_port);
	if(telnet_port < 0)
	{
		printf("telnet port num error\n");
		return 0;
	}
	tcl_port = nds32_registry_portnum(tcl_port);
	if(tcl_port < 0)
	{
		printf("tcl port num error\n");
		return 0;
	}
	printf("Andes ICEman %s (OpenOCD) BUILD_ID: %s\n", ICEMAN_VERSION, BUILD_ID);
	printf("Burner listens on %d\n", burner_port);
	printf("Telnet port: %d\n", telnet_port);
	printf("TCL port: %d\n", tcl_port);

	//printf("gdb_port[0]=%d, burner_port=%d, telnet_port=%d, tcl_port=%d .\n", gdb_port[0], burner_port, telnet_port, tcl_port);
	if (target_type[0] == TARGET_V5) {
		update_openocd_cfg_v5();
	} else {
		open_config_files();
		update_openocd_cfg();
		update_interface_cfg();
		update_target_cfg();
		update_board_cfg();
	}
	/* close config files */
	close_config_files();

	openocd_argv[0] = "openocd";
	openocd_argv[1] = "-d-3";
	//openocd_argv[2] = "-l";
	//openocd_argv[3] = "iceman_debug.log";

	/* reset "optind", for the next getopt_long() usage */
	optind = 1;
	openocd_main(2, openocd_argv);
	//printf("return from openocd_main WOW!\n");
	return 0;
}
