#include <sys/stat.h>
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
#define MAX_FILE 10
#define STR_LEN 256


void create_msgq(int *keynum, int *qid); 
void remove_msgq(int qid); 
struct dirent choose_file(WINDOW *window, DIR *dp, int *file_ind);
char *getLine(WINDOW *window, char *buf, int size); 
int chooseOpt(WINDOW *window, pid_t *options, int optlen); 
int get_int(WINDOW *window);
int receive_pid_list(WINDOW *window, int qid, struct msgbuf *message, pid_t *pids, int *qids);
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
	do {
		msg_len = msgrcv(qid[0], &message, sizeof(struct msgbuf), 1, 0);
		if (msg_len < 0) {
			wprintw(win, "Error: Message queue issue\n");
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
	} while(message.msgcontent.type != REGISTER);	



	// User chooses to do Send Fax

	

        DIR *dp;
        struct dirent file;
        int find;
	caddr_t shmaddr;

        // Get user input of what file to send
        dp = opendir(".");
	file = choose_file(win, dp, &find);

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

	pid_t pid_list[MAX_CLIENT - 1]; // pid list to print
	int qid_list[MAX_CLIENT - 1];
	int list_n = 0;
	int qid_r = qid[0];
	int qid_s = qid[1];
	list_n = receive_pid_list(win, qid[0], &message, pid_list, qid_list);
	// the value of qid changes for some unknown reason at this point
	qid[0] = qid_r;
	qid[1] = qid_s;
	wprintw(win, "Recepton qid: %d\n", qid[0]);
	wgetch(win);
	
	
	// if error occured during reception
	if (list_n < 0) {
		wprintw(win, "Error: Unable to receive message\n");
		wgetch(win);
		end_program(win, qid, &message);
	}
	// if there is no other client registered in server
	if (!list_n) {
		wprintw(win, "No other client to send fax\n");
		wgetch(win);
		end_program(win, qid, &message);
	}	

	// print the received pid list and let the user choose
	int rcvr_ind = chooseOpt(win, pid_list, list_n);
	wclear(win);
	
	wprintw(win, "Receiver Selected\n");
	
	int fd;
	caddr_t addr;
	struct stat statbuf;
	
	// Open file to share
	if (stat(file.d_name, &statbuf) == -1) {
		wprintw(win, "Error: stat failed\n");
		wgetch(win);
		end_program(win, qid, &message);
	}
	if ((fd = open(file.d_name, O_RDWR)) == -1) {
		wprintw(win, "Error: unable to open file\n");
		wgetch(win);
		end_program(win, qid, &message);
	}
	
	// assign memory and write the file
	addr = mmap(NULL, SHARED_MEMORY_SIZE, 
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)0);
	if (addr == MAP_FAILED) {
		wprintw(win, "Error: mmap failed\n");
		wgetch(win);
		end_program(win, qid, &message);
	}
	close(fd);
	closedir(dp);
	

	// set message to send server
	message.msgcontent.type = SEND_FAX;
	message.msgcontent.pid = pid_list[rcvr_ind];
	message.msgcontent.qid = qid_r;
	message.msgcontent.addr = addr;
	
	wprintw(win, "Message set\n");
	

	// send message to server to share memory with receiver
	if (msgsnd(qid_s, (void *)&message, sizeof(struct msgbuf), 0) == -1) {
		wprintw(win, "Error: Unable to send message");
		wgetch(win);
		end_program(win, qid, &message);
	}
	
	wprintw(win, "Message sent\n");
	wprintw(win, "qid_r = %d\n", qid_r);
	wprintw(win, "qid[0] = %d, qid[1] = %d\n", qid[0], qid[1]);
	wgetch(win);
	
	msg_len = msgrcv(qid[0], &message, sizeof(struct msgbuf), 1, 0);
	if (msg_len < 0) {
		wprintw(win, "Error: Message queue issue\n");
		wrefresh(win);
		wgetch(win);
		end_program(win, qid, &message);
	}

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

// let the user choose which file to send
// return the struct dirent of the file
struct dirent choose_file(WINDOW *window, DIR *dp, int *file_ind) {
	int get_txt_files(DIR *dp, struct dirent **list);
	void print_txt_files(WINDOW *window, DIR *dp, struct dirent **list, int size);
	struct dirent *files[MAX_FILE]; // text file list
	
	int file_num = get_txt_files(dp, files);
	*file_ind = -1;
	do {
        	print_txt_files(window, dp, files, file_num);
		*file_ind = get_int(window);
	} while(!is_in_range(*file_ind, 0, file_num)); 
	wclear(window);
	wrefresh(window);
	
	return *(files[(*file_ind)-1]);
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

// Receive pid list from server
int receive_pid_list(WINDOW *window, int qid, struct msgbuf *message, pid_t *pids, int *qids) {
	int msg_len;
	int num = 0;
	while(1) {
		//wprintw(window, "My qid: %d\nServer qid: %d\n", qid[0], qid[1]);
		msg_len = msgrcv(qid, message, sizeof(struct msgbuf), 1, 0);
		wprintw(window, "### message received\n");
		//wprintw(window, "My qid: %d\nServer qid: %d\n", qid[0], qid[1]);
		if (msg_len == -1) {
			return -1;
		}
		if (message->msgcontent.type == GET_PIDS) {
			if (message->msgcontent.pid == 0) {
				break;
			}
			pids[num] = message->msgcontent.pid;
			qids[num++] = message->msgcontent.qid;
			wprintw(window, "  Received %d\n", message->msgcontent.pid);
			wprintw(window, "  pid_list[%d] = %d\n", num, pids[num - 1]);
		}
	}
	return num;
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
