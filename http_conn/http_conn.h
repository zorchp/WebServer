#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/uio.h> //writev
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "http_status.h"
#include "../log/log.h"


class http_conn {
public:
    static int m_epollfd; // 所有 socket 事件都被注册到同一个 epoll 对象上
    static int m_user_count; // 用户数量

    static const int READ_BUF_SIZE = 1024;  // 读缓冲区大小
    static const int WRITE_BUF_SIZE = 1024; // 写缓冲区大小
    static const int FILENAME_LEN = 256;    // 文件名长度支持

    // http
    http_conn() {}
    ~http_conn() {}
    void init(int sockfd, const sockaddr_in& addr); // 初始化新接收的连接
    void process();    // 处理客户端请求(以及响应)
    void close_conn(); // 关闭连接

    // 读写, 非阻塞
    bool read();
    bool write();

private:
    // http 请求处理
    void status_init();                       // 初始化下面的状态
    HTTP_CODE process_read();                 // 解析 HTTP 请求
    HTTP_CODE parse_request_line(char* text); // 解析 HTTP 请求行
    HTTP_CODE parse_headers(char* text);      // 解析 HTTP 请求头
    HTTP_CODE parse_content(char* text);      // 解析 HTTP 请求体
    LINE_STATUS parse_line(); // 解析一行请求, 从状态机, 判断依据 '\r\n'
    // 这并不是独立 的一行(是所有数据的起始指针)
    char* get_line() { return m_read_buf + m_start_line; } // 获取一行
    HTTP_CODE do_request();                                // 具体的处理
    // 写入(发回客户端)
    bool process_write(HTTP_CODE); // 解析 HTTP 请求
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

private:
    int m_sockfd;                     // HTTP 连接的 socket
    sockaddr_in m_addr;               // 通信的地址
    char m_read_buf[READ_BUF_SIZE];   // read buf
    char m_write_buf[WRITE_BUF_SIZE]; // write buf
    int m_write_idx;                  // 写缓冲区待发送的字节数
    int m_read_idx; // 标识读缓冲区以及读入的数据的下一字节位置,
                    // 其实就是当前行的行尾

    // http parse
    int m_checked_idx; // 正在分析的字符在读缓冲区的位置
    int m_start_line;  // 正在解析的行的起始位置
    // request line
    char* m_url;
    char* m_http_version;
    METHOD m_method;
    char* m_hostname;
    bool is_linger;    // 长连接
    int m_content_len; // 消息体大小,(如果有则不为0)

    // 文件资源相关
    char m_real_file[FILENAME_LEN];
    char* m_file_addr; // 客户请求的目标文件被 mmap 到内存中的起始位置
    struct stat m_file_stat; // 目标文件状态

    // 分散写入
    struct iovec m_iv[2];
    int m_iv_cnt;

    CHECK_STATE m_check_state; // 主状态机当前所处的状态
    int bytes_to_send;         // 将要发送的字节数
    int bytes_sent;            // 已发送字节数

    // // timer
    // int timer_flg;
};

#endif // !HTTPCONN_H
