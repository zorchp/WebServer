#ifndef LST_TIMER_H
#define LST_TIMER_H
#include <ctime>
#include <cstdio>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/epoll.h>

#define BUF_SIZE 1024

class util_timer;

struct client_data {
    struct sockaddr_in addr;
    int sockfd;
    char buf[BUF_SIZE];
    util_timer *timer;
};

class util_timer {
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;                  // 任务超时时间, 绝对时间
    void (*cb_func)(client_data *); // 任务回调函数
    client_data *user_data;         // 回调函数处理的客户数据,
                            // 通过指针(定时器的执行者)传递给回调函数
    util_timer *prev, *next;
};

class sort_timer_lst { // 升序链表
public:
    sort_timer_lst() : head(NULL), tail(NULL) {}
    ~sort_timer_lst();
    void add_timer(util_timer *);
    void adjust_timer(util_timer *);
    void del_timer(util_timer *);
    void tick(); // 心搏函数, 每隔一段时间运行一次, 用于检测并处理到期的任务

private:
    void add_timer(util_timer *, util_timer *);

private:
    util_timer *head, *tail;
};


class Utils {
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data); // 回调函数

#endif // !LST_TIMER_H
