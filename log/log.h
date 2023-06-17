#ifndef LOG_H
#define LOG_H

#include <pthread.h>
#include <stdarg.h>
#include "block_queue.hpp"
#include <cstdlib>
#include <cstring>

class Log {
public:
    // C++11以后,使用局部变量懒汉不用加锁
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args) {
        Log::get_instance()->async_write_log();
        return NULL;
    }
    // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int log_buf_size = 8192,
              int split_lines = 5000000, int max_queue_size = 0,
              bool open_log = true);

    void write_log(int level, const char *format, ...);

    void flush(void);

    bool get_log_status() const {
        return open_log;
    }

private:
    Log();
    virtual ~Log();
    void *async_write_log() {
        char *single_log;
        // 从阻塞队列中取出一个日志string，写入文件
        while (m_log_queue->pop(single_log)) {
            lock_guard lk(m_mutex);
            fputs(single_log, m_fp);
        }
        return NULL;
    }


private:
    char dir_name[128]; // 路径名
    char log_name[128]; // log文件名
    int m_split_lines;  // 日志最大行数
    int m_log_buf_size; // 日志缓冲区大小
    long long m_count;  // 日志行数记录
    int m_today;        // 因为按天分类,记录当前时间是那一天
    FILE *m_fp;         // 打开log的文件指针
    char *m_buf;
    block_queue<char *> *m_log_queue; // 阻塞队列
    bool m_is_async;                  // 是否同步标志位
    locker m_mutex;
    bool open_log;
};

#define LOG_DEBUG(format, ...)                 \
    if (Log::get_instance()->get_log_status()) \
        Log::get_instance()->write_log(0, format, ##__VA_ARGS__);

#define LOG_INFO(format, ...)                  \
    if (Log::get_instance()->get_log_status()) \
        Log::get_instance()->write_log(1, format, ##__VA_ARGS__);

#define LOG_WARN(format, ...)                  \
    if (Log::get_instance()->get_log_status()) \
        Log::get_instance()->write_log(2, format, ##__VA_ARGS__);

#define LOG_ERROR(format, ...)                 \
    if (Log::get_instance()->get_log_status()) \
        Log::get_instance()->write_log(3, format, ##__VA_ARGS__);

#endif
