#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#ifdef __MINGW32__
#include <windows.h>
#include <process.h>
#endif

const char *opt_string = "AaDvBsIBhtHKgGjJkd:E:S:R:i:m:M:p:b:z:f:c:u:U:w:W:T:r:e:n:o:C:P:y:Y:F:O:l:L:N:z:";
struct option long_option[] = {
	{"test", no_argument, 0, 't'},
	{"help", no_argument, 0, 'h'},
	{"boot", no_argument, 0, 'B'},
	{"IceBoxCmd", no_argument, 0, 'I'},
	{"source", no_argument, 0, 's'},
	{"version", no_argument, 0, 'v'},
	{"unlimited-log", no_argument, 0, 'D'},
	{"reset-hold", no_argument, 0, 'H'},
	{"soft-reset-hold", no_argument, 0, 'K'},
	{"force-debug", no_argument, 0, 'g'},
	{"enable-virtual-hosting", no_argument, 0, 'j'},
	{"disable-virtual-hosting", no_argument, 0, 'J'},
	{"debug", required_argument, 0, 'd'},
	{"EDM", required_argument, 0, 'E'},
	{"stop-seq", required_argument, 0, 'S'},
	{"resume-seq", required_argument, 0, 'R'},
	{"icebox", required_argument, 0, 'i'},
	{"Mode", required_argument, 0, 'M'},
	{"p", required_argument, 0, 'p'},
	{"port", required_argument, 0, 'p'},
	{"bport", required_argument, 0, 'b'},
	{"ppp", required_argument, 0, 'f'},
	{"clock", required_argument, 0, 'c'},
	{"update-fw", required_argument, 0, 'u'},
	{"update-fpga", required_argument, 0, 'U'},
	{"backup-fw", required_argument, 0, 'w'},
	{"backup-fpga", required_argument, 0, 'W'},
	{"boot-time", required_argument, 0, 'T'},
	{"ice-retry", required_argument, 0, 'r'},
	{"edm-retry", required_argument, 0, 'e'},
	{"test-iteration", required_argument, 0, 'n'},
	{"reset-time", required_argument, 0, 'o'},
	{"check-times", required_argument, 0, 'C'},
	{"passcode", required_argument, 0, 'P'},
	{"reset-aice", no_argument, 0, 'a'},
	{"edm-port-file", required_argument, 0, 'F'},
	{"edm-port-operation", required_argument, 0, 'O'},
	{"enable-global-stop", no_argument, 0, 'G'},
	{"no-crst-detect", no_argument, 0, 'A'},
	{"word-access-mem", no_argument, 0, 'k'},
	{"custom-srst", required_argument, 0, 'l'},
	{"custom-trst", required_argument, 0, 'L'},
	{"custom-restart", required_argument, 0, 'N'},
	{"target", required_argument, 0, 'z'},
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

#define MAX_MEM_OPERATIONS_NUM 32

struct MEM_OPERATIONS stop_sequences[MAX_MEM_OPERATIONS_NUM];
struct MEM_OPERATIONS resume_sequences[MAX_MEM_OPERATIONS_NUM];
int stop_sequences_num = 0;
int resume_sequences_num = 0;

static char *memory_stop_sequence = NULL;
static char *memory_resume_sequence = NULL;
static int aice_retry_time = 10;
static int clock_setting = 8;
static int debug_level;
static int boot_code_debug;
static int gdb_port = 1111;
static int burner_port = 2354;
static int virtual_hosting;
static int startup_reset_halt;
static int soft_reset_halt;
static int force_debug;
static int unlimited_log;
static int boot_time = 2500;
static int reset_time;
static int reset_aice;
static int global_stop;
static int no_crst_detect;
static int word_access_mem;
static enum TARGET_TYPE target_type;

static void show_version(void) {
	printf ("Andes ICEman V2.0.2\n");
	printf ("Copyright (C) 2012 Andes Technology Corporation\n");
}

static void show_usage(void) {
	printf("Usage:\nICEman --port start_port_number[:end_port_number] [--help]\n");
	printf("ICEman --update-fw filename\n\n");

	printf("-h, --help:\t\tThe usage is for ICEman\n");
	printf("-p, --port:\t\tSocket port number for gdb connection\n");
	printf("-b, --bport:\t\tSocket port number for Burner connection\n");
	printf("\t\t\t(default: 2354)\n");
	printf("-v, --version:\t\tVersion of ICEman\n");
	printf("-j, --enable-virtual-hosting:\tEnable virtual hosting\n");
	printf("-J, --disable-virtual-hosting:\tDisable virtual hosting\n");
	printf("-B, --boot:\t\tReset-and-hold while connecting to target\n");
	printf("-D, --unlimited-log:\tDo not limit log file size to 512 KB\n");
	printf("-s, --source:\t\tShow commit ID of this version\n");
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
	printf("\t\t\tAICE-MCU only supports 8 ~ 15\n");
	printf("-r, --ice-retry:\tRetry count when AICE command timeout\n");
	printf("-T, --boot-time:\tBoot time of target board\n");
	printf("-S, --stop-seq:\t\tSpecify the SOC device operation sequence while CPU stop\n");
	printf("-R, --resume-seq:\tSpecify the SOC device operation sequence before CPU resume\n\n");
	printf("\t\tUsage: --stop-seq \"stop-seq address:data\"\n");
	printf("\t\t\t--resume-seq \"resume-seq address:{data|rst}\"\n");
	printf("\t\tExample: --stop-seq \"stop-seq 0x200800:0x80\"\n");
	printf("\t\t\t--resume-seq \"resume-seq 0x200800:rst\"\n\n");
	printf("-o, --reset-time:\tReset time of reset-and-hold\n");
	printf("-C, --check-times:\tCount to check DBGER\n");
	printf("-P, --passcode:\t\tPASSCODE of secure MPU\n");
	printf("-u, --update-fw:\tUpdate AICE F/W\n");
	printf("-U, --update-fpga:\tUpdate AICE FPGA\n");
	printf("-w, --backup-fw:\tBackup AICE F/W\n");
	printf("-W, --backup-fpga:\tBackup AICE FPGA\n");
	printf("-a, --reset-aice:\tReset AICE as ICEman startup\n");
	printf("-O, --edm-port-operation: EDM port0/1 operations\n");
	printf("\t\tUsage: -O \"write_edm 6:0x1234,7:0x5678);\"\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n");
	printf("-F, --edm-port-file:\tEDM port0/1 operations file name\n");
	printf("\t\tFile format:\n");
	printf("\t\t\twrite_edm 6:0x1234,7:0x1234);\n");
	printf("\t\t\twrite_edm 6:0x1111);\n");
	printf("\t\t\t6 for EDM_PORT0 and 7 for EDM_PORT1\n");
	printf("-G, --enable-global-stop: Enable 'global stop'.  As users use up hardware watchpoints, target stops at every load/store instructions. \n");
	printf("-A, --no-crst-detect:\tNo CRST detection in debug session\n");
	printf("-k, --word-access-mem:\tAlways use word-aligned address to access memory device\n");
	printf("-l, --custom-srst:\tUse custom script to do SRST\n");
	printf("-L, --custom-trst:\tUse custom script to do TRST\n");
	printf("-N, --custom-restart:\tUse custom script to do RESET-HOLD\n");
	printf("-z, --target:\tSpecify target type (v2/v3/v3m)\n");
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
				memory_stop_sequence = malloc(optarg_len + 1);
				memcpy(memory_stop_sequence, optarg, optarg_len + 1);
				break;
			case 'R':
				optarg_len = strlen(optarg);
				memory_resume_sequence = malloc(optarg_len + 1);
				memcpy(memory_resume_sequence, optarg, optarg_len + 1);
				break;
			case 'r':
				aice_retry_time = strtol(optarg, NULL, 0);
				break;
			case 'c':
				clock_setting = strtol(optarg, NULL, 0);
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
				break;
			case 'T':
				boot_time = strtol(optarg, NULL, 0);
				break;
			case 'o':
				reset_time = strtol (optarg, NULL, 0);
				break;
			case 'a':
				reset_aice = 1;
				break;
			case 'G':
				global_stop = 1;
				break;
			case 'A':
				no_crst_detect = 1;
				break;
			case 'k':
				word_access_mem = 1;
				break;
			case 'z':
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
				/*
			case 'F':
				{
					edm_port_file_ = optarg;
					set_edm_port_file_ = true;
				}
				break;
			case 'O':
				{
					edm_port_operation_ = optarg;
					set_edm_port_operation_ = true;
				}
				break;
			case 'l': // customer-srst
				customer_srst_file_ = optarg;
				customer_srst_ = true;
				break;
			case 'L': // customer-trst
				customer_trst_file_ = optarg;
				customer_trst_ = true;
				break;
			case 'N': // customer-restart
				customer_restart_file_ = optarg;
				customer_restart_ = true;
				break;
			case 'P':
				exist_pass_code_ = true;
				pass_code_ = optarg;
				pass_code_length_ = pass_code_.length ();
				break;
			case 'E':
				if (strncmp (optarg, "disable_auto", 12) == 0)
					auto_adjust_edm_ = false;
				else
					user_specified_edm_version_ = strtol(optarg, NULL, 0);
				break;
			case 'I':
				execute_aice_command_ = true;
				break;
			case 'f':
				polling_period_ = strtol(optarg, NULL, 0);
				break;
			case 'i':
				if (strncmp(optarg, "fw_ver", 6) == 0)
					aice_fw_version_ = strtol((optarg + 6), NULL, 0);
				else if (strncmp(optarg, "hw_ver", 6) == 0)
					aice_hw_version_ = strtol((optarg + 6), NULL, 0);
				else if (strncmp(optarg, "fpga_ver", 8) == 0)
					aice_fpga_version_ = strtol((optarg + 8), NULL, 0);
				break;
			case 'M':
				if (strncmp(optarg, "SMP", 3) == 0)
					SMP_mode_ = true;
				else
					SMP_mode_ = false;
				break;
			case 'u': // --update-fw
				field_programming_ = true;
				field_programming_type_ = FIELD_PROG_UPDATE_FW;
				strcpy (field_programming_filename_, optarg);
				break;
			case 'U': // --update-fpga
				field_programming_ = true;
				field_programming_type_ = FIELD_PROG_UPDATE_FPGA;
				strcpy (field_programming_filename_, optarg);
				break;
			case 'w': // --backup-fw
				field_programming_ = true;
				field_programming_type_ = FIELD_PROG_BACKUP_FW;
				strcpy (field_programming_filename_, optarg);
				break;
			case 'W': // --backup-fpga
				field_programming_ = true;
				field_programming_type_ = FIELD_PROG_BACKUP_FPGA;
				strcpy (field_programming_filename_, optarg);
				break;
			case 's':
				printf ("Branch%s\n", BRANCH_NAME);
				printf ("%s\n", COMMIT_ID);
				exit(0);
				*/
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
}

#define LINE_BUFFER_SIZE 1024

static char *clock_hz[] = {
	"30000",
	"15000",
	"7500",
	"3750",
	"1875",
	"937.5",
	"468.75",
	"234.375",
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
static FILE *target_cfg;

static open_config_files(void) {
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

static close_config_files(void) {
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
		stop_sequences[stop_sequences_num].address = strtol(mem_operation+9, &next_data, 0);
		stop_sequences[stop_sequences_num].data = strtol(next_data+1, &next_data, 0);
		if (*next_data == ':')
			stop_sequences[stop_sequences_num].mask = strtol(next_data+1, &next_data, 0);
		stop_sequences_num++;
	} else if (strncmp(mem_operation, "resume-seq ", 11) == 0) {
		resume_sequences[resume_sequences_num].address = strtol(mem_operation+11, &next_data, 0);
		if (*(next_data+1) == 'r')
			resume_sequences[resume_sequences_num].restore = 1;
		else {
			resume_sequences[resume_sequences_num].restore = 0;
			resume_sequences[resume_sequences_num].data = strtol(next_data+1, &next_data, 0);
			if (*next_data == ':')
				resume_sequences[resume_sequences_num].mask = strtol(next_data+1, &next_data, 0);
		}
		resume_sequences_num++;
	}
}

int main(int argc, char **argv) {
	char line_buffer[LINE_BUFFER_SIZE];

	parse_param(argc, argv);

	open_config_files();

	/* update openocd.cfg */
	while (fgets(line_buffer, LINE_BUFFER_SIZE, openocd_cfg_tpl) != NULL)
		fputs(line_buffer, openocd_cfg);

	fprintf(openocd_cfg, "gdb_port %d\n", gdb_port);
	printf("The core listens on %d.\n", gdb_port);
	fflush(stdout);

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

	/* update nds32-aice.cfg */
	while (fgets(line_buffer, LINE_BUFFER_SIZE, interface_cfg_tpl) != NULL)
		fputs(line_buffer, interface_cfg);

	fprintf(interface_cfg, "adapter_khz %s\n", clock_hz[clock_setting]);
	fprintf(interface_cfg, "aice retry_times %d\n", aice_retry_time);

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
				strcpy(find_pos - 1, "halt\n");
		}

		fputs(line_buffer, board_cfg);
	}

