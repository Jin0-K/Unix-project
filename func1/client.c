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
//#include <ncurses.h>
#include "msgtype.h"
	
#define KEY_NUM 1
#define SHARED_MEMORY_SIZE 1024*1024
#define MAX_FILE 20
#define STR_LEN 256


void create_msgq(int *qid); 
void remove_msgq(int *qid); 
/*
void put_txt_files(WINDOW *window, DIR *dp, struct dirent **list); 
char *getLine(WINDOW *window, char *buf, int size); 
int get_int(WINDOW *window); 
*/

int main() {
	int qid[2];
	struct msgbuf message;
	// create a key for IPC
        key_t key = ftok(".", (int)getpid());

        // create a ncurses window
	/*
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
	*/

	create_msgq(qid);

	// register in server
	message.mtype = 1;
	message.msgcontent = (struct content) { .type = REGISTER, .pid = getpid() };
	if (msgsnd(qid[1], (void *)&message, sizeof(struct msgbuf), 0) == -1) {
		perror("msgsnd");
		exit(1);
	}





	/*
        DIR *dp;
        struct dirent *files[MAX_FILE]; // text file list
	int find;

        // print txt files of current directory
        dp = opendir(".");
        put_txt_files(win, dp, files);
	find = get_int(win);
	wprintw(win, "Got an input %d\n", find);

        wgetch(win);
        closedir(dp);
	*/

	// remove message queues
	remove_msgq(qid); 

	// close window
        //endwin();

        return 0;
}


// create message queues
void create_msgq(int *qid) {
	// message queue to receive message
	if ((qid[0] = msgget(ftok(".", (int)getpid()), IPC_CREAT|0640)) < 0) {
		perror("msgget");
		exit(1);
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

/*
// print text files in the current directory and put it in inode list
void put_txt_files(WINDOW *window, DIR *dp, struct dirent **list) {
        struct dirent *dent;
        char name[STR_LEN];
        int size;
        int i = 0;
	int ends_txt(char *str, int size); 

        while ((dent = readdir(dp))) {
                size = strlen(dent->d_name);
                if (ends_txt(dent->d_name, size)) {
                        *(list+i) = dent;
			wprintw(window, "%d ", ++i);
                        wprintw(window, "%s\n", dent->d_name);
                }
        }
        *(list+i) = (struct dirent *) NULL;
	wprintw(window, "Which file do you want to send?\n");
        wrefresh(window);
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
*/
