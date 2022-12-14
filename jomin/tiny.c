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
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* 주요 함수 목록

I/O 함수 : 데이터를 읽고 쓰기 위한 함수로서 Linux I/O 함수를 기반으로 가공한 wrapper 함수
  rio_readinitb : read 버퍼를 초기화 하는 함수
  rio_readlineb : 파일에서 텍스트 라인을 읽어 버퍼에 담는 함수
  rio_readnb : 파일에서 지정한 바이트 크기를 버퍼에 담는다.
  rio_writen : 버퍼에서 파일로 지정한 바이트를 전송하는 함수

main : 웹 서버 메인 로직
  open_listenfd : 서버가 연결을 받기 위한 함수
  doit : 한 개의 HTTP 트랜잭션을 처리하는 함수
  read_requesthdrs : request header를 읽는 함수
  parse_uri : 클라이언트가 요청한 URI를 파싱하는 함수
  static_serve : 정적 콘텐츠를 제공하는 함수
  dynamic_serve : 동적 콘텐츠를 제공하는 함수
  get_filetype : 받아야 하는 filetype을 명시하는 함수
  client_error : 에러 처리를 위한 함수
*/

// main 함수 -----------------------------------------------------------------------------------------------------
// 터미널에 ./tiny 8888 입력 / argc = 2, argv[0] = tiny, argv[1] = 8888

int main(int argc, char **argv) {
  int listenfd, connfd; // 리스닝 소켓, 커넥션 소켓
  char hostname[MAXLINE], port[MAXLINE]; // 호스트 이름(주소), 포트번호
  socklen_t clientlen; // 주소길이 저장할 변수
  struct sockaddr_storage clientaddr; // ??

  // 입력 받은 매개변수의 갯수가 2개가 아니면 
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  // Open_listenfd 함수로 듣기 소켓을 오픈한다. 인자로는 포트번호를 넘겨주고 리턴값으로 듣기 식별자를 리턴받는다.
  listenfd = Open_listenfd(argv[1]);

  // 무한 서버 루프를 실행
  while (1) {
    // accpet 함수 인자에 넣기위한 주소길이를 변수에 담는다.
    clientlen = sizeof(clientaddr);

    // 반복적으로 연결 요청을 접수
    // Accept 함수는 인자로 듣기 식별자, 소켓 주소 구조체의 주소, 소켓 구조체의 길이를 받는다.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 

    // 소켓 주소 구조체에서 스트링 표시로 변환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    doit(connfd);  // 트랜젝션을 수행
    Close(connfd); // 트랜젝션을 수행된 후 자신의 쪽의 연결 소켓을 닫는다.
  }
}

// doit 함수 -----------------------------------------------------------------------------------------------------
// 한개의 트랜젝션을 처리해주는 함수

