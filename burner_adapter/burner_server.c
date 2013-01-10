#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "burner_socket.h"
#include "burner_action.h"

/* global variables */
int burner_port = 2354;
int openocd_tcl_port = 6666;

static fd_set readfds;
static int burner_server_fd = -1;
static int burner_client_fd = -1;
static int max_fd = -1;

static void create_burner_server(void) {
	int retry_time = 0;

	FD_ZERO(&readfds);
	while (burner_server_fd < 0) {
		burner_server_fd = prepare_connect(burner_port);
		if ((retry_time > MAX_RETRY) || (burner_server_fd > 0))
			break;
		retry_time++;
	}

	if (burner_server_fd >= 0) {
		FD_SET((unsigned int)burner_server_fd, &readfds);
		if (burner_server_fd > max_fd)
			max_fd = burner_server_fd;
		printf("Burner listens on %d\n", burner_port);
	}
}

static void start_listen(void) {
	int n = 0;

	while (1) {
		n = select(max_fd + 1, &readfds, NULL, NULL, NULL);

		if (n < 0)
			continue;

		if (burner_client_fd < 0)
		{
			burner_client_fd = wait_connect();
			if (burner_client_fd < 0)
				continue;

			FD_SET((unsigned int)burner_client_fd, &readfds);
			if (burner_client_fd > max_fd)
				max_fd = burner_client_fd;

			init_tcl_client();
			send_cmd(TARGET_CMD_BUS_MODE);
			send_cmd(TARGET_CMD_POLL_OFF);
			continue;
		}
		else
		{
			int cmd_length;
			char cmd_buffer[2048];

			cmd_length = burner_receive(cmd_buffer, 2048);

			/* process packet */
			if (handle_packet(cmd_buffer, cmd_length) < 0)
			{
				send_cmd(TARGET_CMD_POLL_ON);
				send_cmd(TARGET_CMD_CPU_MODE);
				burner_client_fd = -1;
				close_connect();
				close_tcl_client();

				FD_ZERO(&readfds);
				FD_SET((unsigned int)burner_server_fd, &readfds);
				max_fd = burner_server_fd;
			}
		}
	}
}

static void usage(void) {
	printf("-p, --port:\t\t\tListening port number for burner (default: 2354)\n");
	printf("-t, --tcl-port:\t\t\tOpenOCD TCL port number (default: 6666)\n");
	printf("-h, --help:\t\t\tShow the help messages\n");
}

static const char *opt_string = "hp:t:";
static struct option long_option[] = 
{
	{"port", required_argument, 0, 'p'},
	{"tcl-port", required_argument, 0, 't'},
	{"help", no_argument, 0, 'h'},
	{0, 0, 0, 0}
};

static void parse_param(int argc, char **argv) {
	while(1) {
		int option_index;
		int c;
		c = getopt_long(argc, argv, opt_string, long_option, &option_index);
		if (c == EOF)
			break;

		switch (c)
		{
			case 'p':
				burner_port = strtol(optarg, NULL, 0);
				break;
			case 't':
				openocd_tcl_port = strtol(optarg, NULL, 0);
				break;
			case 'h':
			case '?':
			default:
				usage();
				exit(0);
				break;
		}
	}
}

int main(int argc, char **argv) {

	parse_param(argc, argv);

	create_burner_server();
	start_listen();

	return 0;
}
