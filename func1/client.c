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
#include "data.h"

#define MAX_FILE 10

void sig_handler_getfax(int signo);
void create_msgq(int *keynum, int *qid); 
void remove_msgq(); 
struct dirent choose_file(WINDOW *window, DIR *dp, int *file_ind);
char *getLine(WINDOW *window, char *buf, int size); 
int choosePid(WINDOW *window, pid_t *options, int optlen); 
int receive_pids(WINDOW *window, int qid, struct message *message, pid_t *pids);
void end_program(WINDOW *window); 

int cli_keynum;
int qid[2];
	
int main() {
	struct message message;
	message.mtype = 1; // mtype is always 1
	int msg_len;
	
	void (*hand)(int);
	hand = signal(SIGALRM, sig_handler_getfax);
	if (hand == SIG_ERR) {
		perror("signal");
		exit(1);
	}

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

	create_msgq(&cli_keynum, qid);

	// register in server
	message.content = (struct cont) { 
					.type = REGISTER, 
					.pid = getpid(),
					.qid = qid[0] };
	if (msgsnd(qid[1], (void *)&message, sizeof(struct message), 0) == -1) {
		wprintw(win, "Failed to connect server\n");
		end_program(win); 
	}
	msg_len = msgrcv(qid[0], &message, sizeof(struct message), 1, 0);
	if (msg_len < 0) {
		//wprintw(win, "Error: Message queue issue\n");
		//wgetch(win);
		//end_program(win, qid, &message);
		endwin();
		perror("msgrcv");
		exit(1);
	}
	if (message.content.type == UNREGISTER) {
		wprintw(win, "Unable to register\n");
		wrefresh(win);
		wgetch(win);
		// remove message queue
		remove_msgq();
		endwin();
		exit(1);
	}



	// User chooses to do Send Fax

	

        DIR *dp;
        struct dirent file;
        int find;
	char *shmaddr;

        // Get user input of what file to send
        dp = opendir(".");
	file = choose_file(win, dp, &find);

	

	// send message to server to give pid list
	message.content.type = GET_PIDS;
	if (msgsnd(qid[1], (void *)&message, sizeof(struct message), 0) == -1) {
		perror("msgsnd");
		exit(1);
	}

	pid_t pid_list[MAX_CLIENT - 1]; // pid list to print
	int list_n = 0;
	list_n = receive_pids(win, qid[0], &message, pid_list);
	
	
	// if error occured during reception
	if (list_n < 0) {
		wprintw(win, "Error: Unable to receive message\n");
		wgetch(win);
		end_program(win);
	}
	// if there is no other client registered in server
	if (!list_n) {
		wprintw(win, "No other client to send fax\n");
		wgetch(win);
		end_program(win);
	}	

	// print the received pid list and let the user choose
	int rcvr_ind = choosePid(win, pid_list, list_n);
	wclear(win);
	
	int fd;
	int shmid;
	char *addr;
	struct stat statbuf;
	
	// Open file to share
	if (stat(file.d_name, &statbuf) == -1) {
		wprintw(win, "Error: stat failed\n");
		wgetch(win);
		end_program(win);
	}
	if ((fd = open(file.d_name, O_RDWR)) == -1) {
		wprintw(win, "Error: unable to open file\n");
		wgetch(win);
		end_program(win);
	}
	
	// assign memory
	shmid = shmget(ftok(".", cli_keynum), statbuf.st_size, IPC_CREAT|0666);
	if (shmid == -1) {
		wprintw(win, "Error: Unable to create shared memory\n");
		wgetch(win);
		end_program(win);
	}
	addr = shmat(shmid, NULL, 0);
	
	// read file to put it in shared memory
	if (read(fd, addr, statbuf.st_size) == -1) {
		wprintw(win, "Error: unable to read file\n");
		wgetch(win);
		end_program(win);
	}
	
	close(fd);
	closedir(dp);
	

	// set message to send server
	message.content.type = SEND_FAX;
	message.content.pid = pid_list[rcvr_ind];
	message.content.qid = shmid;
	
	wprintw(win, "Message set\n");
	

	// send message to server to share memory with receiver
	if (msgsnd(qid[1], (void *)&message, sizeof(struct message), 0) == -1) {
		wprintw(win, "Error: Unable to send message");
		wgetch(win);
		end_program(win);
	}
	
	wprintw(win, "Message sent\n");
	//wprintw(win, "qid_r = %d\n", qid_r);
	wprintw(win, "qid[0] = %d, qid[1] = %d\n", qid[0], qid[1]);
	wgetch(win);

	end_program(win); 

        return 0;
}


