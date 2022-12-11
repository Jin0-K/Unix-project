#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include "FuncTel.h"

#define SIZE 100 //배열에 저장될 수 있는 연락처 수.

//qsort()할 때 비교함수. 이름 사전순으로 비교함.
int CompareTelName(const void* a, const void* b)
{
        TEL* p = (TEL*)a;
        TEL* q = (TEL*)b;

        if (strcmp(p->name, q->name) < 0) //p가 앞쪽에 있음. p가 작음.
                return -1;
        else if (strcmp(p->name, q->name) > 0) //p가 뒤쪽에 있음. 더 큼.
                return 1;
        else //둘이 같음.
                return 0;
}

void PrintMenu(WINDOW* win, char* arr[], int size)
{
        int x, y;
        box(win, 0, 0);
        //wmove(winOp, 1, 0);
        for(int i = 0; i<size; i++){
                getyx(win, y, x);
                wmove(win ,y+1, 1);
                wprintw(win, "%d. %s", i+1, arr[i]);
        }
        wrefresh(win);
}

void EraseWin(int yStart, int xStart, int yMax, int xMax)
{
	int i, j;
	int y = yStart, x = xStart;

	move(yStart, xStart);
	for(i=0; i<=(yMax-yStart); i++){
		for(j=0; j<=(xMax-xStart); j++){
			addch(' ');
		}
		move(++y, x);
	}
}

