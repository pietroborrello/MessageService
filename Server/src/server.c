#include "server.h"

map_t registeredUsers;
map_t msgsToDeliver;
long sem;
int users_file, log_file; // msgs_file

typedef struct handler_args_s {
  int socket_desc;
  struct sockaddr_in *client_addr;
} handler_args_t;

char *sprint_msg(msg_t *msg) {
  char *buf = (char *)calloc(MSG_LEN, sizeof(char));
  sprintf(buf, "%s%c%s%c%s%c%s", msg->from, RECORD_SEP, msg->to, RECORD_SEP,
          msg->object, RECORD_SEP, msg->content);
  return buf;
}

void close_server() {
  fprintf(stderr, "Exiting from server...\n");
  semctl(sem, IPC_RMID, 0);
  fflush(NULL); // flush user buffers
  sync();       // flush kernel buffers
  close(users_file);
  close(log_file);
  hashmap_free(registeredUsers);
  hashmap_free(msgsToDeliver);
  exit(0);
}

int login(int socket_desc, char username[FIELD_LEN], char psw[FIELD_LEN]) {
  int enc_buf[3 * FIELD_LEN];
  char *temp;
  int ret, recv_bytes, sent_bytes, msg_len, attempts = 0;
  users_t *info;
  char isOk;
  while (1) {
    // Username and Password
    ret = 0;
    unsigned char n;
    while (ret < 1) { // bytes to receive
      ret = recv(socket_desc, &n, 1, 0);
      if (ret < 0 && errno == EINTR)
        continue;
      if (ret <= 0)
        return 0;
    }
    recv_bytes = 0;
    while (recv_bytes < n && recv_bytes < 12 * FIELD_LEN) {
      ret = recv(socket_desc, ((char *)enc_buf) + recv_bytes, 1, 0);
      if (ret < 0 && errno == EINTR)
        continue;
      if (ret <= 0)
        return 0;
      recv_bytes += ret;
    }
    // decrypt message
    char *buf =
        decodeMessage(recv_bytes / sizeof(int), 1, enc_buf, RSA_PRIV, RSA_MOD);
    buf[recv_bytes / sizeof(int) - 1] = '\0'; // closing anyway
    fprintf(stderr, "Received: %s\n", buf);
    // get username and psw
    char *context;
    temp = strtok_r(buf, " ", &context);
    if (temp == NULL) {
      free(buf);
      continue;
    }
    // check if asked for registration
    if (strlen(temp) == strlen(REGISTER_CMD) &&
        memcmp(temp, REGISTER_CMD, strlen(REGISTER_CMD)) == 0) {
      // handle registration
      temp = strtok_r(NULL, " ", &context);
      if (temp == NULL) {
        free(buf);
        continue;
      }
      strncpy(username, temp, FIELD_LEN);
      username[FIELD_LEN - 1] = '\0';

      temp = strtok_r(NULL, "\0", &context);
      if (temp == NULL) {
        free(buf);
        continue;
      }
      strncpy(psw, temp, FIELD_LEN);
      psw[FIELD_LEN - 1] = '\0';
      // user yet registered
      if (hashmap_get(registeredUsers, username, (any_t *)&info) == MAP_OK) {
        // Registration failed
        ret = 0;
        isOk = 0;
        while (ret < 1) {
          ret = send(socket_desc, &isOk, 1, 0);
          if (ret < 0 && errno == EINTR)
            continue;
          if (ret < 0)
            free(buf);
          handle_error(ret < 0, "Cannot write to the socket");
        }
        free(buf);
        continue;
      }
      // store in memory new username and psw
      char *mem_user = (char *)malloc(sizeof(char) * (strlen(username) + 1));
      strncpy(mem_user, username, strlen(username) + 1);
      char *mem_psw = (char *)malloc(sizeof(char) * (strlen(psw) + 1));
      strncpy(mem_psw, psw, strlen(psw) + 1);
      users_t *user_info = (users_t *)malloc(sizeof(users_t));
      handle_error(user_info == NULL, "Unable to malloc for user_info");

      user_info->psw = mem_psw;
      // get the whole hashmap, (it will be modified)
      struct sembuf oper[SEM_CELLS];
      for (int i = 0; i < SEM_CELLS; i++) {
        oper[i].sem_num = i;
        oper[i].sem_op = -1;
        oper[i].sem_flg = SEM_UNDO;
      }
      ret = semop(sem, oper, SEM_CELLS);
      handle_error(ret < 0, "Unable to get the whole semaphore");
      // create new user
      ret = hashmap_put(registeredUsers, mem_user, user_info);
      // create new message queue for the new user
      if (ret != MAP_OMEM)
        ret = hashmap_put(msgsToDeliver, mem_user, linked_list_new());
      int err = ret == MAP_OMEM ? 1 : 0;
      // release the whole hashmap
      for (int i = 0; i < SEM_CELLS; i++) {
        oper[i].sem_num = i;
        oper[i].sem_op = 1;
        oper[i].sem_flg = SEM_UNDO;
      }
      ret = semop(sem, oper, SEM_CELLS);
      handle_error(ret < 0, "Unable to release the whole semaphore");
      if (err)
        handle_error(1, "Out of mem for the hashmap");
      // store credentials in users file
      sigset_t set;
      sigemptyset(&set);
      sigaddset(&set, SIGINT);
      sigprocmask(SIG_BLOCK, &set, NULL);

      char credentials[3 * FIELD_LEN];
      sprintf(credentials, "%s %s\n", username, psw);
      int written_bytes = 0, bytes_to_write = strlen(credentials);
      while (written_bytes < bytes_to_write) {
        ret = write(users_file, credentials + written_bytes,
                    bytes_to_write - written_bytes);
        if (ret < 0 && errno == EINTR)
          continue;
        handle_error(ret < 0, "Cannot write to the socket");
        written_bytes += ret;
      }
      sigprocmask(SIG_UNBLOCK, &set, NULL);
    } else {
      strncpy(username, temp, FIELD_LEN);
      username[FIELD_LEN - 1] = '\0';

      temp = strtok_r(NULL, "\0", &context);
      if (temp == NULL) {
        free(buf);
        continue;
      }
      strncpy(psw, temp, FIELD_LEN);
      psw[FIELD_LEN - 1] = '\0';
    }
    free(buf);
    ret = hashmap_get(registeredUsers, username, (any_t *)&info);
    attempts++;
    if (ret == MAP_OK &&
        strnlen(info->psw, FIELD_LEN) == strnlen(psw, FIELD_LEN) &&
        memcmp(info->psw, psw, strnlen(info->psw, FIELD_LEN)) == 0) {
      isOk = 1;
      ret = 0;
      while (ret < 1) {
        ret = send(socket_desc, &isOk, 1, 0);
        if (ret < 0 && errno == EINTR)
          continue;
        handle_error(ret < 0, "Cannot write to the socket");
      }
      return 1;
    } else {
      ret = 0;
      isOk = 0;
      while (ret < 1) {
        ret = send(socket_desc, &isOk, 1, 0);
        if (ret < 0 && errno == EINTR)
          continue;
        handle_error(ret < 0, "Cannot write to the socket");
      }
      if (attempts > 100)
        break;
      continue;
    }
  }
  return 0;
}

