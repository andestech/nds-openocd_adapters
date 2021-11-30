#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	int i, ret, offset = 0;
	char cmd[512];

	offset += sprintf(cmd + offset, "./INICEman ");
	for (i = 1; i < argc; i++)
		offset += sprintf(cmd + offset, "%s ", argv[i]);
	ret = system(cmd);
	if (WEXITSTATUS(ret) == 8) {
		offset += sprintf(cmd + offset, "-I aice_micro_sdp.cfg ");
		system(cmd);
	} else if (WEXITSTATUS(ret) == 9) {
		offset += sprintf(cmd + offset, "-I aice_sdp.cfg ");
		system(cmd);
	}
	return 0;
}
