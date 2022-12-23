#include <ncurses.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <wait.h>
#include <sys/ipc.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "data.h"

WINDOW* pLabelWin; // 출력 라벨 윈도우
int msgid; // 메세지 큐 식별자

void print(char* str);

// 메뉴 선택
int SelectMenu(WINDOW* win, char* menu[], int size)
{
	int direction;
	int highlight = 0;
	int i;

	box(win, 0, 0);
	while(1){
		for(i=0; i<size; i++){
			if(i == highlight){
				wattron(win, A_REVERSE);
			}
			wmove(win, i+1, 1);
			wprintw(win, menu[i]);
			wattroff(win, A_REVERSE);
		}

		direction = wgetch(win);

		switch(direction){
			case KEY_UP:
				highlight--;
				if(highlight < 0)
					highlight = size-1;
				break;
				
			case KEY_DOWN:
				highlight++;
				if(highlight > size-1)
					highlight = 0;
				break;

			default:
				break;
		}

		if(direction == 10) //엔터 치면 종료.
			break;
	}
	
	return highlight;
}

// 메뉴창 크기 변경
void winResize(WINDOW* win, int h, int w){
	box(win, ' ', ' ');
	wrefresh(win);
        wresize(win, h, w);
        box(win, 0, 0);
	wrefresh(win);
}

// 출력 label에 출력
void print(char* str){
	wrefresh(pLabelWin);
	wprintw(pLabelWin, str);
	wrefresh(pLabelWin);
}

// 파일 목록 불러오기
int getFiles(char* files[]){
	DIR* dir;
	struct dirent* ent;

	dir = opendir("./");
	if(dir == NULL){
		perror("opendir");
		exit(1);
	}
	
	int cnt=0;
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
		files[cnt] = ent->d_name;
		cnt++;
	}


	return cnt;
}

// 창 내용물 지우기
void wEraseWin(WINDOW* win, int y, int x)
{
	wborder(win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
	wrefresh(win);
	
	for(int i = 0; i <y; i++){
		for(int j = 0; j<x; j++){
			wmove(win, i, j);
			waddch(win, ' ');
		}
	}
	wrefresh(win);
}

void printStdout(char* file){
	FILE* fpin;

	fpin = fopen(file, "r");
	if(fpin == NULL){
		perror("fopen");
		exit(1);
	}

	char buf[BUFSIZ];

	print("--------------------------\n");
	while(fgets(buf, BUFSIZ, fpin) != NULL){
		print(buf);
		usleep(10000);
	}
	print("--------------------------\n");
	wprintw(pLabelWin, "Complete Print: %s\n", file);
	wrefresh(pLabelWin);

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

void printer(int signo){
	// 메세지 큐 꺼내오기
	// 큐 식별자에 따라 if문으로 나눠 다른 함수 실행하기
	
	// 메세지 큐에 내용이 없다면 함수 끝내기.
	struct msqid_ds tmpbuf;
	msgctl(msgid, IPC_STAT, &tmpbuf);
	if(tmpbuf.msg_qnum <= 0){
		printf("msg num: %ld\n", tmpbuf.msg_qnum);
		return;
	}	
	
	// 메세지큐 맨 앞의 내용 읽어오기
	Msgbuf rcvMsg;
	msgrcv(msgid, &rcvMsg, SIZE, 0, IPC_NOWAIT);
	//wprintw(pLabelWin, "received msg: %s, type: %ld\n", rcvMsg.mtext, rcvMsg.mtype);
	//wrefresh(pLabelWin);

	// 타입별로 출력
	switch(rcvMsg.mtype){
		case NEWFILE:
			printNewFile(rcvMsg.mtext);
			break;

		case STDOUT:
			printStdout(rcvMsg.mtext);
			break;
		case FAX:
			print("fax print\n");
			break;
	}
}


int main(void){

	initscr();
	cbreak();
	noecho();

	// 창 크기 구하기
	int yStd, xStd;
	getmaxyx(stdscr, yStd, xStd);

	// 마진 구하기
	int xMargin = 5;
	int yMargin = 4;

	// 제목 출력
	char* title = "Printer";
	attron(A_BOLD);
	mvprintw(1, (xStd-strlen(title))/2, "%s\n", title);
	attroff(A_BOLD);
	refresh();

	// 메뉴 창 만들기
	char* menu[] = {"Send Fax", "Print", "Print At New File", "Exit"};
	int numOfMenu = sizeof(menu)/sizeof(menu[0]);
	int menuH = numOfMenu + 2;
	int menuW = xStd/3;
	WINDOW* menuWin = newwin(menuH, menuW, yMargin, xMargin);
	box(menuWin, 0, 0);
	keypad(menuWin, true);
	wrefresh(menuWin);
	

	// 출력창 만들기
	int printH = yStd - 10;
	int printW = xStd/2;
	WINDOW* printWin = newwin(printH, printW, yMargin, xStd-xMargin-printW);
	box(printWin, 0, 0);
	wrefresh(printWin);

	// 출력 라벨 만들기
	int pLabelH = printH - 2;
	int pLabelW = printW - 3;
	pLabelWin = newwin(pLabelH, pLabelW, yMargin+1, xStd-xMargin-printW+2);
	scrollok(pLabelWin, TRUE); //스크롤 가능
	wrefresh(pLabelWin);
	
	// 시그널 등록
	signal(SIGUSR1, printer);

	// 메세지 큐 만들기
        key_t key = ftok(keyfile, keynum);

        msgid = msgget(key, IPC_CREAT|0664);
        if(msgid == -1){
                perror("msgget");
                exit(1);
        }



	int choice;
	while((choice = SelectMenu(menuWin, menu, numOfMenu)) != numOfMenu-1){ //메뉴 선택. 종료를 선택하지 않을 동안.

		// 파일 몰록
		char* files[50];
		int fileNum;
		Msgbuf msg;

		switch (choice) {
			case 0: // 팩스
				print("choice 1\n");
				break;
			case 1: // 출력
				print("choce 2\n");
			
				// 파일 목록 가져오기	
				fileNum = getFiles(files);
				// 파일 개수만큼 메뉴창 크기 변경
				wEraseWin(menuWin, menuH, menuW);
				winResize(menuWin, fileNum+2, menuW);
				// 출력할 파일 선택
				choice = SelectMenu(menuWin, files, fileNum);
				print("choide: ");
				print(files[choice]);
				print("\n");
				// 원래 메뉴창으로 크기 변경
				wEraseWin(menuWin, fileNum+2, menuW);
				winResize(menuWin, menuH, menuW);

                        	msg.mtype = STDOUT;
				strcpy(msg.mtext, files[choice]);
				msgsnd(msgid, (void*)&msg, SIZE, IPC_NOWAIT);
				kill(getpid(), SIGUSR1);
				break;
			case 2: // 파일에 출력

	                        // 파일 목록 가져오기
                                fileNum = getFiles(files);
                                // 파일 개수만큼 메뉴창 크기 변경
                                wEraseWin(menuWin, menuH, menuW);
                                winResize(menuWin, fileNum+2, menuW);
                                // 출력할 파일 선택
                                choice = SelectMenu(menuWin, files, fileNum);
                                print("choide: ");
                                print(files[choice]);
                                print("\n");
                                // 원래 메뉴창으로 크기 변경
                                wEraseWin(menuWin, fileNum+2, menuW);
                                winResize(menuWin, menuH, menuW);

				printNewFile(files[choice]);
				//winResize(menuWin, menuH+5, menuW);
				break;
			default:
				printw("deault");
				refresh();
				break;
		}
	}
	
	getch();

	endwin();

	return 0;
}
