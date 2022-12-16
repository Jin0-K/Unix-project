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
#include <ncurses.h>
#include "msgtype.h"
#include "data.h"

#define MAX_FILE 15

void print(char* str);
int SelectMenu(WINDOW* win, char* menu[], int size);
void winResize(WINDOW* win, int h, int w);
int getFiles(char* files[]);
void wEraseWin(WINDOW* win, int y, int x);

// Functions for fax
void send_msg_printer(int signo);
void create_msgq(); 
void remove_msgq(); 

int chooseFile(WINDOW *window, char **options, int optlen);
void request_pids(struct message *msg);
int receive_pids(struct message *message, pid_t *pids);
int choosePid(WINDOW *window, int win_y, int win_x, pid_t *options, int optlen); 
int put_file_shm(char *file, int *shmid, char *shmaddr);
int send_fax(struct message *msg, pid_t receiver, int shmid);
void unregister_server();

WINDOW* pLabelWin; // 출력 라벨 윈도우
int msgid; // 메시지큐 식별자 for printer
int shmidPrinter; // 공유메모리 식별자 for printer
pid_t printerPid;
int cli_keynum;
int qid[2]; // Message queue for fax

// 프린터에서 출력해달라는 문자열 화면에 출력
void printerHandler(int signo){
	// 공유메모리 연결
	char* shmaddr = (char*)shmat(shmidPrinter, (char*)NULL, 0);
	// 공유메모리의 내용 출력
	print(shmaddr);
	// 공유메모리 연결 해제
	shmdt((char*)shmaddr);
}

// 프린터 켜기
void printerOn(void){
	print("printer on\n");
	
	// 메세지큐 생성
        key_t keyM = ftok(keyfile, keynum);

        msgid = msgget(keyM, IPC_CREAT|0664);
        if(msgid == -1){
                perror("msgget");
                exit(1);
        }
	
	// 공유메모리 생성
	key_t keyS = ftok(shmfile, keynum);
	shmidPrinter = shmget(keyS, keynum, IPC_CREAT|0644);
	if(shmidPrinter == -1){
		perror("shmget");
		exit(1);
	}
	
	char shmidbuf[5];
	// 자식프로세스로 printer 생성
	switch(printerPid = fork()){
		case -1:
			perror("fork");
			exit(1);
			break;
		case 0:
			sprintf(shmidbuf, "%d", shmidPrinter);
			execl("printer", "printer", shmidbuf, (char*)NULL);
			exit(0);
			break;
	}
	
}

// 프린터 끄기
void printerOff(void){
	print("printer off\n");
	
	msgctl(msgid, IPC_RMID, (struct msqid_ds*) NULL); //메세지큐 삭제
	shmctl(shmidPrinter, IPC_RMID, (struct shmid_ds*)NULL); //공유메모리 삭제
	// 자식프세 종료시키기
	kill(SIGKILL, printerPid);
}
	

