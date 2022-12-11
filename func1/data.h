#define SIZE 50

typedef struct msgbuf{
	long mtype;
	char mtext[SIZE];
} Msgbuf;

char* keyfile = "ganaKeyFile";
int keynum = 33;

enum TYPE {NEWFILE=1, STDOUT=2, FAX=3}; 
