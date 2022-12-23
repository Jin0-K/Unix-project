#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "data.h"

int main(void){

	key_t key;	

	key = ftok(keyfile, keynum);

	int msgid;
	
	msgid = msgget(key, IPC_CREAT|0664);
	if(msgid == -1){
		perror("msgget");
		exit(1);
	}

	/* 자식 프세로 프린터 만들기 */
	pid_t pid;
	int status;
	switch(pid = fork()){
		case -1:
			perror("fork");
			exit(1);
			break;
		case 0:
			printf("child - msgQsend\n");
			if(execl("printer", "printer", (char*)NULL) == -1){
				perror("execl");
			}
			break;
		default:
			printf("parent - msgQsend\n");
			break;
	}
	/* 자식 프세로 프린터 만들기 끝 */

	sleep(1);
	
	Msgbuf msgbuf;

        msgbuf.mtype = STDOUT;
        strcpy(msgbuf.mtext, "test2.txt");

        if(msgsnd(msgid, (void*)&msgbuf, SIZE, IPC_NOWAIT) == -1){
                perror("msgsnd");
                exit(1);
        }
	kill(pid, SIGUSR1);

	sleep(1);

        msgbuf.mtype = STDOUT;
        strcpy(msgbuf.mtext, "test2.txt");

        if(msgsnd(msgid, (void*)&msgbuf, SIZE, IPC_NOWAIT) == -1){
                perror("msgsnd");
                exit(1);
        }
	kill(pid, SIGUSR1);

	sleep(15);
	//printf("dsdfsdfsdf\n");
}