int get_id(char *string) {
  int count = 0;
  while (*string)
    count += *(string++);
  return count % SEM_CELLS;
}

int parse_message(char buf[MSG_LEN], msg_t *msg) {
  char sep[2];
  char *context;
  sprintf(sep, "%c", RECORD_SEP);
  char *temp = strtok_r(buf, sep, &context);
  if (temp == NULL)
    return -1;
  msg->from = (char *)malloc(strnlen(temp, FIELD_LEN) + 1);
  if (msg->from == NULL) {
    return -1;
  }
  strncpy(msg->from, temp, strnlen(temp, FIELD_LEN) + 1);
  if (strnlen(temp, FIELD_LEN) == FIELD_LEN)
    msg->from[FIELD_LEN] = '\0';

  temp = strtok_r(NULL, sep, &context);
  if (temp == NULL)
    return -1;
  msg->to = (char *)malloc(strnlen(temp, FIELD_LEN) + 1);
  if (msg->to == NULL) {
    free(msg->from);
    return -1;
  }
  strncpy(msg->to, temp, strnlen(temp, FIELD_LEN) + 1);
  if (strnlen(temp, FIELD_LEN) == FIELD_LEN)
    msg->to[FIELD_LEN] = '\0';

  temp = strtok_r(NULL, sep, &context);
  if (temp == NULL)
    return -1;
  msg->object = (char *)malloc(strnlen(temp, 2 * FIELD_LEN) + 1);
  if (msg->object == NULL) {
    free(msg->from);
    free(msg->to);
    return -1;
  }
  strncpy(msg->object, temp, strnlen(temp, 2 * FIELD_LEN) + 1);
  if (strnlen(temp, 2 * FIELD_LEN) == 2 * FIELD_LEN)
    msg->object[2 * FIELD_LEN] = '\0';

  temp = strtok_r(NULL, sep, &context);
  if (temp == NULL)
    return -1;
  msg->content = (char *)malloc(strlen(temp) + 1);
  if (msg->content == NULL) {
    free(msg->object);
    free(msg->from);
    free(msg->to);
    return -1;
  }
  strncpy(msg->content, temp, strlen(temp) + 1);
  return 0;
}

