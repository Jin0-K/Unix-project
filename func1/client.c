#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ncurses.h>
#include "msgtype.h"
	
#define KEY_NUM 1
#define SHARED_MEMORY_SIZE 1024*1024
#define MAX_FILE 20
#define STR_LEN 256


void create_msgq(int *qid); 
void remove_msgq(int *qid); 
void print_txt_files(WINDOW *window, DIR *dp, struct dirent **list, int size); 
int get_txt_files(DIR *dp, struct dirent **list); 
char *getLine(WINDOW *window, char *buf, int size); 
int get_int(WINDOW *window); 
int is_in_range(int num, int least, int most); 
void end_program(WINDOW *window, int *qid, struct msgbuf *message); 

int main() {
	int qid[2];
	struct msgbuf message;
	// create a key for IPC
        key_t key = ftok(".", (int)getpid());

        // create a ncurses window
        WINDOW *win;
        initscr();
        noecho();
        cbreak();
        // Get the size of the current window
        int yMax, xMax;
        getmaxyx(stdscr, yMax, xMax);
        // start a window
        win = newwin(yMax, xMax, 1, 1);
        wrefresh(win);

	create_msgq(qid);

	// register in server
	message.mtype = 1;
	message.msgcontent = (struct content) { .type = REGISTER, 
						.pid = getpid(),
						.qid = qid[0] };
	if (msgsnd(qid[1], (void *)&message, sizeof(struct msgbuf), 0) == -1) {
		perror("msgsnd");
		exit(1);
	}



	// User chooses to do Send Fax

	

        DIR *dp;
        struct dirent *files[MAX_FILE]; // text file list
	int find, fnum;

        // print txt files of current directory
        dp = opendir(".");
	fnum = get_txt_files(dp, files);
	do {
        	print_txt_files(win, dp, files, fnum);
		find = get_int(win);
	} while(!is_in_range(find, 0, fnum)); 
	wclear(win);

	// exit if user canceled
	if (!find) {
		wprintw(win, "Canceled");
		wgetch(win);
		endwin();
		exit(1);
	}

	// send message to server that I will send fax
	message.msgcontent.type = SEND_FAX;
	if (msgsnd(qid[1], (void *)&message, sizeof(struct msgbuf), 0) == -1) {
		perror("msgsnd");
		exit(1);
	}
	



        wgetch(win);
        closedir(dp);

	end_program(win, qid, &message); 

        return 0;
}


// create message queues
void create_msgq(int *qid) {
	// message queue to receive message
	int keynum = (int)getpid() % 253 + 2;
	while ((qid[0] = msgget(ftok(".", keynum), IPC_CREAT|IPC_EXCL|0640)) < 0) {
		if (++keynum > 255) {
			keynum = 2;
		}
	}
	// message queue to send message
	if ((qid[1] = msgget(ftok(".", KEY_NUM), 0)) < 0) {
		perror("msgget");
		exit(1);
	}

}

void remove_msgq(int *qid) {
	msgctl(qid[0], IPC_RMID, (struct msqid_ds *)NULL);
}

// print text files in inode list
// return the number of files
void print_txt_files(WINDOW *window, DIR *dp, struct dirent **list, int size) {
	wclear(window);

	for (int i = 0; i < size; i++) {
		wprintw(window, "%d ", i+1);
                wprintw(window, "%s\n", list[i]->d_name);
        }
	wprintw(window, "Which file do you want to send?(0 is for cancel)\n");
        //wrefresh(window);
}

// put text files of dp in the inode list
// return the number of files
int get_txt_files(DIR *dp, struct dirent **list) {
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

// get an string input from user
char *getLine(WINDOW *window, char *buf, int size) {
	echo();
	wgetnstr(window, buf, size-1);
	noecho();
	return buf;
}

// get the input from user
int get_int(WINDOW *window) {
	char buf[8];
	return atoi(getLine(window, buf, 8));
}

// check if the integer is within range
int is_in_range(int num, int least, int most) {
	if (num < least) {
		return 0;
	}
	if (num > most) {
		return 0;
	}
	return 1;
}
	
// end program
void end_program(WINDOW *window, int *qid, struct msgbuf *message) {
        // remove message queues
        remove_msgq(qid);
        // send message to server to unregister
        message->mtype = 1;
        message->msgcontent.type = UNREGISTER;
        if (msgsnd(qid[1], (void *)message, sizeof(struct msgbuf), 0) == -1) {
                perror("msgsnd");
                exit(1);
        }

        // close window
        endwin();
}
