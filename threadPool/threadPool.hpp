#ifndef THREADPOOL
#define THREADPOOL
#include <exception>
#include <pthread.h>
#include <list> // 工作队列
#include <cstdio>
#include <semaphore.h>
#include "../locker/locker.hpp"

template <typename T>
class ThreadPool {
public:
    ThreadPool(int num = 8, int max_req = 10000);
    ~ThreadPool();

    bool append(T* req);
    void run();

private: //
    // 线程数量
    int m_thread_num;
    // 线程池数组
    pthread_t* m_threads;
    // 最大允许等待处理的请求数
    int m_max_requests;
    // 请求队列: 任务
    std::list<T*> m_workqueue;
    // 锁
    locker m_queuelocker;
    // 信号量: 判断是否有任务需要处理
    sem m_queuestat;
    // 是否结束线程
    bool is_stop;

private:
    // 静态 工作函数: 子线程
    static void* worker(void*);
};

template <typename T>
ThreadPool<T>::ThreadPool(int num, int max_req)
    : m_thread_num(num),
      m_threads(NULL),
      m_max_requests(max_req),
      m_workqueue(),
      m_queuelocker(),
      m_queuestat(),
      is_stop(false) {
    if (num <= 0 || max_req <= 0) throw std::exception();

    // 创建
    m_threads = new pthread_t[m_thread_num];
    if (!m_threads) // TODO: new 异常处理
        throw std::exception();

    for (int i{}; i < num; ++i) {
        printf("creating %d th thread\n", i);
        // worker is static func, using this
        int eno = pthread_create(m_threads + i, NULL, worker, this);
        if (0 != eno) {
            delete[] m_threads;
            throw std::exception();
        }

        eno = pthread_detach(m_threads[i]);
        if (0 != eno) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
    delete[] m_threads;
    is_stop = true;
}

template <typename T>
bool ThreadPool<T>::append(T* req) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(req);
    m_queuelocker.unlock();
    m_queuestat.post(); // +1, 表示执行结束
    return true;
}

template <typename T>
void* ThreadPool<T>::worker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    pool->run();
    return NULL;
}


template <typename T>
void ThreadPool<T>::run() {
    while (!is_stop) {
        m_queuestat.wait(); // 获取, -1
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        // 有数据, 获取队头任务
        T* req = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!req) { // 空, 继续获取
            continue;
        }

        // 执行任务
        req->process();
    }
}
#endif // !THREADPOOL
