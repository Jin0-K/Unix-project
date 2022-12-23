#include <sys/types.h>
#include <ncurses.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(void){

	pid_t pid;
	int status;

	switch(pid = fork()){
		case -1:
			perror("fork");
			exit(1);
			break;

		case 0:
			printf("child proc\n");
			execl("printNcur", "printNcur",  (char*)NULL);
			exit(0);
			break;

		default:
			initscr();
			WINDOW* mywin;
			printw("hello parent!");
			refresh();
			getch();
			endwin();
			//wait(&status);
			printf("parent proc\n");
			break;
	}

	return 0;
}
