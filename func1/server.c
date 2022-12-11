
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


struct client_pid {
	pid_t list[MAX_CLIENT];
	int qid[MAX_CLIENT];
	int size;
};

void sig_handler_ctrlc(int signo); 
int is_in(int *arr, int size, int element); 
int is_pid_in(struct client_pid clients, pid_t pid);
int add_pid(struct client_pid *pid_list, struct cont message); 
int delete_pid(struct client_pid *pid_list, struct cont message); 
int send_pid_list(struct client_pid pid_list, struct cont message, struct message *msg_send); 
int qid_of_pid(struct client_pid pid_list, pid_t pid); 


int main() {
	void (*hand)(int);
	struct message message;
	struct client_pid pid_list;
	pid_list.size = 0;
	int qid; // message queue id to receive
	key_t key = ftok(".", KEY_NUM); // key for IPC

	// Signal handler for Ctrl + c
	hand = signal(SIGINT, sig_handler_ctrlc);
	if (hand == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	// create message queue for reception
	if ((qid = msgget(key, IPC_CREAT|0640)) < 0) {
		perror("msgget");
		exit(1);
	}

	int msg_len;
	while(1) {
		// get message
		msg_len = msgrcv(qid, &message, sizeof(struct message), 1, 0);
		if (msg_len == -1) { // if message cannot be received
                        printf("Message cannot be received\n");
			/* delete the message in the queue */
                        continue;
                }
                else {
                        printf("Received pid: %d, qid: %d, len = %d\n",
                                message.content.pid, 
				message.content.qid,
				msg_len);
                }

		int stat;
                // operation by each message type 
                switch(message.content.type) {
                        case REGISTER :
				stat = add_pid(&pid_list, message.content);
				if (stat == -1) {
					message.content.type = UNREGISTER;
				}
				// Send client whether the cilent has been registered
				msgsnd(message.content.qid, 
					(void *)&message, sizeof(struct message), 0);
				break;

			case UNREGISTER :
				delete_pid(&pid_list, message.content);
				break;

			case GET_PIDS : ;
				struct message msg_to_send;
				msg_to_send.mtype = 1;
			        msg_to_send.content = (struct cont) { 
							.type = GET_PIDS,
	                      			        .pid = 0,
                                                	.qid = 0 };
				
				send_pid_list(pid_list, message.content, &msg_to_send);
				break;

			case SEND_FAX :
				// if the receiver is not on the list
				if (is_pid_in(pid_list, message.content.pid) >= 0) {
					message.content.type = GET_FAX;
					// message will be sent to the process that will receive file
					msgsnd(qid_of_pid(pid_list, message.content.pid),
						(void *)&message, sizeof(struct message), 0);
				}
				break;

			case GET_FAX :
				// deallocate shm received
				break;
			
			default :
				break;
		}
	
		printf("### Current pid list\n");
		for (int i = 0; i < pid_list.size; i++) {
			printf("pid: %d, qid: %d\n",
				(int)(pid_list.list[i]), pid_list.qid[i]);
		}
	}

	system("ipcrm --all=msg"); // remove all message queues
	return 0;
}


void sig_handler_ctrlc(int signo) {
	printf("\n====End server\n");
	system("ipcrm --all=msg"); // remove all message queues
	exit(1);
}

// check if the element is in the function
int is_in(int *arr, int size, int element) {
	int i;
	for (i = 0; i < size; i++) {
		if (arr[i] == element) {
			return i;
		}
	}
	return -1;
}

// check if the pid is in the client list
int is_pid_in(struct client_pid clients, pid_t pid) {
	int i;
	for (i = 0; i < clients.size; i++) {
		if (clients.list[i] == pid) {
			return i;
		}
	}
	return -1;
}

// add a process in pid list
// return the size of pid_list
int add_pid(struct client_pid *pid_list, struct cont message) {
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
int delete_pid(struct client_pid *pid_list, struct cont message) {
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
int send_pid_list(struct client_pid pid_list, struct cont message, struct message *msg_send) {
	int i;
	for (i = 0; i < pid_list.size; i++) {
		if (pid_list.list[i] == message.pid) {
			continue;
		}
		msg_send->content.pid = pid_list.list[i];
		msg_send->content.qid = pid_list.qid[i];
		if (msgsnd(message.qid, (void *)msg_send, sizeof(struct message), 0) == -1) {
			perror("msgsnd");
			return -1;
		}
	}

	// let the receiver know that the pid list is all sent
	msg_send->content.pid = 0;
	if (msgsnd(message.qid, (void *)msg_send, sizeof(struct message), 0) == -1) {
		perror("msgsnd");
		return -1;
	}
	printf("pid list successfully sent to %d\n", (int)(message.pid));

	return --i;
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
