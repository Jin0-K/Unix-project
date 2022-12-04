#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char* argv[]){

	/*
	printf("argc: %d\n", argc);
	printf("argv: \n");
	for(int i=0; i<argc; i++){
		printf("%s\n", argv[i]);
	}
	*/

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

	while(fgets(buf, BUFSIZ, fpin) != NULL){
		printf("%s", buf);
		sleep(1);
	}


	return 0;
}
