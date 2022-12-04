#ifndef MSGTYPE_H_
#define MSGTYPE_H_

typedef enum 
{ 
	REGISTER, 
	UNREGISTER, 
	SEND_FAX, 
	GET_FAX 
}MsgType;

struct content {
	MsgType type;
	pid_t pid;
};

struct msgbuf {
        long mtype;
        struct content msgcontent;
};


#endif