void send_message(msg_t *msg) {
  int id = get_id(msg->to);
  struct sembuf oper[1];
  int msgs_file;
  oper[0].sem_num = id;
  oper[0].sem_op = -1;
  oper[0].sem_flg = SEM_UNDO;
  int ret = semop(sem, oper, 1);
  handle_error(ret < 0, "Unable to access the semaphore");

  // backup message on file
  char file[64];
  sprintf(file, "%s%s.txt", MSGS_FILE, msg->to);
  msgs_file = open(file, O_WRONLY | O_APPEND | O_CREAT, 0600);
  if (msgs_file < 0) {
    free(msg->from);
    free(msg->to);
    free(msg->object);
    free(msg->content);
    free(msg);
    goto end_send;
  }

  char *temp = sprint_msg(msg);

  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigprocmask(SIG_BLOCK, &set, NULL);

  int written_bytes = 0, bytes_to_write = strlen(temp);
  while (written_bytes < bytes_to_write) {
    ret =
        write(msgs_file, temp + written_bytes, bytes_to_write - written_bytes);
    if (ret < 0 && errno == EINTR)
      continue;
    if (ret < 0)
      break;
    written_bytes += ret;
  }
  free(temp);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
  // send msg to the queue
  linked_list *msg_queue = {0};
  ret = hashmap_get(msgsToDeliver, msg->to, (any_t *)&msg_queue);
  if (ret != MAP_MISSING)
    linked_list_add(msg_queue, msg);
  else {
    free(msg->from);
    free(msg->to);
    free(msg->object);
    free(msg->content);
    free(msg);
  }

  close(msgs_file);
// unlock the semaphore
end_send:
  oper[0].sem_num = id;
  oper[0].sem_op = 1;
  oper[0].sem_flg = SEM_UNDO;
  ret = semop(sem, oper, 1);
  handle_error(ret < 0, "Unable to access the semaphore");
}

void deliver_messages(int socket_desc, char username[FIELD_LEN]) {
  int ret, size;
  linked_list *msgs;
  msg_t *msg;
  // get the lock
  int id = get_id((char *)username);
  struct sembuf oper[1];
  oper[0].sem_num = id;
  oper[0].sem_op = -1;
  oper[0].sem_flg = SEM_UNDO;
  ret = semop(sem, oper, 1);
  handle_error(ret < 0, "Unable to access the semaphore");
  // get message queue
  ret = hashmap_get(msgsToDeliver, username, (any_t *)&msgs);
  // user must exists to get this point (logged in)
  size = linked_list_size(msgs);
  if (size > 127)
    size = 127;
  // send msgs number
  ret = 0;
  while (ret < 1) {
    ret = send(socket_desc, (unsigned char *)&size, 1, 0);
    if (ret < 0 && errno == EINTR)
      continue;
    if (ret < 0)
      break;
  }
  // get every message to deliver to user
  char enc_buf[MSG_LEN];
  // get the psw to crypt with
  users_t *info;
  char key[KEY_LEN] = {0};
  char iv[KEY_LEN] = {IV_INIT};
  hashmap_get(registeredUsers, username, (any_t *)&info);
  memcpy(key, info->psw, strnlen(info->psw, KEY_LEN));
  while (size > 0) {
    memset(enc_buf, 0, MSG_LEN);
    linked_list_get(msgs, 0, (any_t *)&msg);
    char *buf = sprint_msg(msg);
    int msg_len = strlen(buf);
    AES128_CBC_encrypt_buffer((unsigned char *)enc_buf, (unsigned char *)buf,
                              msg_len, (unsigned char *)key,
                              (unsigned char *)iv);
    msg_len += KEY_LEN - (msg_len % KEY_LEN); // round up
    char num = msg_len / KEY_LEN;
    // printf("%s\n", enc_buf);
    ret = 0;
    while (ret < 1) // send lenght of message
    {
      ret = send(socket_desc, &num, 1, 0);
      if (ret < 0 && errno == EINTR)
        continue;
      if (ret < 0)
        break;
    }
    int sent_bytes = 0;
    while (sent_bytes < msg_len) {
      ret = send(socket_desc, enc_buf + sent_bytes, msg_len - sent_bytes, 0);
      if (ret < 0 && errno == EINTR)
        continue;
      if (ret < 0)
        break;
      sent_bytes += ret;
    }
    free(msg->from);
    free(msg->to);
    free(msg->object);
    free(msg->content);
    free(buf);
    ret = linked_list_remove(msgs, 0);
    if (ret == LINKED_LIST_NOK)
      break;
    size--;
  }
  // unlock the semaphore
  oper[0].sem_num = id;
  oper[0].sem_op = 1;
  oper[0].sem_flg = SEM_UNDO;
  ret = semop(sem, oper, 1);
  handle_error(ret < 0, "Unable to access the semaphore");
}

