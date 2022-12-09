#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <ncurses.h>
#include "msgtype.h"
	
#define KEY_NUM 1
#define MAX_FILE 20
#define STR_LEN 256


void create_msgq(int *keynum, int *qid); 
void remove_msgq(int qid); 
void print_txt_files(WINDOW *window, DIR *dp, char list[][STR_LEN], int size); 
int get_txt_files(DIR *dp, char list[][STR_LEN]); 
char *getLine(WINDOW *window, char *buf, int size); 
int chooseOpt(WINDOW *window, pid_t *options, int optlen); 
int get_int(WINDOW *window); 
int is_in_range(int num, int least, int most); 
void end_program(WINDOW *window, int *qid, struct msgbuf *message); 

int main() {
	int keynum;
	int qid[2];
	struct msgbuf message;
	message.mtype = 1; // mtype is always 1
	int msg_len;

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
	// enable keyboard input
	keypad(win, true);

	create_msgq(&keynum, qid);

	// register in server
	message.msgcontent = (struct content) { 
					.type = REGISTER, 
					.pid = getpid(),
					.qid = qid[0],
					.addr = NULL };
	if (msgsnd(qid[1], (void *)&message, sizeof(struct msgbuf), 0) == -1) {
		wprintw(win, "Failed to connect server\n");
		end_program(win, qid, &message); 
	}
	msg_len = msgrcv(*qid, &message, sizeof(struct msgbuf), 1, 0);
	if (msg_len < 0) {
		wprintw(win, "Message queue issue\n");
		wrefresh(win);
		wgetch(win);
		end_program(win, qid, &message);
	}
	if (message.msgcontent.type == UNREGISTER) {
		wprintw(win, "Unable to register\n");
		wrefresh(win);
		wgetch(win);
        	// remove message queue
        	remove_msgq(*qid);
		endwin();
		exit(1);
	}
	/*
	if (message.msgcontent.type == REGISTER) {
		wprintw(win, "Server Registeration Successful\n");
		wgetch(win);
		wrefresh(win);
	}
	*/	



	// User chooses to do Send Fax

	

        DIR *dp;
        char files[MAX_FILE][STR_LEN]; // text file list
	int find, fnum;
	void *shmaddr;

        // print txt files of current directory
        dp = opendir(".");
	fnum = get_txt_files(dp, files);
	do {
        	print_txt_files(win, dp, files, fnum);
		find = get_int(win);
	} while(!is_in_range(find, 0, fnum)); 
	wclear(win);
	wrefresh(win);

	// exit if user canceled
	if (find == 0) {
		wprintw(win, "Canceled\n");
		wgetch(win);
		end_program(win, qid, &message); 
	}

	// send message to server to give pid list
	message.msgcontent.type = GET_PIDS;
	if (msgsnd(qid[1], (void *)&message, sizeof(struct msgbuf), 0) == -1) {
		perror("msgsnd");
		exit(1);
	}

	pid_t pid_list[3]; // pid list to print
	int qid_list[3];
	int list_n = 0;
	while(1) {
		msg_len = msgrcv(*qid, &message, sizeof(struct msgbuf), 1, 0);
		if (msg_len == -1) {
			wprintw(win, "Error: unable to receive message\n");
			wgetch(win);
			//end_program(win, qid, &message);
		}
		if (message.msgcontent.type == GET_PIDS) {
			if (message.msgcontent.qid == 0) {
				break;
			}
			pid_list[list_n++] = message.msgcontent.pid;
			qid_list[list_n++] = message.msgcontent.qid;
			wprintw(win, "Received %d\n", message.msgcontent.pid);
		}
	} 
	// if there is no other client registered in server
	if (!list_n) {
		wprintw(win, "No other client to send fax\n");
		wgetch(win);
		end_program(win, qid, &message);
	}	

	// print the received pid list and let the user choose
	int rcvr_ind = chooseOpt(win, pid_list, list_n);

	// set message to send server
	message.msgcontent.type = SEND_FAX;
	message.msgcontent.pid = getpid();
	message.msgcontent.qid = keynum;
	message.msgcontent.addr = NULL;

	// send message to server to give shared memory address
	if (msgsnd(qid[1], (void *)&message, sizeof(struct msgbuf), 0) == -1) {
		perror("msgsnd");
		exit(1);
	}

	// receive the shared memory address
	msg_len = msgrcv(*qid, &message, sizeof(struct msgbuf), 1, 0);
	if (msg_len == -1) {
		wprintw(win, "Error: unable to receive message\n");
		wgetch(win);
		end_program(win, qid, &message);
	}
	wprintw(win, "Received shared memory address\n");
	shmaddr = message.msgcontent.addr;
	
	// Put the content of the file in shmaddr
	int fd, file_len;
	fd = open(files[find], O_RDONLY);
	if (fd = -1) {
		wprintw(win, "Error: unable to open file\n");
	}
	
	file_len = read(fd, shmaddr, SHARED_MEMORY_SIZE);
	if (file_len == -1) {
		wprintw(win, "Error: unable to read file\n");
	}
	
	// Send message to server that the file is all wirtten on the shared memory
	message.msgcontent.qid = qid_list[rcvr_ind];
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
void create_msgq(int *keynum, int *qid) {
	// message queue to receive message
	*keynum = (int)getpid() % 253 + 2;
	while ((qid[0] = msgget(ftok(".", *keynum), IPC_CREAT|IPC_EXCL|0640)) < 0) {
		if (++(*keynum) > 255) {
			*keynum = 2;
		}
	}

	// message queue to send message
	if ((qid[1] = msgget(ftok(".", KEY_NUM), 0)) < 0) {
		perror("msgget");
		exit(1);
	}

}

void remove_msgq(int qid) {
	if (msgctl(qid, IPC_RMID, (struct msqid_ds *)NULL) == -1) {
		printf("Error: Failed to delete message queue\n");
	}
}

// print text files in inode list
// return the number of files
void print_txt_files(WINDOW *window, DIR *dp, char list[][STR_LEN], int size) {
	wclear(window);

	for (int i = 0; i < size; i++) {
		wprintw(window, "%d ", i+1);
                wprintw(window, "%s\n", list[i]);
        }
	wprintw(window, "Which file do you want to send?(0 is for cancel)\n");
        //wrefresh(window);
}

// put text files of dp in the inode list
// return the number of files
int get_txt_files(DIR *dp, char list[][STR_LEN]) {
	int ends_txt(char *str, int size); 
	struct dirent *dent;
        int size;
        int file_num = 0;

        while ((dent = readdir(dp))) {
                size = strlen(dent->d_name);
                if (ends_txt(dent->d_name, size)) {
                	int i;
                	for (i=0; i < size; i++) {
                        	*(*(list+file_num)+i) = *((dent->d_name)+i);
                        }
                        *(*(list+file_num)+i) = '\0';
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

// Get an option from user
int chooseOpt(WINDOW *window, pid_t *options, int optlen) {
	int choice;
	int highlight = 0;

	wclear(window);
	wprintw(window, "Choose an pid to send file\n");

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
	wclear(window);

	return highlight;
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
        // remove message queue
        remove_msgq(*qid);
        // send message to server to unregister
        message->mtype = 1;
        message->msgcontent.type = UNREGISTER;
	message->msgcontent.pid = getpid();
	message->msgcontent.qid = *qid;
        if (msgsnd(qid[1], (void *)message, sizeof(struct msgbuf), 0) == -1) {
                perror("msgsnd");
                exit(1);
        }

        // close window
        endwin();
	exit(1);
}