	/* open sw-reset-seq.txt */
	FILE *sw_reset_fd = NULL;
	char *next_data;
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

	int i;
	for (i = 0 ; i < stop_sequences_num ; i++) {
		fprintf(board_cfg, "set backup_value_%x \"\"\n", stop_sequences[i].address);
	}

	fputs("$_TARGETNAME configure -event halted {\n", board_cfg);
	fputs("\tnds32.cpu nds mem_access bus\n", board_cfg);
	for (i = 0 ; i < stop_sequences_num ; i++) {
		fprintf(board_cfg, "\tglobal backup_value_%x\n", stop_sequences[i].address);
		fprintf(board_cfg, "\tmem2array backup_value_%x 32 0x%x 1\n", stop_sequences[i].address, stop_sequences[i].address);
		fprintf(board_cfg, "\tmww 0x%x 0x%x\n", stop_sequences[i].address, stop_sequences[i].data);
	}
	fputs("\tnds32.cpu nds mem_access cpu\n", board_cfg);
	fputs("}\n", board_cfg);

	fputs("$_TARGETNAME configure -event resumed {\n", board_cfg);
	fputs("\tnds32.cpu nds mem_access bus\n", board_cfg);
	for (i = 0 ; i < resume_sequences_num ; i++) {
		if (resume_sequences[i].restore) {
			fprintf(board_cfg, "\tglobal backup_value_%x\n", resume_sequences[i].address);
			fprintf(board_cfg, "\tmww 0x%x $backup_value_%x(0)\n", resume_sequences[i].address, resume_sequences[i].address);
		} else {
			fprintf(board_cfg, "\tmww 0x%x 0x%x\n", resume_sequences[i].address, resume_sequences[i].data);
		}
	}
	fputs("\tnds32.cpu nds mem_access cpu\n", board_cfg);
	fputs("}\n", board_cfg);

