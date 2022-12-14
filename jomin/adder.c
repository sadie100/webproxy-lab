/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {

  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // getenv : varname에 해당한 항목에 대한 환경 변수의 리스트를 검색/ 실패시 NULL리턴
  if ((buf = getenv("QUERY_STRING")) != NULL){
    // strchr : 인자1에서 인자2가 처음 나오는 부분 부터 마지막 부분까지 문자열 리턴, 찾지 못할시 NULL
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);
    // atoi : 문자 스트링을 정수값으로 변환
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }
  // content배열에 html body를 담는다.
  sprintf(content, "QUERY_STRING = %s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2 , n1+n2);
  sprintf(content, "%sThanks for visiting:)\r\n", content);

  // 헤더 출력
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");

  // html body출력
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
