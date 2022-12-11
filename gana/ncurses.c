#include <ncurses.h>
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
pid_t printerPid;

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
	wprintw(pLabelWin, str);
	wrefresh(pLabelWin);
}

void initPrinter(WINDOW* win){

	wprintw(win, "make printer\n");

        /* 자식 프세로 프린터 만들기 */
        switch(printerPid = fork()){
                case -1:
                        perror("fork");
                        exit(1);
                        break;
                case 0:
			if(win == NULL){
				printf("null");
			}
			else{
				wprintw(win, "not null");
			}
			exit(0);
                        break;
                default:
			print("make parent\n");
                        break;
        }
        /* 자식 프세로 프린터 만들기 끝 */

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
	char* menu[] = {"Send Fax", "Print", "Resize", "Exit"};
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
	

	// 출력 지점 설정
	
	int startingPoint = 13;
	move(startingPoint, 0);
	

	// 메세지 큐 만들기
        key_t key = ftok(keyfile, keynum);

        msgid = msgget(key, IPC_CREAT|0664);
        if(msgid == -1){
                perror("msgget");
                exit(1);
        }

	curs_set(0);
	initPrinter(pLabelWin);
	print("hello!\n");


	int choice;
	while((choice = SelectMenu(menuWin, menu, numOfMenu)) != numOfMenu-1){ //메뉴 선택. 종료를 선택하지 않을 동안.

		// 파일 몰록
		char* files[50];
		int fileNum;

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
				break;
			case 2:
				winResize(menuWin, menuH+5, menuW);
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
