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
#include "timer/lst_timer.h"


// 最大文件描述符个数
const int MAX_FD = 65535;
// 最大监听事件数
const int MAX_EVENT_NUM = 10000;
// 最小超时单位
const int TIMESLOT = 5;
// 端口
const int PORT = 9006;


// epoll 的相关文件描述符操作

extern void set_nonblocking(int fd);
extern void add_fd(int epoll_fd, int fd, bool one_shot);
extern void remove_fd(int epoll_fd, int fd);
extern void modify_fd(int epoll_fd, int fd, int ev);

// 定时器初始化 客户端数据
client_data *users_timer = new client_data[MAX_FD];
Utils utils;

// 主要的通信逻辑
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
    if (eno == -1) LOG_ERROR("bind error");

    eno = listen(lfd, 10);
    if (eno == -1) LOG_ERROR("listen error");

    utils.init(TIMESLOT); // init here
    // epoll init
    epoll_event events[MAX_EVENT_NUM];
    int epoll_fd = epoll_create(1000);

    add_fd(epoll_fd, lfd, false);
    http_conn::m_epollfd = epoll_fd;
    // 定时器初始化
    int pipe_fd[2]; // 内核进程间通信

    eno = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe_fd);
    assert(eno != -1);
    set_nonblocking(pipe_fd[1]);
    add_fd(epoll_fd, pipe_fd[0], false);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    // 工具类,信号和描述符基础操作
    Utils::u_pipefd = pipe_fd;
    Utils::u_epollfd = epoll_fd;


    auto init_timer = [&](int connfd, struct sockaddr_in caddr) {
        users[connfd].init(connfd, caddr);

        // 初始化client_data数据
        // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
        users_timer[connfd].addr = caddr;
        users_timer[connfd].sockfd = connfd;
        auto timer = new util_timer;
        timer->user_data = &users_timer[connfd];
        timer->cb_func = cb_func;

        time_t cur = time(NULL);
        timer->expire = cur + 3 * TIMESLOT;
        users_timer[connfd].timer = timer;
        utils.m_timer_lst.add_timer(timer);
    };

    // 若有数据传输，则将定时器往后延迟3个单位
    // 并对新的定时器在链表上的位置进行调整
    auto adjust_timer = [&](util_timer *timer) {
        time_t cur = time(NULL);
        timer->expire = cur + 3 * TIMESLOT;
        utils.m_timer_lst.adjust_timer(timer);

        // LOG_INFO("%s", "adjust timer once");
    };

    auto del_timer = [&](util_timer *timer, int sockfd) {
        timer->cb_func(&users_timer[sockfd]);
        if (timer) {
            utils.m_timer_lst.del_timer(timer);
        }

        // LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
    };
    auto dealwithsignal = [&](bool &timeout, bool &stop_server) {
        int ret = 0;
        int sig;
        char signals[1024];
        ret = recv(pipe_fd[0], signals, sizeof(signals), 0);
        if (ret == -1) {
            return false;
        } else if (ret == 0) {
            return false;
        } else {
            for (int i = 0; i < ret; ++i) {
                switch (signals[i]) {
                    case SIGALRM: {
                        timeout = true;
                        break;
                    }
                    case SIGTERM: {
                        stop_server = true;
                        break;
                    }
                }
            }
        }
        return true;
    };

    // 进入事件循环
    while (1) {
        bool timeout{}, is_stop{};
        int num = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, -1);
        if (num < 0 && errno != EINTR) LOG_ERROR("epoll error");


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
                // 加入定时器
                init_timer(cfd, caddr);
                // 新的客户数据初始化, 放入数组
                users[cfd].init(cfd, caddr); // 初始化 http_conn 对象
            } else if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                //  Error, 关闭连接
                // printf("关闭连接...\n");
                del_timer(users_timer[sockfd].timer, sockfd); // 删除定时器
                users[sockfd].close_conn();
            } else if ((sockfd == pipe_fd[0]) && (events[i].events & EPOLLIN)) {
                bool flag = dealwithsignal(timeout, is_stop);
                if (false == flag) LOG_ERROR("%s", "dealclientdata failure");
            } else if (events[i].events & EPOLLIN) { // 收客户端数据
                // 读事件发生, 一次性读取全部数据(主线程)
                // printf("reading...\n");
                auto timer = users_timer[sockfd].timer;
                if (users[sockfd].read()) {
                    // pool->append(&users[sockfd]);
                    pool->append(users + sockfd);
                    if (timer) adjust_timer(timer);
                } else {
                    del_timer(timer, sockfd);
                    users[sockfd].close_conn();
                }
            } else if (events[i].events & EPOLLOUT) { // 写事件
                // printf("writing... \n");
                auto timer = users_timer[sockfd].timer;
                // 发回客户端数据
                if (users[sockfd].write()) { // 一次性写完全部数据
                    if (timer) adjust_timer(timer);
                } else { // 删除定时器, 关闭连接
                    del_timer(timer, sockfd);
                    users[sockfd].close_conn();
                }
            }
        }
        if (timeout) { // 处理超时事件
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
    close(epoll_fd);
    close(lfd);
    close(pipe_fd[1]);
    close(pipe_fd[0]);
    delete[] users;
    delete[] users_timer;
    delete pool;
}

void addsig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler; // 信号触发后执行的函数
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

int main(int argc, char *argv[]) {
    //
    // 信号处理: 忽略 EPIPE
    addsig(SIGPIPE, SIG_IGN); // 终止进程
    Log::get_instance()->init("./server.log", 2000, 800000, 0);
    net_communication();

    return 0;
}
