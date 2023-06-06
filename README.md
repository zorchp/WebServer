# WebServer

WebServer using C++11

> environment:

> Ubuntu 20.04 x86_64 g++ 9.4

> CPU: 2 RAM: 4GB

# 值得注意的点

1. 运行之前先忽略`SIGPIPE` 信号
2.

# bug and solution

1. read-error, connection refuse: 设置信号捕捉, `ECONNRESET`
2.

# TODO List

1. 添加阻塞队列+异步日志, 使用双缓冲区方法
2. 修复大文件传输问题(视频播放时候一卡一卡), 尝试多线程写入
3. 手写数据结构(线程池中的双向链表)

# TODO List (optional)

1. 守护进程运行
2. 增加 reactor 模式
3. 优化 HTTP 报文解析, 增加多版本支持: HTTP/1.0, HTTP/1.1(default)
4. webbench 优化
5. 模块化, main 函数优化(提取读写逻辑)
6. 视频传输在 Safari 上的支持(需要回传特定头的报文)
