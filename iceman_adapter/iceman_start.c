#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libgen.h>
#include <signal.h>

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); \
	                               } while (0)
pid_t child;
static void sigintHandler() {
	if (child != 1)
		kill(child, SIGKILL);
}

pid_t spawnChild(const char* program, char** arg_list)
{
	pid_t ch_pid = fork();
	if (ch_pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (ch_pid == 0) {
		execvp(program, arg_list);
		perror("execvp");
		exit(EXIT_FAILURE);
	} else
		return ch_pid;
}

int main(int argc, char* argv[]) 
{
	if (signal(SIGINT, sigintHandler) == SIG_ERR)
		errExit("signal SIGINT");

	int i, wstatus;
	char** child_argv;
	
	child_argv = malloc(sizeof(char *) * (argc + 1));
	for (i = 0; i < argc; i++)
		child_argv[i] = malloc(256);

	sprintf(child_argv[0], "%s/INICEman", dirname(argv[0]));

	for (i = 1; i < argc; i++)
		child_argv[i] = argv[i];
	child_argv[i] = NULL;

	child = spawnChild(child_argv[0], child_argv);
	if (waitpid(child, &wstatus, 0) == -1) {
		perror("waitpid");
		exit(EXIT_FAILURE);
	}
	child = 1;

	if (WEXITSTATUS(wstatus) != 8 && WEXITSTATUS(wstatus) != 9)
		exit(EXIT_FAILURE);

	char **retry_argv;
		
	retry_argv = malloc(sizeof(char *) * (argc + 3));
	for (i = 0; i < (argc + 2); i++)
		retry_argv[i] = malloc(256);

	for (i = 0; i < argc; i++)
		retry_argv[i] = child_argv[i];

	sprintf(retry_argv[i], "-I");
	if (WEXITSTATUS(wstatus) == 8)
		sprintf(retry_argv[i+1], "aice_micro_sdp.cfg");
	else
		sprintf(retry_argv[i+1], "aice_sdp.cfg");
	retry_argv[i+2] = NULL;

	child = spawnChild(retry_argv[0], retry_argv);
	
	if (waitpid(child, &wstatus, 0) == -1) {
		perror("waitpid");
		exit(EXIT_FAILURE);
	}
	child = 1;

	return WEXITSTATUS(wstatus);
}
