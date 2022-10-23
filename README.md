## Tiny Http Server
一个简单的 http 服务器，实现了最基本的功能。

## 一、项目主要文件
1. httpd.c : 主程序
2. Makefile : 编译文件
3. htdocs/index.html : 服务器 index
4. htdocs/color.cgi : CGI 脚本处理用户请求

## 二、项目需要修改的地方
1. make 的时候出现错误
```cpp
/usr/bin/ld: cannot find -lsocket
```
解决方法：
删除 Makefile 中的 -lsocket 即可。
2. make 的时候出现错误
```cpp
httpd.c:(.text+0x1a2f): undefined reference to `pthread_create'
```
这个错误的原因是 pthread 库不是 Linux 系统默认的库，连接时需要使用静态库 libpthread.a．解决方案就是在编译中加入 -lpthread参数．
如果加入还有错误，将 -lpthread 参数放置到最后，如下所是：
```cpp
gcc -g -W -Wall　-o httpd 　httpd.c　-lpthread
```
3. 启动程序后，输入对应颜色后，无法显示颜色，则修改如下：
* color.cgi脚本没有执行权限，通过chmod +x color.cgi命令，给该文件添加执行权限；
* index.html没有写的权限，通过chmod 600 index.html设置权限；
* perl安装的路径不对，perl默认的安装路径是/usr/bin/perl，将color.cgi文件第一行的#!/usr/local/bin/perl -Tw修改为#!/usr/bin/perl -Tw;
4. 如果执行过程中，服务端报错
```cpp
Can't locate CGI.pm in @INC (you may need to install the CGI module) (@INC contains: /etc/perl /usr/local/lib/x86_64-linux-gnu/perl/5.30.0 /usr/local/share/perl/5.30.0 /usr/lib/x86_64-linux-gnu/perl5/5.30 /usr/share/perl5 /usr/lib/x86_64-linux-gnu/perl/5.30 /usr/share/perl/5.30 /usr/local/lib/site_perl /usr/lib/x86_64-linux-gnu/perl-base) at htdocs/color.cgi line 4.
BEGIN failed--compilation aborted at htdocs/color.cgi line 4.
```
解决方法：
```cpp
sudo apt install libcgi-ajax-perl
sudo apt install libcgi-application-perl
```

## 三、编译 && 运行

```cpp
make
./httpd
```

## 四、项目源码解析

### 4.1 主要函数

* accept_request:  处理从套接字上监听到的一个 HTTP 请求，在这里可以很大一部分地体现服务器处理请求流程。
* bad_request: 返回给客户端这是个错误请求，HTTP 状态吗 400 BAD REQUEST.
* cat: 读取服务器上某个文件写到 socket 套接字。
* cannot_execute: 主要处理发生在执行 cgi 程序时出现的错误。
* error_die: 把错误信息写到 perror 并退出。
* execute_cgi: 运行 cgi 程序的处理，也是个主要函数。
* get_line: 读取套接字的一行，把回车换行等情况都统一为换行符结束。
* headers: 把 HTTP 响应的头部写到套接字。
* not_found: 主要处理找不到请求的文件时的情况。
* sever_file: 调用 cat 把服务器文件返回给浏览器。
* startup: 初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。
* unimplemented: 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。

### 4.2 主要流程

1. 服务器启动，在指定端口或随机选取端口绑定 httpd 服务。
2. 收到一个 HTTP 请求时（其实就是 listen 的端口 accpet 的时候），派生一个线程运行 accept_request 函数。
3. 取出 HTTP 请求中的 method (GET 或 POST) 和 url,。对于 GET 方法，如果有携带参数，则 query_string 指针指向 url 中 ？ 后面的 GET 参数。
4. 格式化 url 到 path 数组，表示浏览器请求的服务器文件路径，在 tinyhttpd 中服务器文件是在 htdocs 文件夹下。当 url 以 / 结尾，或 url 是个目录，则默认在 path 中加上 index.html，表示访问主页。
5. 如果文件路径合法，对于无参数的 GET 请求，直接输出服务器文件到浏览器，即用 HTTP 格式写到套接字上，跳到（10）。其他情况（带参数 GET，POST 方式，url 为可执行文件），则调用 excute_cgi 函数执行 cgi 脚本。
6. 读取整个 HTTP 请求并丢弃，如果是 POST 则找出 Content-Length. 把 HTTP 200  状态码写到套接字。
7. 建立两个管道，cgi_input 和 cgi_output, 并 fork 一个进程。
8. 在子进程中，把 STDOUT 重定向到 cgi_outputt 的写入端，把 STDIN 重定向到 cgi_input 的读取端，关闭 cgi_input 的写入端 和 cgi_output 的读取端，设置 request_method 的环境变量，GET 的话设置 query_string 的环境变量，POST 的话设置 content_length 的环境变量，这些环境变量都是为了给 cgi 脚本调用，接着用 execl 运行 cgi 程序。
9. 在父进程中，关闭 cgi_input 的读取端 和 cgi_output 的写入端，如果 POST 的话，把 POST 数据写入 cgi_input，已被重定向到 STDIN，读取 cgi_output 的管道输出到客户端，该管道输入是 STDOUT。接着关闭所有管道，等待子进程结束。这一部分比较乱，
10. 关闭与浏览器的连接，完成了一次 HTTP 请求与回应，因为 HTTP 是无连接的。

### 4.3 工作流程图

[工作流程图](./htdocs/process.png)


### 4.3 阅读代码顺序
main()——>startup()——>accept_request()——>serve_file()——>execute_cig()。

### 4.4 获取源码
http://sourceforge.net/projects/tinyhttpd


### 4.5 参考连接：
1. https://blog.csdn.net/GQB1226/article/details/46844887
2. https://blog.csdn.net/weixin_45808445/article/details/117161707
3. https://blog.csdn.net/zs120197/article/details/114004533


## 五、原 README 文件内容
  This software is copyright 1999 by J. David Blackstone.  Permission
is granted to redistribute and modify this software under the terms of
the GNU General Public License, available at http://www.gnu.org/ .

  If you use this software or examine the code, I would appreciate
knowing and would be overjoyed to hear about it at
jdavidb@sourceforge.net .

  This software is not production quality.  It comes with no warranty
of any kind, not even an implied warranty of fitness for a particular
purpose.  I am not responsible for the damage that will likely result
if you use this software on your computer system.

  I wrote this webserver for an assignment in my networking class in
1999.  We were told that at a bare minimum the server had to serve
pages, and told that we would get extra credit for doing "extras."
Perl had introduced me to a whole lot of UNIX functionality (I learned
sockets and fork from Perl!), and O'Reilly's lion book on UNIX system
calls plus O'Reilly's books on CGI and writing web clients in Perl got
me thinking and I realized I could make my webserver support CGI with
little trouble.

  Now, if you're a member of the Apache core group, you might not be
impressed.  But my professor was blown over.  Try the color.cgi sample
script and type in "chartreuse."  Made me seem smarter than I am, at
any rate. :)

  Apache it's not.  But I do hope that this program is a good
educational tool for those interested in http/socket programming, as
well as UNIX system calls.  (There's some textbook uses of pipes,
environment variables, forks, and so on.)

  One last thing: if you look at my webserver or (are you out of
mind?!?) use it, I would just be overjoyed to hear about it.  Please
email me.  I probably won't really be releasing major updates, but if
I help you learn something, I'd love to know!

  Happy hacking!

                                   J. David Blackstone
