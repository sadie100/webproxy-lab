#include "csapp.h"

int main(int argc, char **argv) {
  // 클라이언트 fd의 시작주소가 될 변수
  int clientfd;
  // 호스트, 포트, buf
  char *host, *port, buf[MAXLINE];
  // rio 상태값
  rio_t rio;
  // 이건 뭔지 모르겠다.
  if (argc != 3) {
    fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
    exit(0);
  }
  // 두 번째 인자를 host로
  host = argv[1];
  // 첫 번째 인자를 port로
  port = argv[2];

  /*
  호스트주소, 포트번호로 클라이언트 fd를 연다
  open_clientfd함수의 내부에 socket과 connect가 들어가 있음
  */
  clientfd = Open_clientfd(host, port);
  // fd와 rio 연결
  Rio_readinitb(&rio, clientfd);

  // fgets() : 현재 stream 위치에서 문자를 읽는 빌트인 함수
  while (Fgets(buf, MAXLINE, stdin) != NULL) {
    // 들어온 문자를 fd에 쓰기
    Rio_writen(clientfd, buf, strlen(buf));
    Rio_readlineb(&rio, buf, MAXLINE);
    // fputs : stream에 문자 쓰기
    Fputs(buf, stdout);
  }
  // close 처리
  Close(clientfd);
  exit(0);
}