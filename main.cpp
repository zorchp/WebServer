#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h> // nonblocking
#include <netinet/in.h>
#include <sys/epoll.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include "threadPool/threadPool.hpp"
#include "http_conn/http_conn.h"


// 最大文件描述符个数
#define MAX_FD 65535
// 最大监听事件数
#define MAX_EVENT_NUM 10000

const int PORT = 9006;

void addsig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler; // 信号触发后执行的函数
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}


// epoll 的相关文件描述符操作
extern void add_fd(int epoll_fd, int fd, bool one_shot);
extern void remove_fd(int epoll_fd, int fd);
extern void modify_fd(int epoll_fd, int fd, int ev);


void net_communication() {
    // 创建线程池:
    ThreadPool<http_conn> *pool{};

    try {
        pool = new ThreadPool<http_conn>;
    } catch (...) {
        exit(-1);
    }

    // 保存客户端信息
    http_conn *users = new http_conn[MAX_FD];
    int lfd = socket(AF_INET, SOCK_STREAM, 0);

    // port reuse
    int reuse{1};
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // bind
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    addr.sin_port = htons(PORT);
    int eno = bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    if (eno == -1) ERR(bind);

    eno = listen(lfd, 10);
    if (eno == -1) ERR(listen);

    // epoll init
    epoll_event events[MAX_EVENT_NUM];
    int epoll_fd = epoll_create(1000);

    add_fd(epoll_fd, lfd, false);
    http_conn::m_epollfd = epoll_fd;
    while (1) {
        int num = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, -1);
        if (num < 0 && errno != EINTR) ERR(epoll);


        // 遍历事件
        for (int i{}; i < num; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == lfd) {
                // 获取客户端连接
                struct sockaddr_in caddr;
                socklen_t clen = sizeof(caddr);
                int cfd = accept(lfd, (struct sockaddr *)&caddr, &clen);

                if (http_conn::m_user_count >= MAX_FD) {
                    // 连接满, 内部正忙
                    printf("connect full\n");
                    close(cfd);
                    continue;
                }
                // 新的客户数据初始化, 放入数组
                users[cfd].init(cfd, caddr);
            } else if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                //  Error, 关闭连接
                printf("关闭连接...\n");
                users[sockfd].close_conn();
            } else if (events[i].events & EPOLLIN) { // 收客户端数据
                // 读事件发生, 一次性读取全部数据(主线程)
                printf("reading...\n");
                if (users[sockfd].read()) {
                    pool->append(&users[sockfd]);
                } else {
                    users[sockfd].close_conn();
                }
            } else if (events[i].events & EPOLLOUT) { // 写事件
                printf("写事件write \n");
                // 发回客户端数据
                if (!users[sockfd].write()) // 一次性写完全部数据
                    users[sockfd].close_conn();
            }
        }
    }
    close(epoll_fd);
    close(lfd);
    delete[] users;
    delete pool;
}

int main(int argc, char *argv[]) {
    //
    // 信号处理: PIPE
    addsig(SIGPIPE, SIG_IGN); // 终止进程

    net_communication();

    return 0;
}
