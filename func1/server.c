
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#define SHARED_MEMORY_SIZE 1024*1024
#define MAX_CLIENT 4
#define STR_LEN 256

int main() {
	// Client PID array
	pid_t client_pid[MAX_CLIENT];
	// create a key for IPC
	key_t key = ftok(".", (int)getpid());

	// create shared memory
	int shmid = shmget(key, SHARED_MEMORY_SIZE, IPC_CREAT|0666);
	void *shmaddr = shmat(shmid, NULL, 0); // will be used by listener client

	return 0;
}


