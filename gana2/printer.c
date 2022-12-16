#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include "data.h"

void printStdout(char* file);
void printNewFile(char* file);

int msgid; // 메세지큐 식별자
int shmid; // 공유메모리 식별자


// 출력 시그널
// SIGUSR1
void print_handler(int signo){

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
	//printf("received msg: %s, type: %ld, len: %ld\n", rcvMsg.mtext, rcvMsg.mtype,  strlen(rcvMsg.mtext));
	
	// 출력 유형에 따라
	switch(rcvMsg.mtype){
		case NEWFILE:
			printNewFile(rcvMsg.mtext);
			break;
		case STDOUT:
			
			printStdout(rcvMsg.mtext);
			break;
		case FAX:
			printf("fax!!\n");
			break;
		default:
			perror("fail to switch..\n");
			exit(1);
			break;
	}	
}

void printStdout(char* file){

	// 출력할 파일 열기
	FILE* fpin = fopen(file, "r");
	if(fpin == NULL){
		perror("fopen");
		exit(1);
	}
	
	char buf[BUFSIZ];
	char* shmaddr;
	
	// 파일 내용 출력.
	shmaddr = (char*) shmat(shmid, (char*)NULL, 0);
	strcpy(shmaddr, "--------------------------\n");
	shmdt((char*) shmaddr);
	kill(getppid(), SIGPOLL);
	usleep(70000);
	
	while(fgets(buf, BUFSIZ, fpin) != NULL){
		shmaddr = (char*) shmat(shmid, (char*)NULL, 0);
		strcpy(shmaddr, buf);
		shmdt((char*) shmaddr);
		//kill(getppid(), SIGTSTP);
		kill(getppid(), SIGPOLL);
		
		usleep(70000);
	}
	
	shmaddr = (char*) shmat(shmid, (char*)NULL, 0);
	char completeBuf[80];
	sprintf(completeBuf, "--------------------------\nComplete: %s\n", file);
	strcpy(shmaddr, completeBuf);
	shmdt((char*) shmaddr);
	kill(getppid(), SIGPOLL);
	usleep(70000);


	// 파일 닫기
	fclose(fpin);
	
	return;
}

void printNewFile(char* file){

	/* 시간 */
	time_t tloc;

	time(&tloc);
	/* 시간 끝 */

	/* 메모리 맵핑 */
	int fd;
	caddr_t addr;
	struct stat statbuf;

	char* filePath = file;

	if(stat(filePath, &statbuf) == -1){
		perror("stat");
		exit(1);
	}

	if((fd = open(filePath, O_RDWR)) == -1){
		perror("open");
		exit(1);
	}

	addr = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)0);
	if(addr == MAP_FAILED){
		perror("mmap");
		exit(1);
	}
	close(fd);
	/* 메모리 맵핑 끝 */


	/* 새로운 파일의 이름만들기 */
	char* str = file;

	char* extension = (char*) malloc(sizeof(char) * strlen(str));
	strcpy(extension, str);
	extension = strchr(extension, '.');
	//printf("ex: %s\n", extension);

	char* newFileName = strtok(str, ".");

	char timeBuf[100];
	sprintf(timeBuf, "-%d-copy", (int)tloc);
	newFileName = strcat(newFileName, timeBuf);

	newFileName = strcat(newFileName, extension);
	/* 새로운 파일의 이름만들기 끝 */

	/* 복사본 파일 만들기 */
	FILE* fpout;
	fpout = fopen(newFileName, "w");

	fputs(addr, fpout);

	fclose(fpout);
	/* 복사본 파일 만들기 끝 */

	return;
}

int main(int argc, char* argv[]){
	
	
	if(argc < 2){
		perror("not enough argc");
		exit(1);
	}
	
	// 메세지큐 
	key_t key;

	key = ftok(keyfile, keynum);
	if((msgid = msgget(key, 0)) < 0){
		perror("msgget");
		exit(1);
	}
	
	// 공유메모리
	shmid = atoi(argv[1]);
	
	//printf("in printer");

	// 시그널 핸들러 등록
	signal(SIGUSR1, print_handler);
	
	while(1){
		pause();
	}
	
	return 0;
}	
