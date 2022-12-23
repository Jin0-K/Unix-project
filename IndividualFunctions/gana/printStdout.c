#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>

void kill_ctrl_z_handler(int signo){
	printf("---------------------------\n");
	printf("출력 취소\n");
	exit(0);
}

int main(int argc, char* argv[]){

	/*
	printf("argc: %d\n", argc);
	printf("argv: \n");
	for(int i=0; i<argc; i++){
		printf("%s\n", argv[i]);
	}
	*/

	signal(SIGTSTP, kill_ctrl_z_handler);

	if(argc < 2){
		perror("argc is not enough");
		exit(1);
	}

	FILE* fpin;

	fpin = fopen(argv[1], "r");
	if(fpin == NULL){
		perror("fopen");
		exit(1);
	}

	char buf[BUFSIZ];

	/*
	int c;
	while((c = fgetc(fpin)) != EOF){
		if(c == '\n'){
			sleep(1);
		}
		printf("%c", c);
		//sleep(0.3);
	}
	*/

	printf("--------------------------\n");
	while(fgets(buf, BUFSIZ, fpin) != NULL){
		printf("%s", buf);
		sleep(1);
	}
	printf("--------------------------\n");
	printf("출력 완료: %s\n", argv[1]);

	kill(getppid(), SIGUSR2);
	
	return 0;
}