void synchronize_messages(int socket_desc, char username[FIELD_LEN]) {
  int ret, size;
  linked_list *msgs;
  msg_t *msg;
  int id = get_id((char *)username);
  struct sembuf oper[1];
  int msgs_file;
  oper[0].sem_num = id;
  oper[0].sem_op = -1;
  oper[0].sem_flg = SEM_UNDO;
  ret = semop(sem, oper, 1);
  handle_error(ret < 0, "Unable to access the semaphore");

  // remove waiting messages from message queue
  ret = hashmap_get(msgsToDeliver, username, (any_t *)&msgs);
  // user must exists to get this point (logged in)
  while (linked_list_size(msgs) > 0) {
    ret = linked_list_get(msgs, 0, (any_t *)&msg);
    if (ret == LINKED_LIST_NOK)
      break;
    free(msg->from);
    free(msg->to);
    free(msg->object);
    free(msg->content);
    ret = linked_list_remove(msgs, 0);
    if (ret == LINKED_LIST_NOK)
      break;
  }

  // load all messages
  char file[64];
  int eof = 0;
  sprintf(file, "%s%s.txt", MSGS_FILE, username);
  msgs_file = open(file, O_RDONLY | O_CREAT, 0600);
  if (msgs_file < 0)
    eof = 1;
  while (!eof) {
    int read_bytes = 0;
    char buf[MSG_LEN];
    while (read_bytes < MSG_LEN - 1) {
      ret = read(msgs_file, buf + read_bytes, 1);
      if (ret < 0 && errno == EINTR)
        continue;
      if (ret == 0) {
        eof = 1;
        break;
      }
      if (ret < 0)
        break;
      read_bytes += ret;
      if (buf[read_bytes - 1] == MSG_SEP)
        break;
    }
    if (eof)
      break;

    // closing string anyway
    buf[read_bytes] = '\0';
    msg = (msg_t *)malloc(sizeof(msg_t));
    ret = parse_message(buf, msg); // message not well formed
    if (ret != 0) {
      free(msg);
      continue;
    }
    linked_list_add(msgs, msg);
  }
  close(msgs_file);
  // unlock the semaphore
  oper[0].sem_num = id;
  oper[0].sem_op = 1;
  oper[0].sem_flg = SEM_UNDO;
  ret = semop(sem, oper, 1);
  handle_error(ret < 0, "Unable to access the semaphore");

  deliver_messages(socket_desc, username);
}

