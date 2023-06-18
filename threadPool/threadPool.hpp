#ifndef THREADPOOL
#define THREADPOOL
#include <exception>
#include <cstdio>
#include "../log/log.h"
#include "../locker/locker.hpp"
#include "../base/queue.h"
// #include "../base/threadsafe_queue.h"

template <typename T>
class ThreadPool {
public:
    ThreadPool(int num = 8, int max_req = 100000);
    ~ThreadPool();

    bool append(T* req);
    void run();

private: //
    int m_thread_num;
    // array to store threads
    pthread_t* m_threads;
    // Maximum number of requests waiting to be processed
    int m_max_requests;
    // task queue
    queue<T*> m_workqueue;
    // lock
    locker m_queuelocker;
    // sem, check task process or not
    sem m_queuestat;
    // check all threads over
    bool is_stop;

private:
    // woker sub-thread
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
    if (num <= 0 || max_req <= 0)
        throw std::exception();

    // init
    m_threads = new pthread_t[m_thread_num];
    if (!m_threads) // TODO: new handler
        throw std::exception();

    for (int i{}; i < num; ++i) {
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

    m_workqueue.push(req); // http_conn*
    m_queuelocker.unlock();
    m_queuestat.post(); // +1, finish
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
        m_queuestat.wait(); // get, -1
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        // get data from queue front
        T* req = *m_workqueue.pop().get();
        m_queuelocker.unlock();

        if (!req) { // empty, get continually
            LOG_INFO("http req empty\n");
            continue;
        }

        // main task
        req->process();
    }
}
#endif // !THREADPOOL
