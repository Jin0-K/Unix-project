
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include "msgtype.h"

#define KEY_NUM 1
#define SHARED_MEMORY_SIZE 1024*1024
#define MAX_CLIENT 4
#define STR_LEN 256


struct client_pid {
	pid_t list[MAX_CLIENT];
	int size;
};

void sig_handler(int signo); 


int main() {
	void (*hand)(int);
	struct msgbuf message;
	struct client_pid pid_list;
	pid_list.size = 0;
	int qid; // message queue id to receive
	key_t key = ftok(".", KEY_NUM); // key for IPC

	hand = signal(SIGINT, sig_handler);
	if (hand == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	// create shared memory
	int shmid = shmget(key, SHARED_MEMORY_SIZE, IPC_CREAT|0666);
	void *shmaddr = shmat(shmid, NULL, 0); // will be used by listener client

	// create message queue for reception
	if ((qid = msgget(key, IPC_CREAT|0640)) < 0) {
		perror("msgget");
		exit(1);
	}

	system ("ipcs -q");
	int msg_len;
	//while(1) {
		// get message
	msg_len = msgrcv(qid, &message, sizeof(struct msgbuf), 0, 0);
	pid_list.list[pid_list.size++] = message.msgcontent.pid;
	printf("Received pid: %d, len = %d\n", 
		pid_list.list[pid_list.size - 1], msg_len);
		
	//}

	system ("ipcs -q");
	system("ipcrm --all=msg"); // remove all message queues
	return 0;
}


void sig_handler(int signo) {
	printf("\n====End server\n");
	system("ipcrm --all=msg"); // remove all message queues
	exit(1);
}
