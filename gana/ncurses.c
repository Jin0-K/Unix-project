#include <ncurses.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
	
WINDOW* pLabelWin;

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

void winResize(WINDOW* win, int h, int w){
	box(win, ' ', ' ');
        wresize(win, h, w);
        box(win, 0, 0);
}

void print(char* str){
	wprintw(pLabelWin, str);
	wrefresh(pLabelWin);
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
	
	
	int choice;
	while((choice = SelectMenu(menuWin, menu, numOfMenu)) != numOfMenu-1){ //메뉴 선택. 종료를 선택하지 않을 동안.

		switch (choice) {
			case 0: // 팩스
				print("choice 1\n");
				break;
			case 1: // 출력
				print("choce 2\n");
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
