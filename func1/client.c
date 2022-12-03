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

#define SHARED_MEMORY_SIZE 1024*1024
#define MAX_FILE 20
#define STR_LEN 256

void put_txt_files(WINDOW *window, DIR *dp, struct dirent **list); 

int main() {
	// create a key for IPC
        key_t key = ftok(".", (int)getpid());

        // create shared memory
        int shmid = shmget(key, SHARED_MEMORY_SIZE, IPC_CREAT|0666);
        void *shmaddr = shmat(shmid, NULL, 0); // will be used by listener client

        DIR *dp;
        struct dirent *files[MAX_FILE]; // text file list

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

        // print txt files of current directory
        dp = opendir(".");
        put_txt_files(win, dp, files);
        closedir(dp);

        // print to check files
        for (int i = 0; files[i] != NULL; i++) {
                wprintw(win, "%d\n", (int) (*(files+i))->d_ino);
        }

        wgetch(win);

        endwin();

        return 0;
}



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
                        *(list+i++) = dent;
                        wprintw(window, "%s\n", dent->d_name);
                }
        }
        *(list+i) = (struct dirent *) NULL;
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
