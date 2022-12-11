#include <ncurses.h>
#include <stdio.h>

int main(void){
	move(15,15);
	addch('a');
	printf("dddd");
	printf("kkkk");
	mvprintw(15,15,"ssss");
	printw("ffff");
}