	/* close config files */
	close_config_files();

	char *openocd_argv[4] = {0, 0, 0, 0};
	char *burner_adapter_argv[4] = {0, 0, 0, 0};

#ifdef __MINGW32__
	int burner_adapter_pid;
	int openocd_pid;

	char burner_port_num[12];
	burner_adapter_argv[0] = "burner_adapter.exe";
	burner_adapter_argv[1] = "-p";
	sprintf(burner_port_num, "%d", burner_port);
	burner_adapter_argv[2] = burner_port_num;
	burner_adapter_pid = _spawnv(_P_NOWAIT, "./burner_adapter.exe", (const char * const *)burner_adapter_argv);

	openocd_argv[0] = "openocd";
	if (unlimited_log)
		openocd_argv[1] = "-d";
	openocd_pid = _spawnv(_P_NOWAIT, "./openocd.exe", (const char * const *)openocd_argv);

	int burner_adapter_status;
	int openocd_status;
	_cwait(&burner_adapter_status, burner_adapter_pid, WAIT_CHILD);
	_cwait(&openocd_status, openocd_pid, WAIT_CHILD);
#else
	pid_t burner_adapter_pid;
	pid_t openocd_pid;

	burner_adapter_pid = fork();
	if (burner_adapter_pid < 0) {
		fprintf(stderr, "invoke burner_adapter failed\n");
		exit(127);
	} else if (burner_adapter_pid == 0) {
		/* invoke burner adapter */
		char burner_port_num[12];
		burner_adapter_argv[0] = "burner_adapter";
		burner_adapter_argv[1] = "-p";
		sprintf(burner_port_num, "%d", burner_port);
		burner_adapter_argv[2] = burner_port_num;

		execv("./burner_adapter", burner_adapter_argv);
		exit(127);
	}

	openocd_pid = fork();
	if (openocd_pid < 0) {
		fprintf(stderr, "invoke openocd failed\n");
		exit(127);
	} else if (openocd_pid == 0) {
		/* invoke openocd */
		openocd_argv[0] = "openocd";
		if (unlimited_log)
			openocd_argv[1] = "-d";

		execv("./openocd", openocd_argv);
		exit(127);
	}

	int burner_adapter_status;
	int openocd_status;
	if (waitpid(burner_adapter_pid, &burner_adapter_status, 0) < 0)
		fprintf(stderr, "wait burner_adapter failed\n");

	if (waitpid(openocd_pid, &openocd_status, 0) < 0)
		fprintf(stderr, "wait openocd failed\n");
#endif

	return 0;
}

