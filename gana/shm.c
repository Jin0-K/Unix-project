#include <sys/types.h>
#include <sys/shm.h>
#include <ncurses.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(void){
    int shmid, i;
    void* shmaddr, *shmaddr2;

    shmid = shmget(IPC_PRIVATE, 20, IPC_CREAT|0644);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    switch (fork()) {
        case -1:
            perror("fork");
            exit(1);
            break;
        case 0:
	    initscr();
            shmaddr = shmat(shmid, NULL, 0);
            WINDOW* win = newwin(10,10,10,10);
            wprintw(win, "Child Process =====\n");
	    shmaddr = win;
	    shmdt(shmaddr);
            exit(0);
            break;
        default:
            wait(0);
            shmaddr2 = shmat(shmid, NULL, 0);
            WINDOW* win2 = shmaddr2;
	    wprintw(win2, "hello");
            wprintw(win2,"Parent Process =====\n");
            sleep(5);
            shmdt(shmaddr2);
            shmctl(shmid, IPC_RMID, NULL);
	    
	    getch();
	    endwin();
            break;
    }
}
