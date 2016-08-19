#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	if(argc <= 1){
		fprintf(stderr, "Usage: %s command args...\n", argv[0]);
		return 127;
	}

	signal(SIGHUP, SIG_IGN);

	execvp(argv[1], argv + 1);

	fprintf(stderr, "exec %s: %s\n", argv[1], strerror(errno));
	return 127;
}