// FUNCTIONS

// operation when received GET_FAX
void sig_handler_getfax(int signo) {
	void send_msg_printer(struct message *message);
	int msg_len;
	struct message msg;
	
	// Receive message
	msg_len = msgrcv(qid[0], &msg, sizeof(struct message), 1, 0);
	send_msg_printer(&msg);
}

// Send the received message to printer
void send_msg_printer(struct message *message) {
	int msqid;
	Msgbuf msg_send;
	
	msg_send.mtype = FAX;
	sprintf(msg_send.mtext, "%d", message->content.qid);
	
	// send the message to printer message queue
	msqid = msgget(ftok(keyfile, keynum), 0);
	if (msgsnd(msqid, (void *)&msg_send, SIZE, IPC_NOWAIT) == -1) {
		perror("msgsnd");
	}
}

// create message queues
void create_msgq(int *key_num, int *qids) {
	// message queue to receive message
	*key_num = (int)getpid() % 253 + 2;
	// don't let client key num value same as printer key num
	if (*key_num == keynum) {
		(*key_num)++;
	}
	// reset key_num if it already exists
	while ((qids[0] = msgget(ftok(".", *key_num), IPC_CREAT|IPC_EXCL|0640)) < 0) {
		if (++(*key_num) > 255) {
			*key_num = 2;
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

// let the user choose which file to send
// return the struct dirent of the file
struct dirent choose_file(WINDOW *window, DIR *dp, int *file_ind) {
	int get_txt_files(DIR *dp, struct dirent **list);
	int chooseFile(WINDOW *window, struct dirent **options, int optlen);
	struct dirent *files[MAX_FILE]; // text file list
	int file_num;
	
	file_num = get_txt_files(dp, files);
	*file_ind = chooseFile(window, files, file_num);
	wclear(window);
	wrefresh(window);
	
	// exit if user canceled
	if (*file_ind == file_num) {
		wprintw(window, "Canceled\n");
		wgetch(window);
		end_program(window); 
	}
	
	return *(files[*file_ind]);
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

// Get an option input from user
int chooseFile(WINDOW *window, struct dirent **options, int optlen) {
	int choice;
	int file_num = optlen + 1;
	int highlight = 0;

	wclear(window);
	wprintw(window, "Choose an pid to send file\n");

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

// Receive pid list from server
int receive_pids(WINDOW *window, int qid_r, struct message *message, pid_t *pids) {
	int msg_len;
	int num = 0;
	while(1) {
		msg_len = msgrcv(qid_r, message, sizeof(struct message), 1, 0);
		if (msg_len == -1) {
			return -1;
		}
		if (message->content.type == GET_PIDS) {
			if (message->content.pid == 0) {
				break;
			}
			pids[num++] = message->content.pid;
			wprintw(window, "  Received %d\n", message->content.pid);
			wprintw(window, "  pid_list[%d] = %d\n", num-1, pids[num - 1]);
		}
	}
	return num;
}


int choosePid(WINDOW *window, pid_t *options, int optlen) {
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

// end program
void end_program(WINDOW *window) {
        // remove message queue
        remove_msgq();
        // send message to server to unregister
        struct message msg;
        msg.mtype = 1;
        msg.content.type = UNREGISTER;
	msg.content.pid = getpid();
	msg.content.qid = qid[0];
        if (msgsnd(qid[1], (void *)&msg, sizeof(struct message), 0) == -1) {
                perror("msgsnd");
                exit(1);
        }

        // close window
        endwin();
	exit(1);
}
