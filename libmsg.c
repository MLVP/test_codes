//gcc -shared -o libmsg.so -fPIC libmsg.c 
//cp libmsg.so /usr/lib/libmsg.so
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/types.h>

#define msgflg (IPC_CREAT | 0666)
#define MSGSZ     256

int msgid;							// Идентификатор очереди
//key_t key = 17;						// Ключ очереди

typedef struct msgbuf {
    long    mtype;
    char    mtext[MSGSZ];
} message_buf;


int get_msg(key_t key, char *buff) {
	message_buf  rbuf;
	int cc;
	msgid = msgget(key, msgflg);
	if (msgid == -1) {
		return -1;
	}else{
		cc = msgrcv(msgid, &rbuf, MSGSZ, 1, 0);
		if (cc < 0) {
			return -1;
		}else{
			//buff = rbuf.mtext;
			memcpy(buff, rbuf.mtext, cc);
			return cc;
		}
	}
}

int set_msg(key_t key, char *buff, int buffl) {
	message_buf  sbuf;
	size_t buf_length;
	//buff[buffl] = 0;
	
	msgid = msgget(key, msgflg);
	if (msgid == -1) {
		return -1;
	}else{
		sbuf.mtype = 1;
		//strcpy(sbuf.mtext, buff);
		memcpy(sbuf.mtext, buff, buffl);
		//buf_length = strlen(sbuf.mtext);
		return msgsnd(msgid, &sbuf, buffl, IPC_NOWAIT);
	}
}


