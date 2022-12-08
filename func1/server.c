
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
#define MAX_CLIENT 4
#define STR_LEN 256

struct client_pid {
	pid_t list[MAX_CLIENT];
	int qid[MAX_CLIENT];
	int size;
};

void sig_handler(int signo); 
int is_in(int *arr, int size, int element); 
int add_pid(struct client_pid *pid_list, struct content message); 
int delete_pid(struct client_pid *pid_list, struct content message); 
int send_pid_list(struct client_pid pid_list, struct content message, struct msgbuf *msg_send); 
int is_shmaddr(struct content message); 
int qid_of_pid(struct client_pid pid_list, pid_t pid); 


int main() {
	void (*hand)(int);
	struct msgbuf message;
	struct client_pid pid_list;
	pid_list.size = 0;
	int qid; // message queue id to receive
	key_t key = ftok(".", KEY_NUM); // key for IPC

	// Signal handler for Ctrl + c
	hand = signal(SIGINT, sig_handler);
	if (hand == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	/*
	// create shared memory
	int shmid = shmget(key, SHARED_MEMORY_SIZE, IPC_CREAT|0666);
	void *shmaddr = shmat(shmid, NULL, 0); // will be used by listener client
	*/

	// create message queue for reception
	if ((qid = msgget(key, IPC_CREAT|0640)) < 0) {
		perror("msgget");
		exit(1);
	}

	system ("ipcs -q");
	int msg_len;
	while(1) {
		// get message
		msg_len = msgrcv(qid, &message, sizeof(struct msgbuf), 1, 0);
		if (msg_len < 0) { // if message cannot be received
                        printf("Message cannot be received\n");
			/* delete the message in the queue */
                        continue;
                }
                else {
                        printf("Received pid: %d, qid: %d, len = %d\n",
                                message.msgcontent.pid, 
				message.msgcontent.qid,
				msg_len);
                }

		int stat;
                // operation by each message type 
                switch(message.msgcontent.type) {
                        case REGISTER :
				stat = add_pid(&pid_list, message.msgcontent);
				if (stat == -1) {
					message.msgcontent.type = UNREGISTER;
				}
				// Send client whether the cilent has been registered
				msgsnd(message.msgcontent.qid, 
					(void *)&message, sizeof(struct msgbuf), 0);
				break;

			case UNREGISTER :
				delete_pid(&pid_list, message.msgcontent);
				break;

			case GET_PIDS : ;
				struct msgbuf msg_to_send;
				msg_to_send.mtype = 1;
			        msg_to_send.msgcontent = (struct content) { 
							.type = GET_PIDS,
	                      			        .pid = pid_list.list[0],
                                                	.qid = 0,
                                                	.addr = (caddr_t)NULL };
				
				send_pid_list(pid_list, message.msgcontent, &msg_to_send);
				break;

			case SEND_FAX :
				// allocate shared memory
				if (!is_shmaddr(message.msgcontent)) {
					message.msgcontent.addr = mmap(
						NULL, 
						SHARED_MEMORY_SIZE, 
						PROT_READ|PROT_WRITE, 
						MAP_SHARED|MAP_ANONYMOUS,
						0, 0);
				}
				// send the address of shared memory
				msgsnd(qid_of_pid(pid_list, message.msgcontent.pid), 
					(void *)&message, sizeof(struct msgbuf), 0);
				
				break;

			case GET_FAX :
				break;
			
			default :
				break;
		}
	
		printf("Current pid list\n");
		for (int i = 0; i < pid_list.size; i++) {
			printf("pid: %d, qid: %d\n",
				(int)(pid_list.list[i]), pid_list.qid[i]);
		}
	}

	system("ipcrm --all=msg"); // remove all message queues
	return 0;
}


void sig_handler(int signo) {
	printf("\n====End server\n");
	system("ipcrm --all=msg"); // remove all message queues
	exit(1);
}

// check if the element is in the function
int is_in(int *arr, int size, int element) {
	for (int i = 0; i < size; i++) {
		if (arr[i] == element) {
			return 1;
		}
	}
	return 0;
}

// add a process in pid list
// return the size of pid_list
int add_pid(struct client_pid *pid_list, struct content message) {
	static int client_key = 2;

	// return -1 if the pid_list is full
	if (pid_list->size == MAX_CLIENT) {
		return -1;
	}

	// add pid in the list
	pid_list->list[pid_list->size] = message.pid;
	/* 
	I made useless code by misunderstanding, but not gonna delete it just in case
	// give the added pid new key num for message queue
	do {
		if (++client_key > 255) {
			client_key = 2;
		}
	} while (is_in(pid_list->key_num, pid_list->size, client_key)); 
	pid_list->key_num[pid_list->size] = client_key;
	*/

	// add qid in the list
	pid_list->qid[pid_list->size] = message.qid;

	return ++(pid_list->size);
}

// delete a process from pid list
// return the size of pid_list
int delete_pid(struct client_pid *pid_list, struct content message) {
	// return -1 if pid_list is empty
	if (pid_list->size < 1) {
		return -1;
	}

	int index;
	// find the pid index
	for (index = 0; index < pid_list->size; index++) {
		if (pid_list->list[index] == message.pid) {
			break;
		}
	}
	while ((index+1) < pid_list->size) {
		pid_list->list[index] = pid_list->list[index+1];
		pid_list->qid[index] = pid_list->qid[index+1];
		index++;
	}
	msgctl(message.qid, IPC_RMID, (struct msqid_ds *)NULL);
	
	return --(pid_list->size);
}

// send pid in pid list one by one
// return the number of messages sent
int send_pid_list(struct client_pid pid_list, struct content message, struct msgbuf *msg_send) {
	int i;
	for (i = 0; i < pid_list.size; i++) {
		if (pid_list.list[i] == message.pid) {
			continue;
		}
		msg_send->msgcontent.pid = pid_list.list[i];
		msg_send->msgcontent.qid = pid_list.qid[i];
		if (msgsnd(message.qid, (void *)msg_send, sizeof(struct msgbuf), 0) == -1) {
			perror("msgsnd");
			return -1;
		}
	}

	// let the receiver know that the pid list is all sent
	msg_send->msgcontent.pid = 0;
	if (msgsnd(message.qid, (void *)msg_send, sizeof(struct msgbuf), 0) == -1) {
		perror("msgsnd");
		return -1;
	}
	printf("pid list successfully sent to %d\n", (int)(message.pid));

	return --i;
}

// check if addr of the message is NULL
int is_shmaddr(struct content message) {
	if (message.addr == NULL) {
		return 0;
	}
	else {
		return 1;
	}
}

// find the given pid's qid from pid_list
int qid_of_pid(struct client_pid pid_list, pid_t pid) {
	for(int i = 0; i < pid_list.size; i++) {
		if (pid == pid_list.list[i]) {
			return pid_list.qid[i];
		}
	}
	return -1;
}
