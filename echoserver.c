#include "csapp.h"

void echo(int connfd) {
  size_t n;
  char buf[MAXLINE];
  rio_t rio;
  // 주어진 fd와 파일 read&write를 위한 패키지를 연결하는 과정인 듯
  Rio_readinitb(&rio, connfd);
  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    // 데이터가 읽혔으면
    printf("server received %d bytes\n", (int)n);
    Rio_writen(connfd, buf, n);
  }
};

int main(int argc, char **argv) {
  // listen 받을 file descriptor와 connect fd 정의
  int listenfd, connfd;
  // socklen_t : 주소의 길이. 클라이언트 ip주소의 길이를 말하는듯
  socklen_t clientlen;

  // socket address 구조체를 저장하기 위한 공간인듯
  struct sockaddr_storage clientaddr; /* Enough space for any address */
  // 클라이언트 호스트이름, 포트이름 string
  char client_hostname[MAXLINE], client_port[MAXLINE];

  // 만약 argc가 2가 아니면 exit 처리?
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  /**
  포트를 받아서 listen 상태의 file descriptor 생성
  listen : 클라이언트를 상정한 active 상태의 소켓을, 서버 쪽에서 사용되는
  상태(listen)으로 바꿔주는 함수
  open_listenfd 함수 안에 socket, bind 작업이 포함되어 있음
  */

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(struct sockaddr_storage);
    /*
    accept 처리
    accept : 소켓 설립이 완료되었고, 연결 요청을 받을 준비가 되었다고 알리는
    함수.
    새로운 연결에서 클라이언트와 소통할 파일 기술자를 따로 생성하여 이를 리턴함
    */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    // getnameinfo : 소켓 주소를 통해 호스트 주소와 도메인 이름을 알고 싶을 때
    // 사용
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);
    // 새로 만든 파일 기술자를 echo의 인자로 넘겨서 호출
    echo(connfd);
    // echo가 끝나면 connfd 닫기
    Close(connfd);
  }
  exit(0);
}