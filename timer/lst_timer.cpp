#include "lst_timer.h"
#include "../http_conn/http_conn.h"

sort_timer_lst::~sort_timer_lst() { // 双向链表的析构操作
    util_timer* tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer) { // 将目标定时器添加到链表中
    if (!timer) return;
    if (!head) {
        head = tail = timer;
        return;
    }
    // 目标定时器超时时间小于当前链表中所有定时器的超时时间,
    // 则把该定时器插入链表头部 否则重载 add_timer(util_timer*, util_timer*)
    // 把它插入链表中合适位置, 以保证升序

    if (timer->expire < head->expire) { // 直接头插
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    // 可能处在任意位置, 插入头结点为 head 的升序链表
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer* timer) {
    // 调整, 考虑时间延长情况, 后移当前节点
    if (!timer) return;
    auto tmp = timer->next;
    if (!tmp || timer->expire < tmp->expire) return;
    // 若为头结点, 取出->重新插入
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    } else { // 先取出, 然后插入原位置之后的链表中
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer* timer) {
    if (!timer) return;
    if (timer == head && tail == timer) {
        // 仅有一个定时器
        delete timer;
        head = tail = NULL;
        return;
    }
    // 至少两个定时器
    if (timer == head) { // 头
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail) { // 尾
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }

    // 中间位置
    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    delete timer;
}


void sort_timer_lst::tick() { // 心搏函数
    // 处理链表上到期的任务
    if (!head) return;
    printf("time tick\n");
    time_t cur = time(NULL); // 使用绝对时间, 直接和系统时间比较
    auto tmp = head;
    // 从头结点开始依次处理每个定时器(结点)
    // 直到遇到一个尚未到期的定时器
    while (tmp) {
        if (cur < tmp->expire) break; // 还未到超时时间
        // callback 执行定时任务
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head) head->prev = NULL;
        delete tmp; // 删除完成的定时器
        tmp = head;
    }
}

// 私有方法:
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head) {
    auto pre = lst_head;
    auto cur = pre->next;
    // 遍历 lst_head 之后的部分链表,
    // 直到找到一个超时时间大于目标定时器超时时间的节点,
    // 并将目标定时器插入该节点之前
    while (cur) {
        if (timer->expire < cur->expire) { // 插入 pre之后, cur 之前
            pre->next = timer;
            timer->next = cur;
            timer->prev = pre;
            cur->prev = timer;
        }
        pre = cur;
        cur = cur->next;
    }
    // 此时仍未找到, 尾插
    if (!cur) {
        pre->next = timer;
        timer->prev = pre;
        timer->next = NULL;
        tail = timer;
    }
}


// 工具类

void Utils::init(int timeslot) { m_TIMESLOT = timeslot; }

// 信号处理函数
void Utils::sig_handler(int sig) {
    // 为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler() {
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char* info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data* user_data) {
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
