#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "data.h"

void temp(char* program, char* arg);

pid_t printoutPid;
int msgid;

// 출력 취소 시그널
// SIGTSTP
void kill_printout_handler(int signo){
	// 없는 프세pid에 시그널 보내면 어떻게 되는지 알아보기.
	// 아니면 pid에 해당하는 프세가 있으면 시그널 보내기
	kill(printoutPid, SIGTSTP);
}

// 출력 완료되었다는 시그널
// SIGUSR2
void complete_print_handler(int signo){
	// 함수 변경 필요
	// sigrelse(SIGUSR1);
	// SIGUSR1 언블록킹
	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals, SIGUSR1);
	sigprocmask(SIG_UNBLOCK, &signals, (sigset_t*)NULL);
	printf("sig1 언블록킹\n");
}

// 출력 시그널
// SIGUSR1
void print_handler(int signo){
	// 다른 시그널 다 막아야하나?
	// SIGUSR1, SIGUSR2 블록킹 하기
	printf("SIG1, SIG2 블록킹\n");
	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals, SIGUSR1);
	sigaddset(&signals, SIGUSR2);
	sigprocmask(SIG_BLOCK, &signals, (sigset_t*)NULL);


	// 메세지 큐에 내용이 없다면 함수 끝내기.
	struct msqid_ds tmpbuf;
	msgctl(msgid, IPC_STAT, &tmpbuf);
	if(tmpbuf.msg_qnum <= 0){
		printf("msg num: %ld\n", tmpbuf.msg_qnum);
		return;
	}
	
	// 메세지 큐 맨 압의 내용 읽어오기
	int len;
	Msgbuf rcvMsg;
	len = msgrcv(msgid, &rcvMsg, SIZE, 0, IPC_NOWAIT);
	printf("received msg: %s, type: %ld, len: %ld\n", rcvMsg.mtext, rcvMsg.mtype,  strlen(rcvMsg.mtext));
	
	// 자식 프세 만들기
	switch(rcvMsg.mtype){
		case NEWFILE:
			temp("printNewFile", rcvMsg.mtext);
			break;
		case STDOUT:
			temp("printStdout", rcvMsg.mtext);
			break;
		case FAX:
			printf("fax!!\n");
			break;
		default:
			printf("fail to switch..\n");
			break;
	}

	// SIGUSR2 언블록킹
	
	sigemptyset(&signals);
	sigaddset(&signals, SIGUSR2);
	sigprocmask(SIG_UNBLOCK, &signals, (sigset_t*)NULL);
	printf("sig2 언블록킹\n");
	
}

void temp(char* program, char* arg){
	printf("progrma: %s, arg: %s\n", program, arg);
	pid_t pid;
        switch(pid = fork()){
                case -1:
                        perror("fork");
                        exit(1);
                        break;
                case 0:
                        printf("child - printer\n");
                        execl(program, program, arg, (char*)NULL);
                        break;
                default:
                        break;
        }
}

int main(void){
	
	key_t key;

	key = ftok(keyfile, keynum);
	if((msgid = msgget(key, 0)) < 0){
		perror("msgget");
		exit(1);
	}


	signal(SIGUSR1, print_handler);
	signal(SIGUSR2, complete_print_handler);

	printf("in printer\n");

	while(1){
		pause();
	}

	/*	
	int len;
	Msgbuf rcvMsg;
	len = msgrcv(msgid, &rcvMsg, SIZE, 0, IPC_NOWAIT);
	printf("received msg: %s, len: %ld\n", rcvMsg.mtext, strlen(rcvMsg.mtext));
	*/
}	
