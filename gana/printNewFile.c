#include <sys/types.h> //time(), open()
#include <time.h> //time()
#include <sys/stat.h> //open()
#include <fcntl.h> //open(), mmap()
#include <sys/mman.h> //mmap()
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(void){

	/* 시간 */
	time_t tloc;

	time(&tloc);
	printf("time: %d\n", (int)tloc);
	/* 시간 끝 */

	/* 파일 목록 */
	DIR* dir;
	struct dirent* ent;
	char* files[50];

	dir = opendir("./");
	if(dir == NULL){
		perror("opendir");
		exit(1);
	}
	
	int i=0;
	while((ent = readdir(dir)) != NULL){
		
		if(strcmp(ent->d_name, ".") == 0){
			continue;
		}
		else if(strcmp(ent->d_name, "..") == 0){
			continue;
		}
		else if(strchr(ent->d_name, '.') == NULL){
			continue;
		}
		files[i] = ent->d_name;
		i++;
		//printf("file: %s\n", ent->d_name);
	}

	for(int cnt=0; cnt<i; cnt++){
		printf("file: %s\n", files[cnt]);
	}
		
	/* 파일 목록 끝  */

	/* 메모리 맵핑 */	
	int fd;
	caddr_t addr;
	struct stat statbuf;

	char* filePath = "test.txt";

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

	printf("%s\n", addr);
	/* 메모리 맵핑 끝 */


	/* 새로운 파일의 이름만들기 */

	char str[30] = "test.txt";

	char* extension = (char*) malloc(sizeof(char) * strlen(str));
	strcpy(extension, str);
	extension = strchr(extension, '.');
	printf("ex: %s\n", extension);

	// 왜 .txt로 잘랐을 땐 안되는지 확인필요
	char* newFileName = strtok(str, ".");
	printf("new fileName: %s\n", newFileName);
	
	char timeBuf[100];
	sprintf(timeBuf, "-%d-복사본", (int)tloc);
	newFileName = strcat(newFileName, timeBuf);
	printf("new fileName2: %s\n", newFileName);
	
	newFileName = strcat(newFileName, extension);
	printf("new fileName3: %s\n", newFileName);

	/* 새로운 파일의 이름만들기 끝 */
	
	/* 복사본 파일 만들기 */
	FILE* fpout;
	fpout = fopen(newFileName, "w");
	
	fputs(addr, fpout);

	fclose(fpout);
	/* 복사본 파일 만들기 끝 */

	return 0;

}
	