void *connection_handler(void *arg) {
  int ret, recv_bytes, sent_bytes;
  char enc_buf[MSG_LEN];
  char buf[MSG_LEN];
  // login user: update registeredUsers
  handler_args_t *args = (handler_args_t *)arg;
  char username[FIELD_LEN] = {0}, psw[FIELD_LEN] = {0};
  if (!login(args->socket_desc, username, psw)) {
    // close socket
    ret = close(args->socket_desc);
    handle_error(ret < 0, "Cannot close socket");
    free(args->client_addr);
    free(args);
    errno = ECONNRESET;
    handle_error(1, "Socket closed");
  }
  fprintf(stderr, "Logged in: %s - %s\n", username, psw);
  while (1) {
    memset(enc_buf, 0, MSG_LEN);
    memset(buf, 0, MSG_LEN);
    // accept message from user to deliver to others
    ret = 0;
    unsigned char n;
    while (ret < 1) {
      ret = recv(args->socket_desc, &n, 1, 0); // n = 0 command,!= 0 len of msg
      if (ret < 0 && errno == EINTR)
        continue;
      if (ret <= 0) {
        ret = close(args->socket_desc);
        handle_error(ret < 0, "Cannot close socket");
        free(args->client_addr);
        free(args);
        errno = ECONNRESET;
        handle_error(1, "Socket closed");
      }
    }
    int to_receive = n * KEY_LEN; // 0 if command
    recv_bytes = 0;
    while ((recv_bytes == 0) ||
           (((to_receive == 0 && enc_buf[recv_bytes - 1] != MSG_SEP) ||
             (to_receive != 0 && recv_bytes < to_receive)) &&
            recv_bytes < MSG_LEN - 1)) {
      ret = recv(args->socket_desc, enc_buf + recv_bytes,
                 to_receive == 0 ? 1 : to_receive, 0);
      if (ret < 0 && errno == EINTR)
        continue;
      if (ret <= 0) {
        ret = close(args->socket_desc);
        handle_error(ret < 0, "Cannot close socket");
        free(args->client_addr);
        free(args);
        errno = ECONNRESET;
        handle_error(1, "Socket closed");
      }
      recv_bytes += ret;
    }
    if (to_receive != 0) // decrypt
    {
      unsigned char key[KEY_LEN] = {0};
      unsigned char iv[KEY_LEN] = {IV_INIT};
      memcpy(key, psw, strnlen(psw, FIELD_LEN));
      AES128_CBC_decrypt_buffer((unsigned char *)buf, (unsigned char *)enc_buf,
                                recv_bytes, (unsigned char *)key,
                                (unsigned char *)iv);
    } else {
      memcpy(buf, enc_buf, recv_bytes);
    }
    buf[recv_bytes] = '\0';
    fprintf(stderr, "Received: %s\n", buf);
    // deliver message from msgsToDeliver
    if (strlen(buf) == strlen(GET_CMD) + 1 && // closing char
        memcmp(buf, GET_CMD, strlen(GET_CMD)) == 0) {
      deliver_messages(args->socket_desc, username);
    } else if (strlen(buf) == strlen(SYNC_CMD) + 1 && // closing char
               memcmp(buf, SYNC_CMD, strlen(SYNC_CMD)) == 0) {
      synchronize_messages(args->socket_desc, username);
    } else {
      // parse msg to send
      msg_t *msg = (msg_t *)malloc(sizeof(msg_t));
      ret = parse_message(buf, msg);
      if (ret == 0)
        send_message(msg);
      else {
        free(msg->from);
        free(msg->to);
        free(msg->object);
        free(msg->content);
        free(msg);
      }
    }
  }
}

int main(int argc, char **argv) {
  // open Users and Msgs files (user,  psw) (from|to|obj|cont)
  char *filechars;
  int ret;
  struct stat sb;
  sigset_t set;
  struct sigaction act;

  log_file = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600);
  handle_error(log_file < 0, "Unable to open log_file");

#if 1
  // close stderr
  close(2);
  // redirect all writing
  ret = dup(log_file);
  handle_error(ret < 0, "Unable to dup on log file");
