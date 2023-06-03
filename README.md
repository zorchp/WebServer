# WebServer

WebServer using C++11

> environment:
> Ubuntu 20.04 x86_64 g++ 9.4
> CPU: 2 RAM: 4GB

# 值得注意的点

1. 运行之前先忽略`SIGPIPE` 信号
2.

# bug and solution

1. read-error, connection refuse
2.

# TODO List

1. 手写数据结构(线程池中的双向链表)
2. 修复大文件传输问题(视频播放时候一卡一卡)

# TODO List (optional)

1. 守护进程运行
2. 增加 reactor 模式
3. 优化 HTTP 报文解析, 增加多版本支持: HTTP/1.0, HTTP/1.1(default)
