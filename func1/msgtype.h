#ifndef MSGTYPE_H_
#define MSGTYPE_H_

#define SHARED_MEMORY_SIZE 1024*1024

typedef enum 
{ 
	REGISTER, 
	UNREGISTER, 
	GET_PIDS,
	SEND_FAX, 
	GET_FAX 
}MsgType;

struct content {
	MsgType type;
	pid_t pid;
	int qid;
	caddr_t addr;
};

struct msgbuf {
        long mtype;
        struct content msgcontent;
};


#endif
