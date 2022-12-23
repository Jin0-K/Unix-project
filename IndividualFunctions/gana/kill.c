#include <signal.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char* argv[]){

	for(int i=0; i<10; i++){
		printf("%d-%d\n", getpid(), i);
		sleep(1);
	}
	kill(getppid(), SIGUSR2);
}
