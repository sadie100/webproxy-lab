/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/*
1. 명령줄에서 넘겨받은 포트로의 연결 요청을 듣는다.
2. open_listenfd 함수를 호출해서 듣기 소켓을 오픈한다.
3. 무한 서버 루프를 실행
    - 반복적으로 연결 요청을 접수
    - 트랜잭션을 수행
    - 자신 쪽의 연결 끝을 닫음
*/
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

/*
한 개의 HTTP 트랜잭션을 처리하는 함수
*/
void doit(int fd) {
  // 요청이 static인지 판별하는 변수
  int is_static;
  // ?
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // 인자로 들어온 fd를 rio와 연결
  Rio_readinitb(&rio, fd);
  // rio에 들어온 리퀘스트 라인과 헤더를 읽어서 buf에 저장
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  // buf에 있는 문자들을 각각 method, uri, version에 저장
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {
    /*
    만약 method가 "GET"과 같다면, strcasecmp는 0을 리턴 => 해당 조건문에
    들어오지 않음. 즉, 이 조건문은 method와 "GET"을 비교해서 서로 다를 경우를
    예외처리
    */
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method");
    return;
  }

  /*
  tiny는 요청 헤더 내의 어떤 정보도 사용하지 않음
  => read_requesthdrs 함수를 호출해서 이들을 읽고 무시
  */
  read_requesthdrs(&rio);

  // GET 리퀘스트에서 uri 파싱해서 static인지 dynamic인지를 나타내는 플래그 설정
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    // 파일을 sbuf에 가져오는 과정에서 에러가 나면(파일이 디스크 상에 있지
    // 않으면)
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  }
  // static content 제공
  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      // regular file이 아니거나 read 권한이 없으면 예외 처리
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    // static content 제공
    serve_static(fd, filename, sbuf.st_size);
  } else {
    // dynamic content 제공
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      // regular file이 아니거나 read 권한이 없으면 예외 처리
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    // dynamic content 제공
    serve_dynamic(fd, filename, cgiargs);
  }
}

// http 응답을 응답 라인에 적절한 상태 코드&메시지와 함께 클라이언트에 보내며,
// response body에 html 파일도 보내는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  // HTTP 응답 body를 만듦
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body,
          "%s<body bgcolor="
          "ffffff"
          ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // http 응답 헤더 출력
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  // http 응답 body 출력
  Rio_writen(fd, body, strlen(body));
}

// 요청 헤더를 읽고 무시하는 함수
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];
  // rp에 들어온 입력값을 읽어서 buf에 저장
  Rio_readlineb(rp, buf, MAXLINE);
  // buf가 \r\n이 될 때까지 반복문 돌림
  while (strcmp(buf, "\r\n")) {
    // rp의 입력값을 읽어서 buf에 저장
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// cgi 인자 스트링을 분석하고, 정적인지 동적인지에 따라 uri를 변환하고 isStatic
// 리턴
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;
  // 만약 uri가 'cgi-bin'을 포함하고 있지 않으면
  if (!strstr(uri, "cgi-bin")) { /* Static content */
    // cgiargs의 문자열을 ""로 초기화
    strcpy(cgiargs, "");
    // filename의 문자를 "."로 설정
    strcpy(filename, ".");
    // filename 뒤에 uri를 이어붙임
    strcat(filename, uri);
    // uri의 맨 마지막 문자가 '/'이면
    if (uri[strlen(uri) - 1] == '/') {
      // filename 뒤쪽에 "home.html" 이어 붙이기
      strcat(filename, "home.html");
    }
    // isStatic 1 리턴
    return 1;
  } else { /* Dynamic content */
    // ptr은 ?문자가 등장하는 포인터
    ptr = index(uri, '?');
    if (ptr) {
      // 만약 ?이 있으면 => ? 다음에 나오는 문자들을 cgiargs에 복사
      strcpy(cgiargs, ptr + 1);
      // ptr은 null 문자로 초기화
      *ptr = '\0';
    } else {
      // 만약 ?이 없으면, cgiargs에 빈값 복사
      strcpy(cgiargs, "");
    }
    // filename의 문자를 "."로 설정
    strcpy(filename, ".");
    // filename 뒤에 uri를 이어붙임
    strcat(filename, uri);
    // isStatic 0 리턴
    return 0;
  }
}

/*
Tiny는 다섯 개의 정적 컨텐츠 타입을 지원한다 : HTML, 무형식 텍스트 파일, GIF,
PNG, JPEG로 인코딩된 영상
serve_static은 지역 파일의 내용을 포함해서 respond body를 설정하고 http 응답을
보냄
*/
void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  /* respond header 전송 */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  // srcfd는 response body
  srcfd = Open(filename, O_RDONLY, 0);
  // 파일을 가상메모리 영역으로 맵핑
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = (char *)Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  // 맵핑 후 파일은 필요 없으므로 파일 닫기
  Close(srcfd);
  // fd에 srcp부터 filesize까지의 자리를 복사
  Rio_writen(fd, srcp, filesize);
  // mmap으로 만들어진 맵핑 제거
  // Munmap(srcp, filesize);
  free(srcp);
}

/*
 * get_filetype - Derive file type from filename
 */
void get_filetype(char *filename, char *filetype) {
  // filename에 특정 확장자가 포함돼 있으면, 그에 따라 filetype을 지정
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = {NULL};
  /* Return first part of HTTP response */
  // buf에 http 응답을 입력
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  // buf를 fd에 입력
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /*
  Fork() : 빌트인 fork() 함수 호출.
  fork() : 현재 프로세스를 복사해서 새 프로세스를 만드는 함수.
  새로 만들어진 프로세스는 자식 프로세스로, 호출한 프로세스는 부모 프로세스로
  불림
  */
  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    // cgiargs를 쿼리스트링 환경변수로 정의
    setenv("QUERY_STRING", cgiargs, 1);
    // 자식의 표준 출력을 fd로 재지정
    // => 자식 컨텍스트(CGI 프로그램)에서 표준 출력에 쓰는 모든 것은 직접
    // 클라이언트 프로세스로 전달됨.
    Dup2(fd, STDOUT_FILENO);              /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  // child가 죽을 때까지 기다림
  Wait(NULL); /* Parent waits for and reaps child */
}