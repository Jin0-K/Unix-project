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
void printFax(int shmid);
void printStdout(char* file);
void printNewFile(char* file);
void printer(int signo);

// Functions for fax
void sig_handler_getfax(int signo);
void create_msgq(); 
void remove_msgq(); 
/* void register_server(struct message *msg); */
int choose_file(WINDOW *pWin, WINDOW *mWin, int y, int x, struct dirent *file);
void request_pids(struct message *msg);
int receive_pids(struct message *message, pid_t *pids);
int choosePid(WINDOW *window, int win_y, int win_x, pid_t *options, int optlen); 
int put_file_shm(struct dirent file, int *shmid, char *shmaddr);
int send_fax(struct message *msg, pid_t receiver, int shmid);
void unregister_server();

WINDOW* pLabelWin; // 출력 라벨 윈도우
int msgid; // Message queue for printer
int cli_keynum;
int qid[2]; // Message queue for fax



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
	signal(SIGUSR2, sig_handler_getfax);
	/*
	struct sigaction act_usr2;
	sigemptyset(&act_usr2.sa_mask);
	act_usr2.sa_flags = SA_SIGINFO || SA_ONSTACK;
	act_usr2.sa_sigaction = (void (*)(int, siginfo_t *, void *))sig_handler_getfax;
	if (sigaction(SIGUSR2, &act_usr2, (struct sigaction *)NULL) < 0) {
		endwin();
		perror("sigaction");
		exit(1);
	}
	*/

	// Create message queue for printer
        key_t key = ftok(keyfile, keynum);

        msgid = msgget(key, IPC_CREAT|0664);
        if(msgid == -1){
                perror("msgget");
                exit(1);
        }
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

	int choice;
	while((choice = SelectMenu(menuWin, menu, numOfMenu)) != numOfMenu-1){ 
	//메뉴 선택. 종료를 선택하지 않을 동안.

		// File list
		char* files[MAX_FILE];
		int fileNum;
		Msgbuf msg;

		switch (choice) {
			case 0: // 팩스
				print(">> SEND FAX\n");
				
				// Let the user choose which file to send
				struct dirent file;
				if (choose_file(printWin, menuWin, menuH, menuW, &file) == -1) {
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
				if (put_file_shm(file, &shmid, addr) == -1) {
					break;
				}
				// send fax
				if (send_fax(&msgf, pid_list[rcvr_ind], shmid) == -1) {
					print("Unable to send message\n");
				}
				print("File has sent\n");
				
				break;
			case 1: // 출력
				print(">> PRINT\n");
			
				// 파일 목록 가져오기	
				fileNum = getFiles(files);
				// 파일 개수만큼 메뉴창 크기 변경
				wEraseWin(menuWin, menuH, menuW);
				winResize(menuWin, fileNum+2, menuW);
				// 출력할 파일 선택
				choice = SelectMenu(menuWin, files, fileNum);
				print("choice: ");
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
				print(">> PRINT AT NEW FILE\n");

	                        // 파일 목록 가져오기
                                fileNum = getFiles(files);
                                // 파일 개수만큼 메뉴창 크기 변경
                                wEraseWin(menuWin, menuH, menuW);
                                winResize(menuWin, fileNum+2, menuW);
                                // 출력할 파일 선택
                                choice = SelectMenu(menuWin, files, fileNum);
                                print("choice: ");
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
	

	endwin();
	unregister_server();

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

void printFax(int shmid) {
	char *shmaddr = shmat(shmid, NULL, SHM_RDONLY);

	print("--------------------------\n");
		print(shmaddr);
		usleep(10000);
	print("--------------------------\n");
	wprintw(pLabelWin, "Fax Reception Complete\n");
	wrefresh(pLabelWin);
	
	// dettach and remove shared memory
	shmdt(shmaddr);
	shmctl(shmid, IPC_RMID, (struct shmid_ds *)NULL);

	return;
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
			print("Fax Received!\n");
			printFax(atoi(rcvMsg.mtext));
			break;
	}
	print("Printer over\n");
}


// operation when received GET_FAX
void sig_handler_getfax(int signo) {
	void send_msg_printer(struct message *message);
	struct message msg;
	
	// Receive message
	msgrcv(qid[0], &msg, sizeof(struct message), 1, 0);
	send_msg_printer(&msg);
	raise(SIGUSR1);
	print("Signal Handler Done\n");
}

// Send the received message to printer
void send_msg_printer(struct message *message) {
	Msgbuf msg_send;
	
	msg_send.mtype = FAX;
	sprintf(msg_send.mtext, "%d", message->content.qid);
	
	// send the message to printer message queue
	if (msgsnd(msgid, (void *)&msg_send, SIZE, IPC_NOWAIT) == -1) {
		perror("msgsnd");
	}
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

/* Server cannot receive message with this function (don't know why)
// register in server
void register_server(struct message *msg) {
	msg->mtype = 1; // mtype will always be 1
	msg->content = (struct cont) { 
				.type = REGISTER, 
				.pid = getpid(),
				.qid = qid[0] };
	if (msgsnd(qid[1], (void *)&msg, sizeof(struct message), 0) == -1) {
		print("Failed to connect server\n");
		return;
	}
	
	int msg_len = msgrcv(qid[0], &msg, sizeof(struct message), 1, 0);
	if (msg_len < 0) {
		endwin();
		perror("msgrcv");
		exit(1);
	}
	if (msg->content.type == UNREGISTER) {
		print("Unable to register\n");
		// remove message queue
		remove_msgq();
		endwin();
		exit(1);
	}
	print("Regesteration Successful\n");
}
*/

// let the user choose which file to send
// return the struct dirent of the file
int choose_file(WINDOW *pWin, WINDOW *mWin, int y, int x, struct dirent *file) {
	int get_files(DIR *dp, struct dirent **list);
	int chooseFile(WINDOW *window, struct dirent **options, int optlen);
	DIR *dp;
	struct dirent *files[MAX_FILE]; // text file list
	int file_num, file_ind;
	
	// open directory
	dp = opendir(".");
	// get the number of files
	file_num = get_files(dp, files);
	closedir(dp);
	// set the size of menu window as the number of files
	wEraseWin(mWin, y, x);
	winResize(mWin, file_num + 3, x);
	// select file
	file_ind = chooseFile(mWin, files, file_num);
	
	// exit if user canceled
	if (file_ind == file_num) {
		print("Canceled to choose\n");
		return -1;
	}
	
	*file = *(files[file_ind]);
	print("You've chosen ");
	print(file->d_name);
	print("\n");
	
	// reset the menu size	
	wEraseWin(mWin, file_num+3, x);
	winResize(mWin, y, x);
	
	
	return 0;
}

// put text files of dp in the inode list
// return the number of files
int get_files(DIR *dp, struct dirent **list) {
	int ends_txt(char *str, int size); 
	struct dirent *dent;
        int size;
        int file_num = 0;

        while ((dent = readdir(dp))) {
                size = strlen(dent->d_name);
                if (ends_txt(dent->d_name, size)) {
                	*(list+file_num) = dent;
                       file_num++;
                }
        }

        return file_num;
}

// check if the string ends with ".txt"
int ends_txt(char *str, int size) {
        // check if the last 4 characters of str are ".txt"
        if (strcmp((str+size-4), ".txt") == 0) {
                return 1;
        }
        return 0;
}

// Get an option input from user
int chooseFile(WINDOW *window, struct dirent **options, int optlen) {
	int choice;
	int file_num = optlen + 1;
	int highlight = 0;

	box(window, 0, 0);

	while (1) {
		int i;
		for (i = 0; i < optlen; i++) {
			if (i == highlight) {
				wattron(window, A_REVERSE);
			}
			mvwprintw(window, i+2, 1, "%s", options[i]->d_name);
			wattroff(window, A_REVERSE);
		}
		if (i == highlight) {
			wattron(window, A_REVERSE);
		}
		mvwprintw(window, i+2, 1, "Cancel");
		wattroff(window, A_REVERSE);
		choice = wgetch(window);

		switch (choice) {
			case KEY_UP :
				highlight--;
				// When the highlight get out of range
				if (highlight < 0) {
					highlight = 0;
				}
				break;

			case KEY_DOWN :
				highlight++;
				// When the highlight get out of range
				if (highlight > file_num) {
					highlight = file_num;
				}
				break;

			default :
				break;
		}
		if (choice == 10) {
			break;
		}
	}

	return highlight;
}	

// send message to server to give pid list
void request_pids(struct message *msg) {
	msg->content.type = GET_PIDS;
	msg->content.qid = getpid();
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


int choosePid(WINDOW *window, int win_y, int win_x, pid_t *options, int optlen) {
	int choice;
	int highlight = 0;

	// Set the window size
	wEraseWin(window, win_y, win_x);
	winResize(window, optlen+2, win_x);

	while (1) {
		for (int i = 0; i < optlen; i++) {
			if (i == highlight) {
				wattron(window, A_REVERSE);
			}
			mvwprintw(window, i+2, 1, "%d", (int)options[i]);
			wattroff(window, A_REVERSE);
		}
		choice = wgetch(window);

		switch (choice) {
			case KEY_UP :
				highlight--;
				// When the highlight get out of range
				if (highlight < 0) {
					highlight = 0;
				}
				break;

			case KEY_DOWN :
				highlight++;
				// When the highlight get out of range
				if (highlight >= optlen) {
					highlight = optlen - 1;
				}
				break;

			default :
				break;
		}
		if (choice == 10) {
			break;
		}
	}
	
	// Reset the window size
	wEraseWin(window, optlen+2, win_x);
	winResize(window, win_y, win_x);

	return highlight;
}

int put_file_shm(struct dirent file, int *shmid, char *shmaddr) {
	int fd;
	struct stat statbuf;
	
	// Open file to share
	if (stat(file.d_name, &statbuf) == -1) {
		print("Error: stat failed\n");
		return -1;
	}
	if ((fd = open(file.d_name, O_RDWR)) == -1) {
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
