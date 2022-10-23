/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
int get_line(int, char *, int);
void unimplemented(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
//处理从套接字上监听到的一个 HTTP 请求。
/**
 * http 请求格式如下所是：
 * 请求方法[空格]url[空格]协议/版本[回车符换行符]
 * 
 * @param client 
 */
void accept_request(int client)
{
  char buf[1024];
  int numchars;
  char method[255];
  char url[255];
  char path[512];
  size_t i = 0, j = 0;
  struct stat st;
  int cgi = 0; /* becomes true if server decides this is a CGI
                * program */
  char *query_string = NULL;

  //1. 获得请求的第一行，请求的第一行往往是 ： GET / HTTP/1.1
  numchars = get_line(client, buf, sizeof(buf));

  printf("buf = %s\n", buf);
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';

  //2. 分析请求
  //server 支持 get 和 post 两种方法，如果是其他的方法，就不支持了，返回状态码501，服务器不支持这个方法
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
  {
    unimplemented(client);
    return;
  }

  //对于是post的请求，把cgi(common gateway interface)的flag 设为1，表示这个需要cgi来处理
  if (strcasecmp(method, "POST") == 0)
    cgi = 1;

  //获得请求的url
  i = 0;
  while (ISspace(buf[j]) && (j < sizeof(buf)))
    j++;
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
  {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';

  //如果是get方法，判断这个get请求，是否是带有参数的请求
  if (strcasecmp(method, "GET") == 0)
  {
    query_string = url;
    while ((*query_string != '?') && (*query_string != '\0'))
      query_string++;
    if (*query_string == '?')
    {
      cgi = 1;
      *query_string = '\0';
      query_string++;
    }
  }

  sprintf(path, "htdocs%s", url);
  if (path[strlen(path) - 1] == '/') // 获取 index.html
    strcat(path, "index.html");
  if (stat(path, &st) == -1)
  {
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
    //路径不对，没有发现
    not_found(client);
  }
  else
  {
    if ((st.st_mode & S_IFMT) == S_IFDIR)
      strcat(path, "/index.html");
    if ((st.st_mode & S_IXUSR) ||
        (st.st_mode & S_IXGRP) ||
        (st.st_mode & S_IXOTH))
      cgi = 1;
    if (!cgi)
      serve_file(client, path);
    else
      execute_cgi(client, path, method, query_string);
  }

  close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
// 读取服务器上客户端想要的文件写到 socket 套接字
void cat(int client, FILE *resource)
{
  char buf[1024];

  fgets(buf, sizeof(buf), resource);
  while (!feof(resource)) // 判断是否读取完毕
  {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
  perror(sc);
  exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
//运行 CGI 程序
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
  char buf[1024];
  int cgi_output[2];
  int cgi_input[2];
  pid_t pid;
  int status;
  int i;
  char c;
  int numchars = 1;
  int content_length = -1;

  buf[0] = 'A';
  buf[1] = '\0';
  if (strcasecmp(method, "GET") == 0)
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
  else /* POST */
  {
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf))
    {
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16]));
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1)
    {
      bad_request(client);
      return;
    }
  }

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);

  //创建管道 cgi_output
  if (pipe(cgi_output) < 0)
  {
    cannot_execute(client);
    return;
  }

  //创建管道 cgi_input
  if (pipe(cgi_input) < 0)
  {
    cannot_execute(client);
    return;
  }

  //产生进程
  if ((pid = fork()) < 0)
  {
    cannot_execute(client);
    return;
  }
  if (pid == 0) // child: CGI script
  {
    char meth_env[255];
    char query_env[255];
    char length_env[255];

    dup2(cgi_output[1], 1); // 标准输出
    dup2(cgi_input[0], 0);  // 标准输入

    close(cgi_output[0]);
    close(cgi_input[1]);

    sprintf(meth_env, "REQUEST_METHOD=%s", method);
    putenv(meth_env); // 添加环境变量 REQUEST_METHOD=method
    if (strcasecmp(method, "GET") == 0)
    {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env); // 添加环境变量 QUERY_STRING=query_string
    }
    else
    { // POST 可能会有消息体
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env); // 添加环境变量 CONTENT_LENGTH=content_length
    }
    printf("path = %s\n", path);
    execl(path, path, NULL);
    exit(0);
  }
  else
  { /* parent */
    close(cgi_output[1]);
    close(cgi_input[0]);
    if (strcasecmp(method, "POST") == 0)
      for (i = 0; i < content_length; i++) // 读取消息体
      {
        recv(client, &c, 1, 0);
        write(cgi_input[1], &c, 1); // 传递给子进程
      }
    while (read(cgi_output[0], &c, 1) > 0) // 读取子进程的输出信息
      send(client, &c, 1, 0);

    close(cgi_output[0]);
    close(cgi_input[1]);
    waitpid(pid, &status, 0);
  }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
