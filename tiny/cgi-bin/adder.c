/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  /*
  buf는 환경변수 추출해서 담는 변수
  p는 buf 스트링에서 인자값 구분 문자(&)의 위치를 담을 포인터 변수
  */
  char *buf, *p;
  /*
   arg1와 arg2는 인자의 숫자가 들어갈 변수, content는 response body에 들어갈
   html 코드를 담을 변수
  */
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // 2개의 인자 추출
  // 쿼리 스트링에서 환경변수 추출해서 buf에 넣기
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    // strchr : buf 스트링에서 문자(&)의 첫 번째 표시에 대한 포인터 리턴
    p = strchr(buf, '&');
    // \0은 문자열의 끝을 의미하는 문자(널 문자)
    *p = '\0';
    // strcpy(dest, source) : source값을 dest로 복사
    // \0이 나오기 전까지의 string을 arg1에 복사
    strcpy(arg1, buf + 5);
    // \0 이후부터의 문자열을 arg2에 복사
    strcpy(arg2, p + 6);
    // atoi : string을 int로 형변환
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  // response body 만들기
  // sprintf(s, format string) : s에 formatted된 string을 담기
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2,
          n1 + n2);
  sprintf(content, "%sThanks for visiting! \r\n", content);

  // http 응답 생성
  // cgi 프로그램이 출력하는 모든 것은 클라이언트로 직접 전송됨
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  // fflush : 스트림을 비우는 함수.
  fflush(stdout);

  exit(0);
}
/* $end adder */