void wEraseWin(WINDOW* win, int y, int x)
{
	wborder(win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
	wrefresh(win);
	
	for(int i = 0; i <y; i++){
		for(int j = 0; j<x; j++){
			//printw("d");
			wmove(win, i, j);
			waddch(win, ' ');
		}
	}
	wrefresh(win);
}

void ClearWin(int startingPoint, int xStd)
{
        int x, y;

        attron(A_BLINK | A_DIM);
        printw("\n");
        printw("Press anykey to do next fucntion.");
        attroff(A_BLINK | A_DIM);

        getch();
        getyx(stdscr, y, x);
        EraseWin(startingPoint, 0, y, xStd);
        refresh();
}

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


int main(void)
{
	int choice;
	int xStd, yStd;
	int xWin, yWin;
	int x, y;
	int height, width;
	char* str;
	char* menu[] = {"search", "Add", "Delete", "Change", "List", "Exit"};
	char* updateMenu[] = {"Name", "Phone", "Memo", "Save"};
        int size = sizeof(menu)/sizeof(menu[0]);
	WINDOW* winOp;
	TEL telArr[SIZE];	
        int numOfTel = 0; //저장된 연락처 개수.
	
	////////////////////////////////////////
	initscr();
	cbreak();
	noecho();
	
	getmaxyx(stdscr, yStd, xStd); //창 크기 구하기.

	//제목 출력.
	str = "Telephon Dictionary";
	attron(A_BOLD);
	mvprintw(1, (xStd-strlen(str))/2, "%s\n", str);
	attroff(A_BOLD);
	refresh();
	
	//메뉴창 만들기.
	height = size + 2;
    width = 30;
    winOp = newwin(height, width, 4, (xStd-width)/2);
	keypad(winOp, true);
        refresh();
	
	//출력이 시작되는 지점.
	int startingPoint = 13;
	move(startingPoint, 0);

        //파일 읽기 확인요 코드.
        /*OpenFileReadAndCreatStr(telArr, &numOfTel, SIZE);
        PrintTel(&telArr[0]);
        printw("%d\n", numOfTel);
        for (int i = 0; i < numOfTel; i++) {
                printw("%d", i);
                PrintTel(&telArr[i]);
        }*/

	while((choice = SelectMenu(winOp, menu, size)) != size-1){ //메뉴 선택. 종료를 선택하지 않을 동안.
		int n; //함수 동작 확인용.
		TEL tempTel;
		char tempStr[50];
		
		switch (choice) {
		case 0: //검색.
			echo();
			printw("Enter word for search phone: "); //찾을 단어 입력.
			getstr(tempStr);
			noecho();

			int n = OpenFileReadAndCreatStr(telArr, &numOfTel, SIZE); //파일 열어 데이터 가져와 배열에 저장..
                	if (n) {
				printw("\n");
                        	n = SearchTelAndPrint(telArr, numOfTel, tempStr, NULL); //배열에서 연락처 찾기.
                        	if (n == 0) {
                               		printw("no match found.\n"); //못찾음.
                        	}
                	}
                	else {
                        	printw("OpenFileReadAndCreatStr() error\n");
                	}
			refresh();

			ClearWin(startingPoint, xStd);

			break;

                case 1: //연락처 추가. append.
			echo();
			printw("Enter the 3 or 2 information of phone\n");
			printw("name: ");
			getstr(tempTel.name);
			printw("phone number: ");
			getstr(tempTel.phone);
			printw("memo: ");
			getstr(tempTel.memo);
			noecho();
			
                        //입력 확인용 출력.
			printw("\n");
			attron(A_REVERSE);
			PrintTel(&tempTel);
			attroff(A_REVERSE);
			
			//저장.
			printw("add? [Y/N] "); //정말 추가할건지 물음.
                        char check = getch();
                        if (check == 'Y' || check == 'y') { //연락처 저장.
                                FILE* fApd = fopen(TELDATA, "a"); //파일 append모드로 열음.
                                if (fApd != NULL) {
                                        n = TelAddAtFile(fApd, &tempTel); //연락처 파일에 추가.
                                        fclose(fApd);
                                }
                                if (n) //파일에 연락처 잘 추가 됐으면,
                                        printw("saved.\n");
                                else //파일 추가 때 오류 발생했으면,
                                        printw("TelAdd() error.\n");
                        }
                        else { //저장 취소.
                                printw("cancel save\n");
                        }

			ClearWin(startingPoint, xStd); 
			break;
		
		case 2: //연락처 삭제. delete.
			echo();
                        printw("Enter word to delete tel: ");
			getstr(tempStr);
			printw("\n");
			noecho();
                        
			n = OpenFileReadAndCreatStr(telArr, &numOfTel, SIZE); //파일 열어 연락처데이터 배열에 저장.
                        if (n) {
                                int indArr[20]; //<단어>있는 연락처 찾아서 인덱스 저장. 일단 <단어>가 포함된 연락처가 20개를 넘지 않>을>거라 가정.
                                int n = SearchTelAndPrint(telArr, numOfTel, tempStr, indArr); //연락처 찾기.
                                if (n > 0) { //찾은 연락처가 있다면,
					echo();
                                        int line;
					printw("\n");
                                        printw("which one? ");
                                        scanw("%d", &line);
					noecho();

                                        if(0 < line && line <= n){ //삭제.
                                                OpenFileForDelet(indArr[line - 1], numOfTel, telArr); //삭제.
						printw("completion delete\n");
					}
					else
                                                printw("input was out of range.\n");
                                }
                                else{
                                        printw("can't found\n");
                                }
                        }
			
			ClearWin(startingPoint, xStd);

			break;

		case 3: //수정.
			echo();
                        printw("Enter word to update tel: "); //수정할 연락처 찾기.
                        getstr(tempStr);
                        printw("\n");
                        noecho();

                        n = OpenFileReadAndCreatStr(telArr, &numOfTel, SIZE); //파일 열어 데이터 가져오기.
                        if (n) {
                                int indArr[10]; //<단어>있는 연락처 찾아서 인덱스 저장. 일단 <단어>가 포함된 연락처가 10개를 넘지 않>을>거라 가정.
                                int n = SearchTelAndPrint(telArr, numOfTel, tempStr, indArr); //연락처 찾기.
                                if (n > 0) {
                                        echo();
                                        int line;
                                        printw("\n");
                                        printw("which one? ");
                                        scanw("%d", &line);
                                        noecho();

                                        if(0 < line && line <= n){
						printw("\n");
						PrintTel(&telArr[indArr[line-1]]);
						tempTel = telArr[indArr[line-1]];
						
						//연락처 수정하기.
						wEraseWin(winOp, height, width); //기존 메뉴창 삭제. 새로운 메뉴창 띄우기 위함.
						while((choice = SelectMenu(winOp, updateMenu, 4)) != 4-1){
							printw("Enter word for change: ");
							echo();
							if(choice == 0)
								getstr(tempTel.name);
							else if(choice == 1)
								getstr(tempTel.phone);
							else if(choice == 2)
								getstr(tempTel.memo);
							noecho();
						}

                                                OpenFileForDelet(indArr[line - 1], numOfTel, telArr); //삭제.
						FILE* fApd = fopen(TELDATA, "a"); //파일 append모드로 열음.
	        	                        if (fApd != NULL) {
        	        	                        n = TelAddAtFile(fApd, &tempTel); //수정된 연락처 파일에 추가.
                                		        fclose(fApd);
                              			}
						
                        			//입력 확인용 출력.
						printw("\n");
						attron(A_REVERSE);
						PrintTel(&tempTel);
						attroff(A_REVERSE);
                                                printw("completion update\n");
                                        }
                                        else
                                                printw("input was out of range.\n");
                                }
                                else{
                                        printw("can't found\n");
                                }
                        }

                        ClearWin(startingPoint, xStd);
			wEraseWin(winOp, height, width); //여기서 쓴 메뉴창 삭제.

			break;
                
		case 4: //이름 사전순으로 전화번호부 출력.
                        n = OpenFileReadAndCreatStr(telArr, &numOfTel, SIZE);
                        if(n){
                                qsort(telArr, numOfTel, sizeof(telArr[0]), CompareTelName); //이름 사전순으로 정렬.
                                for (int i = 0; i < numOfTel; i++) { //전화번호부 출력.
                                        PrintTelLineNum(&telArr[i], i + 1);
                                }
                        }
			
			ClearWin(startingPoint, xStd);
                        
			break;

                default:
                        printw("error in option.\n");
			
			ClearWin(startingPoint, xStd);
                        
			break;
                }
		
		numOfTel = 0; //새로 데이터 파일 받아오기 위해 연락처 개수 초기화.
		move(startingPoint, 0);
	}
	refresh();

	delwin(winOp);	
	endwin();
	/////////////////////////////////////////
}