// 读取套接字的一行，把回车换行等情况都统一为换行符结束。
int get_line(int sock, char *buf, int size)
{
  int i = 0;
  char c = '\0';
  int n;

  // size - 1 留出一个字节是为了最后添加 '\0' 结束符
  while ((i < size - 1) && (c != '\n'))
  {
    n = recv(sock, &c, 1, 0);
    /* DEBUG printf("%02X\n", c); */
    if (n > 0)
    {
      if (c == '\r') //把回车换行等情况都统一为换行符结束
      {
        n = recv(sock, &c, 1, MSG_PEEK);
        /* DEBUG printf("%02X\n", c); */
        if ((n > 0) && (c == '\n'))
          recv(sock, &c, 1, 0);
        else
          c = '\n';
      }
      buf[i] = c;
      i++;
    }
    else
      c = '\n';
  }
  buf[i] = '\0';

  return (i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
  char buf[1024];
  (void)filename; /* could use filename to determine file type */

  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);

  /**
   * HTTP/1.0 200 OK
   * Server: jdbhttpd/0.1.0\r\n
   * Content-Type: text/html\r\n
   * \r\n
   */
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
// 向客户端发送常规文件。 使用标头，并在发生错误时向客户端报告。调用 cat 把服务器文件返回给浏览器。
// filename 表示客户想要的服务器文件路径
void serve_file(int client, const char *filename)
{
  FILE *resource = NULL;
  int numchars = 1;
  char buf[1024];

  buf[0] = 'A';
  buf[1] = '\0';
  while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    numchars = get_line(client, buf, sizeof(buf));

  // 读取内容
  resource = fopen(filename, "r");
  if (resource == NULL) // 服务器上没有发现内容
    not_found(client);
  else
  {
    headers(client, filename);
    cat(client, resource);
  }

  // 关闭文件描述符
  fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
// 初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。
int startup(u_short *port)
{
  int httpd = 0;
  struct sockaddr_in name;

  httpd = socket(PF_INET, SOCK_STREAM, 0);
  if (httpd == -1)
    error_die("socket");
  memset(&name, 0, sizeof(name));
  name.sin_family = AF_INET;
  name.sin_port = htons(*port);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    error_die("bind");
  if (*port == 0) //端口为 0，则动态分配一个
  {
    int namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
      error_die("getsockname");
    *port = ntohs(name.sin_port);
  }

  //开始在指定端口上监听连接，参数 5 表示连接请求队列的最大长度为5
  if (listen(httpd, 5) < 0)
    error_die("listen");
  return (httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
//返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。
void unimplemented(int client)
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
  /***
   * HTTP/1.0 501 Method Not Implemented\r\n
   * Server: jdbhttpd/0.1.0\r\n
   * Content-Type: text/html\r\n
   * \r\n     // 表示下面是消息体 BODY
   * <HTML><HEAD><TITLE>Method Not Implemented\r\n
   * </TITLE></HEAD>\r\n
   * <BODY><P>HTTP request method not supported.\r\n
   * </BODY></HTML>\r\n
   */
}

/**********************************************************************/

int main(void)
{
  int server_sock = -1;
  u_short port = 0;
  int client_sock = -1;
  struct sockaddr_in client_name;
  int client_name_len = sizeof(client_name);
  pthread_t newthread;

  server_sock = startup(&port);
  printf("httpd running on port %d\n", port);

  while (1)
  {
    client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
    printf("client\n");
    if (client_sock == -1) {
      printf("JIN");
      error_die("accept");
    }
      
    //每个请求都分配一个线程去处理，头文件 pthread.h
    //accept_request(client_sock);
    if (pthread_create(&newthread, NULL, accept_request, client_sock) != 0)
      perror("pthread_create");
  }

  close(server_sock);

  return (0);
}