void doit(int fd){

  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  // Rio_readlineb 함수를 사용하기 위해 rio_t 구조체 선언
  // Rio = Robust I/O
  rio_t rio;

  Rio_readinitb(&rio, fd); // rio 구조체를 초기화 해준다.
  Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인을 읽고 buf에 담는다.
  printf("Request header :\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s",method, uri, version); //buf를 통해 공백 기준으로 각각의 인자에 담는다.

  // strcasecmp : 대소문자를 무시하고 문자열 비교 함수 / 리턴값 : 두개의값이 같으면 0 리턴
  if (strcasecmp(method, "GET")){ // 요청 메소드가 get이 아니라면
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // 요청이 get이라면 읽어들인다.
  read_requesthdrs(&rio);

  is_static = parse_uri(uri, filename, cgiargs); // 요청이 정적 컨텐츠인지 동적 컨텐츠인지 판단하는 변수

  // 파일이 디스크상에 있지 않는 경우
  if (stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  // 정적 컨텐츠라면
  if (is_static){
    // 이 파일이 보통 파일이면서 읽기 권화을 가지고 있는지 검증
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 검증이 끝나면 정적 컨텐- 츠를 클라이언트에게 제공
    serve_static(fd, filename, sbuf.st_size);
  }
  // 동적 컨텐츠라면
  else{
    // 실행 가능한 파일인지 검증
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 위의 조건을 통과하면 동적 컨텐츠를 클라이언트에게 제공
    serve_dynamic(fd, filename, cgiargs);
  }
}

// Clienterror 함수------------------------------------------------------------------------------------------
// error 발생 시, client에게 보내기 위한 response (error message)
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){

  char buf[MAXLINE], body[MAXBUF];

  // response body 쓰기 (HTML 형식)
  // sprintf를 통해 body배열에 html를 차곡차곡 넣어준다.
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // buf에 에러넘버와 메세지를 담아준다.
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);

  // buf를 fd로 보내고 buf에 새로운 값을 넣는다.
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // Rio_writen으로 쌓아놓은 길쭉한 배열을 fd로 보내준다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// read-requesthdrs 함수 -------------------------------------------------------------------------------------
// request header를 읽기 위한 함수

void read_requesthdrs(rio_t *rp){

  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);

  // strcmp : 두 문자열을 비교하는 함수
  // 빈 텍스트 줄이 아닐 때까지 읽기
  while(strcmp(buf, "\r\n")){

    // rio_readlineb는 \n를 만날때 멈춘다
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// parse_uri 함수 --------------------------------------------------------------------------------------------
// URI를 파일 이름과 옵션으로 CGI 인자 스트링을 분석한다.

int parse_uri(char *uri, char *filename, char *cgiargs){

  char *ptr;
  // parsing 결과, static file request인 경우 (uri에 cgi-bin이 포함이 되어 있지 않으면)
  // strstr은 인자 1에서 인자 2가 있는지 확인
  // strcpy는 인자 1에 인자 2를 복사해서 붙혀넣는다.
  if(!strstr(uri, "cgi-bin")){
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);

    // uri의 끝이 home.html이 아니라면 strcat으로 home.html를 붙혀준다.
    if (uri[strlen(uri)-1] == '/'){
      strcat(filename, "home.html");
    }
    return 1;
  }
  // parsing 결과, dynamic file request인 경우
  else{
    // uri부분에서 '?'의 인덱스 반환
    ptr = index(uri, '?');
    // ?가 있으면
    if (ptr){
      //cgiargs에 인자 넣어주기
      strcpy(cgiargs, ptr+1);
      // 포인터 ptr은 null처리
      *ptr = '\0';
    }else{
      // ?가 없으면
      strcpy(cgiargs, "");
    }
    // filename에 uri담기
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// get_filetype 함수 -------------------------------------------------------------------------------------
// file name에 있는 타입으로 file type를 변환한다.

void get_filetype(char *filename, char *filetype){

  if (strstr(filename, ".html")){
    strcpy(filetype, "text/html");
  }
  else if (strstr(filename, ".gif")){
    strcpy(filetype, "image/gif");
  }
  else if (strstr(filename, ".png")){
    strcpy(filetype, "image/png");
  }
  else if (strstr(filename, ".jpg")){
    strcpy(filetype, "image/jpeg");
  }
  else{
    strcpy(filetype, "text/plain");
  }
}

// serve_static 함수 ----------------------------------------------------------------------------------------

void serve_static(int fd, char *filename, int filesize){

  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // get_filetype을 통해 filetype을 변환
  get_filetype(filename, filetype);

  // 클라이언트에 보낼 자료를 buf에 담는다.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sserver: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  // 클라이언트에 buf를 보내준다.
  Rio_writen(fd, buf, strlen(buf));

  // 서버 쪽에 출력
  printf("Response headers:\n");
  printf("%s", buf);

  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  // srcp = (char*)malloc(sizeof(char) * filesize); // Mmap대신 말록을 사용할수있다.
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  // free(srcp); // 말록을 사용 할 경우 free해주어야한다.
  Munmap(srcp, filesize);
}

// serve_dynamic 함수 ---------------------------------------------------------------------------------

void serve_dynamic(int fd, char *filename, char *cgiargs){

  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // Fork함수로 자식 프로세스 생성 : 현재 부모 프로세스를 그대로 복사한 형태
  if (Fork() == 0){
    // 환경 변수 설정
    setenv("QUERY_STRING", cgiargs, 1);
    // 자식 프로세스의 표준 출력을 연결 파일 식별자로 재지정
    Dup2(fd, STDOUT_FILENO);
    // cgi 프로그램 실행?
    Execve(filename, emptylist, environ);
  }
  // 부모 프로세스가 자식 프로세스가 종료될때까지 대기
  Wait(NULL);
}