int main(void) {

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
	signal(SIGUSR2, send_msg_printer);
	signal(SIGPOLL, printerHandler);
	
	printerOn();


        // Create message queue for fax
        create_msgq();
        
	struct message msgf;
	// Initialize msgf
	msgf.mtype = 1;
	msgf.content = (struct cont) { 
				.type = REGISTER, 
				.pid = getpid(),
				.qid = qid[0] };
	// register in server
	if (msgsnd(qid[1], (void *)&msgf, sizeof(struct message), 0) == -1) {
		endwin();
		perror("msgsnd");
		exit(1);
	}
	
	if (msgrcv(qid[0], &msgf, sizeof(struct message), 1, 0) < 0) {
		endwin();
		perror("msgrcv");
		exit(1);
	}
	if (msgf.content.type == UNREGISTER) {
		// remove message queue
		remove_msgq();
		endwin();
		printf("Unable to register\n");
		exit(1);
	}

	print("Connected to server\n");
	
	
	// 메뉴 선택 반복
	int choice;
	while((choice = SelectMenu(menuWin, menu, numOfMenu)) != numOfMenu-1){ 
	//메뉴 선택. 종료를 선택하지 않을 동안.

		// File list
		char* files[MAX_FILE];
		int fileNum;
		Msgbuf msg; // 메세지큐 버퍼
		
		print("select choice\n");

		switch (choice) {
			case 0: // 팩스
				print(">> SEND FAX\n");
				
				// read files from the current directory
				fileNum = getFiles(files);
				// resize the menu box
				wEraseWin(menuWin, menuH, menuW);
				winResize(menuWin, fileNum + 4, menuW);
				// Let the user choose which file to send
				choice = chooseFile(menuWin, files, fileNum);
				// reset menu box to original size
				wEraseWin(menuWin, fileNum+3, menuW);
				winResize(menuWin, menuH, menuW);
				
				if (choice == fileNum) { // if user cooses to cancel
					print("Canceled to choose\n");
					break;
				}
				
				// Get pid list from server
				pid_t pid_list[MAX_CLIENT - 1]; // pid list to print
				int list_n = 0;
				int rcvr_ind;
				request_pids(&msgf);
				list_n = receive_pids(&msgf, pid_list);
				if (!list_n) {
					print("There is no other client to send fax\n");
					break;
				}
				// Let the user choose where to send
				print("Choose where to send\n");
				rcvr_ind = choosePid(menuWin, menuH, menuW, pid_list, list_n);
				// Create shared memory to put file
				int shmid;
				char *addr;
				if (put_file_shm(files[choice], &shmid, addr) == -1) {
					break;
				}
				// send fax
				if (send_fax(&msgf, pid_list[rcvr_ind], shmid) == -1) {
					print("Unable to send message\n");
				}
				print("Fax Sent!\n");
				
				break;
			case 1: // 출력
				print(">> PRINT\n");
			
				// 파일 목록 가져오기	
				fileNum = getFiles(files);
				// 파일 개수만큼 메뉴창 크기 변경
				wEraseWin(menuWin, menuH, menuW);
				winResize(menuWin, fileNum+3, menuW);
				// 출력할 파일 선택
				choice = chooseFile(menuWin, files, fileNum);
				// 원래 메뉴창으로 크기 변경
				wEraseWin(menuWin, fileNum+3, menuW);
				winResize(menuWin, menuH, menuW);
				
				if (choice == fileNum) { // if user cooses to cancel
					print("Canceled to choose\n");
					break;
				}
				
				print("choice: ");
				print(files[choice]);
				print("\n");

				// 출력할 파일 메세지큐에 넣기
                        	msg.mtype = STDOUT;
				strcpy(msg.mtext, files[choice]);
				msgsnd(msgid, (void*)&msg, SIZE, IPC_NOWAIT);
				// 프린터로 시그널 보내서 출력하게하기
				kill(printerPid, SIGUSR1);
				
				break;
			case 2: // 파일에 출력
				print(">> PRINT AT NEW FILE\n");

	                       // 파일 목록 가져오기
                               fileNum = getFiles(files);
                               // 파일 개수만큼 메뉴창 크기 변경
                               wEraseWin(menuWin, menuH, menuW);
                               winResize(menuWin, fileNum+3, menuW);
                               // 출력할 파일 선택
                               choice = chooseFile(menuWin, files, fileNum);
                               // 원래 메뉴창으로 크기 변경
                               wEraseWin(menuWin, fileNum+3, menuW);
                               winResize(menuWin, menuH, menuW);
                                
				if (choice == fileNum) { // if user cooses to cancel
					print("Canceled to choose\n");
					break;
				}
                               print("choice: ");
                               print(files[choice]);
                               print("\n");
                               
                               // 복사할 파일 메세지큐에 넣기
                               msg.mtype = NEWFILE;
				strcpy(msg.mtext, files[choice]);
				msgsnd(msgid, (void*)&msg, SIZE, IPC_NOWAIT);
				// 프린터로 시그널보내서 출력하게하기
				kill(printerPid, SIGUSR1);
				break;
			default:
				printw("deault");
				refresh();
				break;
		}
	}
	

	printerOff();
	unregister_server();
	endwin();
	
	return 0;
}

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


// Send the received message to printer
void send_msg_printer(int signo) {
	struct message msg_rcv;
	Msgbuf msg_snd;
	
	// Receive message from server
	msgrcv(qid[0], &msg_rcv, sizeof(struct message), 1, 0);
	msg_snd.mtype = FAX;
	sprintf(msg_snd.mtext, "%d", msg_rcv.content.qid);
	
	// send the message to printer message queue
	if (msgsnd(msgid, (void *)&msg_snd, SIZE, IPC_NOWAIT) == -1) {
		perror("msgsnd");
	}
	
	print("fax recieved\n");
	kill(printerPid, SIGUSR1);
	//raise(SIGUSR1);
}

// create message queues
void create_msgq() {
	// message queue to receive message
	cli_keynum = (int)getpid() % 253 + 2;
	// don't let client key num value same as printer key num
	if (cli_keynum == keynum) {
		cli_keynum++;
	}
	// reset key_num if it already exists
	while ((qid[0] = msgget(ftok(".", cli_keynum), IPC_CREAT|IPC_EXCL|0640)) < 0) {
		if (++cli_keynum > 255) {
			cli_keynum = 2;
		}
	}

	// message queue to send message
	if ((qid[1] = msgget(ftok(".", KEY_NUM), 0)) < 0) {
		perror("msgget");
		exit(1);
	}

}

