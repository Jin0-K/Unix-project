#ifndef MSGTYPE_H_
#define MSGTYPE_H_

#define SHARED_MEMORY_SIZE 1024*512

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
	void *addr;
};

struct msgbuf {
        long mtype;
        struct content msgcontent;
};


#endif
