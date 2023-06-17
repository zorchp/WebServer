#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>

class locker {
public:
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }
    ~locker() {
        pthread_mutex_destroy(&m_mutex); // destroy
    }
    bool lock() { return 0 == pthread_mutex_lock(&m_mutex); }
    bool unlock() { return 0 == pthread_mutex_unlock(&m_mutex); }
    pthread_mutex_t* get() { return &m_mutex; }

private:
    pthread_mutex_t m_mutex;
};


// RAII
class lock_guard {
    locker mtx;

public:
    lock_guard(locker mtx_) : mtx(mtx_) { mtx.lock(); }
    ~lock_guard() { mtx.unlock(); }
};

// 条件变量
class cond {
public:
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }

    ~cond() {
        pthread_cond_destroy(&m_cond); // destroy
    }

    bool wait(pthread_mutex_t* mutex) {
        return 0 == pthread_cond_wait(&m_cond, mutex);
    }

    bool timedwait(pthread_mutex_t* mutex, struct timespec t) {
        return 0 == pthread_cond_timedwait(&m_cond, mutex, &t);
    }

    bool signal() { return 0 == pthread_cond_signal(&m_cond); }

    bool broadcast() { return 0 == pthread_cond_broadcast(&m_cond); }

private:
    pthread_cond_t m_cond;
};

// 信号量
class sem {
public:
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) // 默认条件变量
            throw std::exception();
    }
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) //
            throw std::exception();
    }

    ~sem() { sem_destroy(&m_sem); }

    //-
    bool wait() { return 0 == sem_wait(&m_sem); }
    //+
    bool post() { return 0 == sem_post(&m_sem); }

private:
    sem_t m_sem;
};

#endif // !LOCKER_H
