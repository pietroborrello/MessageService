#define SERVER_PORT 12345
#define SERVER_ADDR "46.101.199.176" 
//#define SERVER_ADDR "192.168.56.1" 
#define FIELD_LEN 16
#define MSG_LEN 1024
#define REGISTER "register"
#define GET_CMD "get"
#define SYNC "sync"
#define RECORD_SEP 0x1e
#define MSG_SEP 0x1f
#define KEY_LEN FIELD_LEN
#define IV_INIT 115
#define RSA_MOD 48829241
#define RSA_PUB 643

#define SEND_CMD 1
#define SEND_EDIT 2
#define RECV_CMD 3
#define ID_LIST 4
#define NEW_CMD 5
#define SHOW_CMD 6
#define RM_CMD 7
#define BACK_CMD 8
#define LOGIN_CMD 9
#define REGISTER_CMD 10
#define SYNC_CMD 11

typedef struct tagMSGINFO {
	_TCHAR from[FIELD_LEN];
	_TCHAR obj[FIELD_LEN*2];
	_TCHAR msg[MSG_LEN];
} MSGINFO;

#define handle_error(cond,msg) \
  do { if(cond) {perror(msg); MessageBox(NULL, TEXT(msg), NULL, MB_OK | MB_ICONERROR); ExitProcess(1); } } while (0)