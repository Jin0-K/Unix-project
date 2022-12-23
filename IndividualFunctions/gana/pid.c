#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int main(void){

	pid_t pids1[2];
	int pids2[2];

	pids1[0] = getpid();
	pids2[0] = getpid();

	printf("%d, %d", pids1[0], pids2[0]);

}
