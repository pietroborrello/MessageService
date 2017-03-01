#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <signal.h>
#include "hashmap.h"
#include "linked_list.h"
#include "aes.h"
#include "rsa.h"

#define handle_error(cond,msg) \
  do { if(cond) {perror(msg); pthread_exit(NULL); } } while (0)

typedef struct msg_s {
  char* from;
  char* to;
  char* object;
  char* content;
} msg_t;

typedef struct user_s {
  char* psw;
  //short isOnline;
} users_t;

#define USERS_FILE "../files/users.txt"
#define MSGS_FILE "../files/msgs/"
#define LOG_FILE "../files/log.txt"
#define OUT_ON_LOG_FILE 1
#define SEM_KEY 1234
#define SEM_CELLS 5 //server:256 (500 max), mac:5 max
#define SERVER_PORT 12345
#define MAX_CONN_QUEUE 64
#define FIELD_LEN 16
#define MSG_LEN 1024
#define GET_CMD "get"
#define REGISTER_CMD "register"
#define SYNC_CMD "sync"
#define RECORD_SEP 0x1e
#define MSG_SEP 0x1f
#define KEY_LEN FIELD_LEN
#define IV_INIT 115
#define RSA_MOD 48829241
#define RSA_PRIV 45019291


#endif //_SERVER_H_
