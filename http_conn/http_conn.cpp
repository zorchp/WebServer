#include "http_conn.h"
#include "http_status.h"

// ignore clang enum switch variable warning
#pragma clang diagnostic ignored "-Wswitch"

// 一些定义
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form =
    "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form =
    "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form =
    "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form =
    "There was an unusual problem serving the requested file.\n";

// html 资源 根目录
const char* doc_root = "/assets";

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

// 义务性定义
const int http_conn::READ_BUF_SIZE;  // 读缓冲区大小
const int http_conn::WRITE_BUF_SIZE; // 写缓冲区大小
const int http_conn::FILENAME_LEN;   // 文件名长度支持

void set_nonblocking(int fd) {
    int old_flg = fcntl(fd, F_GETFL);
    int new_flg = old_flg | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flg);
}

// 添加需要监听的文件描述符
void add_fd(int epoll_fd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot) event.events |= EPOLLONESHOT;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

// 删除 监听的 描述符
void remove_fd(int epoll_fd, int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改, 重置 EPOLLONESHOT 事件, 确保下一次可读时, EPOLLIN 可以触发
void modify_fd(int epoll_fd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP | EPOLLET; // 边缘触发
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
}


void http_conn::init(int sockfd, const sockaddr_in& addr) {
    m_sockfd = sockfd;
    m_addr = addr;
    // port reuse
    int reuse{1};
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    add_fd(m_epollfd, sockfd, true);
    ++m_user_count;

    status_init(); // 初始化状态
}

void http_conn::close_conn() {
    if (m_sockfd != -1) {
        remove_fd(m_epollfd, m_sockfd);
        m_sockfd = -1; // 未使用
        --m_user_count;
    }
}


// 一次性读取所有数据, 循环读取, 直到客户端关闭连接或者无数据可读
bool http_conn::read() {
    if (m_read_idx >= READ_BUF_SIZE) { // 读取超出了缓冲区大小
        return false;
    }
    // 读取到的字节数
    int total{};
    while (1) {
        total = recv(m_sockfd, m_read_buf + m_read_idx,
                     READ_BUF_SIZE - m_read_idx, 0);
        if (total == -1) {
            // 非阻塞读
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有数据
                break;
            }
            // ERR(recv);
            return false;
        } else if (total == 0) {
            return false;
        }
        m_read_idx += total;
        LOG_INFO("read data : %s", m_read_buf);
        // printf("read data:\n%s\n", m_read_buf);
    }
    return true;
}

