#include <sys/types.h>
#include <sys/ipc.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

pid_t printoutPid;

// 출력 취소 시그널
// SIGTSTP
void kill_printout_handler(int signo){
	// 없는 프세pid에 시그널 보내면 어떻게 되는지 알아보기.
	// 아니면 pid에 해당하는 프세가 있으면 시그널 보내기
	kill(printoutPid, SIGTSTP);
}

// 출력 완료되었다는 시그널
// SIGUSR2
void complete_print_handler(int signo){
	// 함수 변경 필요
	sigrelse(SIGUSR1);
}

// 출력 시그널
// SIGUSR1
void print_handler(int signo){
	// 다른 시그널 다 막아야하나?
	sighold(SIGUSR1);

	// 메세지 큐에 내용이 없다면 함수 끝내기.
	// 메세지 큐 맨 압의 내용 읽어오기
	// 자식 프세 만들기
}


int main(void){
	
	key_t key;

	key = ftok("keyfile", 33);

}	
