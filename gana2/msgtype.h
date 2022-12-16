#ifndef MSGTYPE_H_
#define MSGTYPE_H_

#define KEY_NUM 1
#define MAX_CLIENT 4
#define SHARED_MEMORY_SIZE 1024*512

typedef enum 
{ 
	REGISTER, 
	UNREGISTER, 
	GET_PIDS,
	SEND_FAX, 
	GET_FAX 
}MsgType;

struct cont {
	MsgType type;
	pid_t pid;
	int qid;
};

struct message {
        long mtype;
        struct cont content;
};


#endif