bool http_conn::write() { // in main.cpp
    int tmp{};
    if (bytes_to_send == 0) {
        //
        modify_fd(m_epollfd, m_sockfd, EPOLLIN);
        status_init();
        return true;
    }
    //

    while (1) {
        tmp = writev(m_sockfd, m_iv, m_iv_cnt);
        if (tmp < 0) {
            // TCP 写缓冲没有空间, 等待下一轮 EPOLLOUT 事件,
            // 服务器无法立即接收到同一客户的下一个请求, 但保证了连接完整性
            if (errno == EAGAIN) {
                modify_fd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= tmp;
        bytes_sent += tmp;
        // add this
        if (bytes_sent >= m_iv[0].iov_len) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_addr + (bytes_sent - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        } else {
            m_iv[0].iov_base = m_write_buf + bytes_sent;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_sent;
        }

        if (bytes_to_send <= 0) {
            unmap();
            modify_fd(m_epollfd, m_sockfd, EPOLLIN);

            if (is_linger) {
                status_init();
                return true;
            } else {
                return false;
            }
        }
    }
}

void http_conn::unmap() {
    if (m_file_addr) {
        munmap(m_file_addr, m_file_stat.st_size);
        m_file_addr = 0;
    }
}

// 工作线程调用, 处理 HTTP 请求的入口函数
void http_conn::process() {
    // 解析 HTTP 请求
    HTTP_CODE read_ans = process_read();
    if (read_ans == NO_REQUEST) {
        modify_fd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    // 生成响应
    modify_fd(m_epollfd, m_sockfd, EPOLLOUT);
    if (!process_write(read_ans)) close_conn();
    // printf("written...\n");
}


void http_conn::status_init() {
    // 初始化状态为解析请求首行
    m_check_state = CHECK_STATE_REQUEST_LINE;
    m_checked_idx = 0;
    m_start_line = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    // request line init
    m_method = GET;
    m_url = 0;
    m_http_version = 0;
    m_hostname = 0;
    is_linger = false;
    m_content_len = 0;
    // 初始化缓冲区
    memset(m_read_buf, 0, READ_BUF_SIZE);
    memset(m_write_buf, 0, WRITE_BUF_SIZE);
    memset(m_real_file, 0, FILENAME_LEN);
    // 发送的数据
    bytes_to_send = 0;
    bytes_sent = 0;
}

HTTP_CODE http_conn::process_read() {
    // 主状态机, 定义初始状态
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ans = NO_REQUEST;

    char* text{};

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
           (line_status = parse_line()) == LINE_OK) {
        // 解析到了一行数据, 或者解析到了请求体(完整的数据)
        text = get_line();
        m_start_line = m_checked_idx;
        // printf("got 1 http line :\n%s\n", text);

        // 开始解析
        switch (m_check_state) {
            case CHECK_STATE_REQUEST_LINE: {
                //
                ans = parse_request_line(text);
                if (ans == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                //
                ans = parse_headers(text);
                if (ans == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (ans == GET_REQUEST) {
                    // printf("get-header\n");
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                //
                ans = parse_content(text);
                if (ans == GET_REQUEST) {
                    // printf("get-body\n");
                    return do_request();
                }
                printf("body-un completed\n");
                line_status = LINE_OPEN;
                break;
            }
            default: {
                //

                return INTERNAL_ERROR;
            }
        }
    }

    return NO_REQUEST;
}

HTTP_CODE http_conn::parse_request_line(char* text) {
    // GET / HTTP/1.1
    m_url = strpbrk(text, " \t");
    *m_url++ = '\0';
    char* method = text; // 获取方法
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else
        return BAD_REQUEST;

    m_http_version = strpbrk(m_url, " \t");
    if (!m_http_version) return BAD_REQUEST;
    *m_http_version++ = '\0';

    if (strcasecmp(m_http_version, "HTTP/1.1") != 0) return BAD_REQUEST;
    // 'http://'
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/'); // 定位第一个出现的字符位置
    }

    // 'https://'
    if (strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/'); // 定位第一个出现的字符位置
    }

    if (!m_url || m_url[0] != '/') return BAD_REQUEST;

    // 当url为/时，显示默认页面
    if (strlen(m_url) == 1) strcat(m_url, "index.html");

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST; // 不完整, 还需要继续解析
}

HTTP_CODE http_conn::parse_headers(char* text) {
    // 遇到空行, 说明头部字段解析完毕
    if (text[0] == '\0') {
        // HTTP 请求有消息体, 还需要读取 m_content_len 字节的消息体
        if (m_content_len != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) // 长连接
            is_linger = true;
    } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_len = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_hostname = text;
    } else {
        // printf("unknown header %s\n", text);
    }

    return NO_REQUEST;
}

HTTP_CODE http_conn::parse_content(char* text) {
    // 未解析
    if (m_read_idx >= (m_content_len + m_checked_idx)) {
        text[m_content_len] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

LINE_STATUS http_conn::parse_line() {
    for (char tmp{}; m_checked_idx < m_read_idx; ++m_checked_idx) {
        tmp = m_read_buf[m_checked_idx];
        if (tmp == '\r') {
            if (m_checked_idx + 1 == m_read_idx) { // 到尾
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') { // 完整的一行
                m_read_buf[m_checked_idx++] = '\0';             // '\r' ->'\0'
                m_read_buf[m_checked_idx++] = '\0';             // '\n' ->'\0'
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (tmp == '\n') { // 往回找匹配的'\r'
            // 为什么会到这里呢?
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //
    return LINE_OPEN;
}


// 分析文件属性
HTTP_CODE http_conn::do_request() {
    //
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    // 获取根目录
    char buf[FILENAME_LEN];
    strcat(getcwd(buf, FILENAME_LEN), m_real_file);
    strcpy(m_real_file, buf);
    // printf("%s\n", m_real_file); // assets
    // printf("%s\n", buf);
    if (stat(m_real_file, &m_file_stat) < 0) return NO_RESOURCE;

    // 权限
    if (!(m_file_stat.st_mode & S_IROTH)) return FORBIDDEN_REQUEST;

    // 资源
    if (S_ISDIR(m_file_stat.st_mode)) return BAD_REQUEST;


    int fd = open(m_real_file, O_RDONLY);
    // 创建内存映射
    m_file_addr =
        (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}


// 发回, 生成响应, 具体操作
bool http_conn::add_response(const char* format, ...) {
    if (m_write_idx >= WRITE_BUF_SIZE) return false;

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx,
                        WRITE_BUF_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUF_SIZE - 1 - m_write_idx)) return false;

    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len) {
    return add_content_length(content_len) && add_linger() && add_blank_line();
}

bool http_conn::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger() {
    return add_response("Connection: %s\r\n",
                        (is_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line() { return add_response("%s", "\r\n"); }

bool http_conn::add_content(const char* content) {
    return add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)) {
                return false;
            }
            break;
        }
        case BAD_REQUEST: {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form)) {
                return false;
            }
            break;
        }
        case NO_RESOURCE: {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form)) {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form)) {
                return false;
            }
            break;
        }
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0) {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_addr;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_cnt = 2;
                // 发送字节
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                // printf("data:\n%s\n", m_write_buf);
                return true;
            } else {
                const char* ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string)) {
                    return false;
                }
            }
        }
        default:
            return false;
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_cnt = 1;
    return true;
}
