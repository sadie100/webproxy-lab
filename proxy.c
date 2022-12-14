#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_HASH_TABLE_SIZE (1 << 16)

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char* request_line_format = "GET %s HTTP/1.0\r\n";
static const char* host_header_format = "Host: %s\r\n";
static const char* end_of_header = "\r\n";

void process(int fd);
void parse_uri(char* uri, char* hostname, int* port, char* path);
void set_http_request_header(char* http_header, char* hostname, int* port,
                             char* path, rio_t* rio);
int connect_to_server(char* hostname, int port);

void* thread_main(void* targs);
unsigned int get_hash_key(char* string);
void set_table_entry(unsigned int hash_key);

// typedef : 별칭
typedef struct cached_data {
  char is_used;                // 1 or 0
  char data[MAX_OBJECT_SIZE];  // 실제 데이터가 저장되는 공간
} cached_data_t;

// 캐시 테이블 cache_table을 0으로 초기화
cached_data_t cache_table[MAX_HASH_TABLE_SIZE] = {0};

int main(int argc, char** argv) {
  printf("%s", user_agent_hdr);
  char hostname[MAXLINE], port[MAXLINE];
  int fd_listen;
  // Client to Proxy
  int fd_client;

  socklen_t client_len;
  struct sockaddr_storage client_addr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  fd_listen = Open_listenfd(argv[1]);
  while (1) {
    client_len = sizeof(client_addr);
    fd_client = Accept(fd_listen, (SA*)&client_addr, &client_len);
    Getnameinfo((SA*)&client_addr, client_len, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s %s).\n", hostname, port);
    // Do forward and reverse
    pthread_t thread;
    /*
    새 스레드 생성
    pthread_t는 스레드 식별 번호
    스레드 식별 번호를 부여해서 스레드를 생성하고, &fd_client를 인자로 가진
    thread_main 함수를 돌리도록 호출
    */
    pthread_create(&thread, NULL, thread_main, &fd_client);
  }

  return 0;
}

// 프록시가 할 일(클라이언트-서버 중개 역할)
void process(int fd) {
  // Proxy to Server
  int fd_server;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char http_header_to_server[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE];
  int port = 0;

  rio_t rio_client;
  rio_t rio_server;

  Rio_readinitb(&rio_client, fd);
  Rio_readlineb(&rio_client, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);

  if (strcasecmp(method, "GET")) {
    printf("[%s] This method isn't implemented on Proxy server.\n", method);
    return;
  }
  // if hit cache
  unsigned int hash_key = get_hash_key(uri);
  if (cache_table[hash_key].is_used) {
    // 캐시 테이블에 이미 있는 데이터이면
    char* cached_data_buffer = cache_table[hash_key].data;
    Rio_writen(fd, cached_data_buffer, strlen(cached_data_buffer));
    return;
  }

  parse_uri(uri, hostname, &port, path);
  // 서버에 보낼 헤더 만듦
  set_http_request_header(http_header_to_server, hostname, &port, path,
                          &rio_client);

  // 프록시를 서버와 연결
  fd_server = connect_to_server(hostname, port);
  if (fd_server < 0) {
    printf("Connection failed");
    return;
  }

  // fd_server를 rio 패키지와 연결
  Rio_readinitb(&rio_server, fd_server);
  // 만들어둔 헤더를 fd_server에 쓰기
  Rio_writen(fd_server, http_header_to_server, strlen(http_header_to_server));

  // 캐쉬 테이블 초기화(해쉬 충돌이 났을 때 데이터가 꼬이는 걸 막기 위함)
  set_table_entry(hash_key);
  char* cache_buf = cache_table[hash_key].data;

  size_t len;
  while ((len = Rio_readlineb(&rio_server, buf, MAXLINE))) {
    printf("Proxy received %ld Bytes and send\n", len);
    Rio_writen(fd, buf, len);
    // cache_buf 버퍼에 buf의 데이터를 len만큼 복사
    // 캐싱하는 과정
    memcpy(cache_buf, buf, len);
    cache_buf += len;
    //...
  }
  // 연결 끝나면 close
  Close(fd_server);
}

// uri 파싱하는 함수
void parse_uri(char* uri, char* hostname, int* port, char* path) {
  // ex. https://localhost:8884 <=> 192.168.0.1:8884
  char* port_pos = '\0';
  char* path_pos = '\0';
  char* hostname_pos = strstr(uri, "//");
  // https://localhost~ vs localhost~
  /*
  //가 주소에 있는지(https://localhost) 없는지(192.168.0.1:) 체크해서 있으면
  //까지를 떼냄
  */
  hostname_pos = hostname_pos ? hostname_pos + 2 : uri;
  port_pos = strstr(hostname_pos, ":");
  // localhost:8884~
  if (port_pos) {
    // 포트가 있을 경우(:이 있는 경우)
    *port_pos = '\0';
    sscanf(hostname_pos, "%s", hostname);
    sscanf(port_pos + 1, "%d%s", port, path);
  }
  // localhost~
  // 포트가 없을 경우
  else {
    path_pos = strstr(hostname_pos, "/");
    if (path_pos) {
      *path_pos = '\0';
      sscanf(hostname_pos, "%s", hostname);
      sscanf(path_pos, "%s", path);
    } else {
      sscanf(hostname_pos, "%s", hostname);
    }
  }
}

// 인자값으로 http 헤더 구성
void set_http_request_header(char* http_header, char* hostname, int* port,
                             char* path, rio_t* rio_client) {
  char buf[MAXLINE];
  char request_header[MAXLINE];
  char general_header[MAXLINE];
  char host_header[MAXLINE];

  sprintf(request_header, request_line_format, path);
  while (Rio_readlineb(rio_client, buf, MAXLINE)) {
    if (!strcmp(buf, end_of_header)) {
      break;
    }
    strcat(general_header, buf);
  }

  if (strlen(host_header) == 0) {
    sprintf(host_header, host_header_format, hostname);
  }

  sprintf(http_header, "%s%s%s%s", request_header, user_agent_hdr,
          general_header, end_of_header);
}

int connect_to_server(char* hostname, int port) {
  char port_string[8];
  sprintf(port_string, "%d", port);
  return Open_clientfd(hostname, port_string);
}

// 스레드 메인 함수
void* thread_main(void* targs) {
  /*
   스레드 메인 함수를 돌리고 있는 스레드 자신(pthread_self())을 프로그램 메인
   스레드에서 detach => 독자적으로 수행되게 됨
  */
  pthread_detach(pthread_self());

  process(*(int*)targs);

  // 프로세스가 끝나면 중개하던 소켓 닫음
  Close(*(int*)targs);

  return NULL;
}

unsigned int get_hash_key(char* string) {
  unsigned long long hash = 5381;
  char* ptr = string;
  while (*ptr) {
    // 해쉬 테이블 생성할 때 자주 쓰는 해쉬 설계 함수(djb2)
    hash = ((hash << 5) + hash) + *ptr;
    ptr++;
  }
  return (unsigned int)(hash % MAX_HASH_TABLE_SIZE);
}

void set_table_entry(unsigned int hash_key) {
  cache_table[hash_key].is_used = 1;
  // memset : 메모리 세팅. 지정한 메모리 위치에서부터의 크기를 지정한 값으로
  // 초기화(위치, 초기화할 값, 크기)
  memset(cache_table[hash_key].data, 0, MAX_OBJECT_SIZE);
}