#endif

  fprintf(stderr, "SERVER ON\n");

  users_file = open(USERS_FILE, O_RDWR | O_APPEND | O_CREAT, 0600);
  handle_error(users_file < 0, "Unable to open users_file");
  fstat(users_file, &sb);

  filechars = mmap(NULL, sb.st_size + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE,
                   users_file, 0);
  handle_error(filechars == MAP_FAILED, "users mmap failed");
  filechars[sb.st_size] = '\0';

  // load registeredUsers Hashmap (all offline) (String: user -> (String:
  // hashedPsw, Bool: isOnline))
  // init msgsToDeliver Hashmap (String: user-> Queue: msgs to deliver(From, To,
  // Object, Content)) to void
  registeredUsers = hashmap_new();
  handle_error(registeredUsers == NULL, "Unable to create HashMap");
  msgsToDeliver = hashmap_new();
  handle_error(msgsToDeliver == NULL, "Unable to create HashMap");

  char *context;
  char *user = strtok_r(filechars, " ", &context);
  char *psw;
  users_t *user_info;
  while (user != NULL) {
    fprintf(stderr, "%s - ", user);
    psw = strtok_r(NULL, "\n", &context);
    fprintf(stderr, "%s\n", psw);
    user_info = (users_t *)malloc(sizeof(users_t));
    handle_error(user_info == NULL, "Unable to malloc for user_info");

    user_info->psw = psw;
    // user_info->isOnline = 0;
    ret = hashmap_put(registeredUsers, user, user_info);
    handle_error(ret == MAP_OMEM, "Out of memory for the hashmap");

    ret = hashmap_put(msgsToDeliver, user, linked_list_new());
    handle_error(ret == MAP_OMEM, "Out of memory for the hashmap");

    user = strtok_r(NULL, " ", &context);
  }

  // init semaphores (SEM_CELLS cells) for message queues
  sem = semget(SEM_KEY, SEM_CELLS, IPC_CREAT | IPC_EXCL | 0600);
  if (sem == -1) {
    sem = semget(SEM_KEY, SEM_CELLS, IPC_CREAT | 0600);
    semctl(sem, IPC_RMID, 0);
    sem = semget(SEM_KEY, SEM_CELLS, IPC_CREAT | IPC_EXCL | 0600);
    handle_error(sem < 0, "Unable to create semaphore");
  }
  for (int i = 0; i < SEM_CELLS; i++) {
    ret = semctl(sem, i, SETVAL, 1);
    handle_error(ret < 0, "Cannot initialize semaphore");
  }

  // init server socket
  int server_desc, client_desc;
  struct sockaddr_in server_addr = {0};
  int sockaddr_len = sizeof(struct sockaddr_in);

  // initialize socket for listening
  server_desc = socket(AF_INET, SOCK_STREAM, 0);
  handle_error(server_desc < 0, "Could not create socket");

  server_addr.sin_addr.s_addr =
      INADDR_ANY; // accept connections from any interface
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  // SO_REUSEADDR to quickly restart server after a crash:
  int reuseaddr_opt = 1;
  ret = setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt,
                   sizeof(reuseaddr_opt));
  handle_error(ret < 0, "Cannot set SO_REUSEADDR option");

  // bind address to socket
  ret = bind(server_desc, (struct sockaddr *)&server_addr, sockaddr_len);
  handle_error(ret < 0, "Cannot bind address to socket");

  // start listening
  ret = listen(server_desc, MAX_CONN_QUEUE);
  handle_error(ret < 0, "Cannot listen on socket");

  struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));

  sigfillset(&set);
  sigdelset(&set, SIGINT);
  act.sa_sigaction = close_server;
  act.sa_mask = set;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGINT, &act, NULL);
  sigprocmask(SIG_BLOCK, &set, NULL);

  while (1) {
    // accept incoming connection
    client_desc = accept(server_desc, (struct sockaddr *)client_addr,
                         (socklen_t *)&sockaddr_len);
    if (client_desc == -1 && errno == EINTR)
      continue; // check for interruption by signals
    handle_error(client_desc < 0, "Cannot open socket for incoming connection");
    fprintf(stderr, "Incoming connection accepted...\n");
    // set socket timeout
    struct timeval tv;
    tv.tv_sec = 30; /* 30 Secs Timeout */
    tv.tv_usec = 0;
    ret = setsockopt(client_desc, SOL_SOCKET, SO_RCVTIMEO, (void *)&tv,
                     sizeof(struct timeval));
    handle_error(ret < 0, "Cannot set socket timeout");

    pthread_t thread;
    handler_args_t *thread_args = malloc(sizeof(handler_args_t));
    thread_args->socket_desc = client_desc;
    thread_args->client_addr = client_addr;

    ret =
        pthread_create(&thread, NULL, connection_handler, (void *)thread_args);
    handle_error(ret != 0, "Cannot create a new thread");
    pthread_detach(thread);
    client_addr = calloc(1, sizeof(struct sockaddr_in));
  }
}