void remove_msgq() {
	if (msgctl(qid[0], IPC_RMID, (struct msqid_ds *)NULL) == -1) {
		printf("Error: Failed to delete message queue\n");
	}
}


// Get an option input from user
int chooseFile(WINDOW *window, char **options, int optlen) {
	
	int highlight = 0;
	int i;
	int direction;

	box(window, 0, 0);

	while (1) {
		for (i = 0; i < optlen; i++) {
			if (i == highlight) {
				wattron(window, A_REVERSE);
			}
			wmove(window, i+1, 1);
			wprintw(window, options[i]);
			wattroff(window, A_REVERSE);
		}
		if (i == highlight) {
			wattron(window, A_REVERSE);
		}
		wmove(window, i+1, 1);
		wprintw(window, "CANCEL");
		wattroff(window, A_REVERSE);
		
		direction = wgetch(window);

		switch(direction){
			case KEY_UP:
				highlight--;
				if(highlight < 0)
					highlight = optlen;
				break;
				
			case KEY_DOWN:
				highlight++;
				if(highlight > optlen)
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

// send message to server to give pid list
void request_pids(struct message *msg) {
	msg->content.type = GET_PIDS;
	msg->content.pid = getpid();
	msg->content.qid = qid[0];
	if (msgsnd(qid[1], (void *)msg, sizeof(struct message), 0) == -1) {
		perror("msgsnd");
		exit(1);
	}
}

// Receive pid list from server
int receive_pids(struct message *message, pid_t *pids) {
	int msg_len;
	int num = 0;
	while(1) {
		msg_len = msgrcv(qid[0], message, sizeof(struct message), 1, 0);
		if (msg_len == -1) {
			return -1;
		}
		if (message->content.type == GET_PIDS) {
			if (message->content.pid == 0) {
				break;
			}
			pids[num++] = message->content.pid;
		}
	}
	return num;
}


int choosePid(WINDOW* window, int win_y, int win_x, pid_t* options, int optlen) {
	int direction;
	int highlight = 0;
	int i;

	// Set the window size
	wEraseWin(window, win_y, win_x);
	winResize(window, optlen+2, win_x);

	while (1) {
		for(i=0; i<optlen; i++){
			if(i == highlight){
				wattron(window, A_REVERSE);
			}
			wmove(window, i+1, 1);
			wprintw(window, "%d", (int)options[i]);
			wattroff(window, A_REVERSE);
		}
		
		direction = wgetch(window);

		switch(direction){
			case KEY_UP:
				highlight--;
				if(highlight < 0)
					highlight = optlen-1;
				break;
				
			case KEY_DOWN:
				highlight++;
				if(highlight > optlen-1)
					highlight = 0;
				break;

			default:
				break;
		}

		if(direction == 10) //엔터 치면 종료.
			break;
	}
	
	// Reset the window size
	wEraseWin(window, optlen+2, win_x);
	winResize(window, win_y, win_x);

	return highlight;
}

int put_file_shm(char *file, int *shmid, char *shmaddr) {
	int fd;
	struct stat statbuf;
	
	// Open file to share
	if (stat(file, &statbuf) == -1) {
		print("Error: stat failed\n");
		return -1;
	}
	if ((fd = open(file, O_RDWR)) == -1) {
		print("Error: unable to open file\n");
		return -1;
	}
	
	// assign memory
	*shmid = shmget(ftok(".", cli_keynum), statbuf.st_size, IPC_CREAT|0666);
	if (*shmid == -1) {
		print("Error: Unable to create shared memory\n");
		return -1;
	}
	shmaddr = shmat(*shmid, NULL, 0);
	
	// read file to put it in shared memory
	if (read(fd, shmaddr, statbuf.st_size) == -1) {
		print("Error: unable to read file\n");
		return -1;
	}
	
	close(fd);
	
	return 0;
}

// send message to server with shmid
int send_fax(struct message *msg, pid_t receiver, int shmid) {
	// set message
	msg->content.type = SEND_FAX;
	msg->content.pid = receiver;
	msg->content.qid = shmid;

	if (msgsnd(qid[1], (void *)msg, sizeof(struct message), 0) == -1) {
		return -1;
	}
	return 0;
}

// end program
void unregister_server() {
        // remove message queue
        remove_msgq();
        // send message to server to unregister
        struct message msg;
        msg.mtype = 1;
        msg.content.type = UNREGISTER;
	msg.content.pid = getpid();
	msg.content.qid = qid[0];
        if (msgsnd(qid[1], (void *)&msg, sizeof(struct message), 0) == -1) {
        	endwin();
               perror("msgsnd");
               exit(1);
        }
}
