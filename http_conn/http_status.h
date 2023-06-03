#ifndef HTTP_STATUS
#define HTTP_STATUS
//  HTTP 的 请求方法
enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };


// 主状态机的状态: 请求行, 请求头, 请求体
enum CHECK_STATE {
    CHECK_STATE_REQUEST_LINE = 0, // 当前正在分析请求行
    CHECK_STATE_HEADER,           // 当前正在分析头部字段
    CHECK_STATE_CONTENT           // 当前正在分析请求体
};

// 从状态机的状态: 读取状态, 成功(完整行), 失败, 数据还不完整
enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };


// 服务器处理 HTTP 请求的可能结果, 报文解析的结果
enum HTTP_CODE {
    NO_REQUEST = 0,    // 请求不完整, 需要继续读取客户数据
    GET_REQUEST,       // 获得了一个完成的客户请求
    BAD_REQUEST,       // 客户请求语法错误
    NO_RESOURCE,       // 服务器没有相应资源
    FORBIDDEN_REQUEST, // 权限不足
    FILE_REQUEST,      // 文件请求, 获取文件成功
    INTERNAL_ERROR,    // 服务器内部错误
    CLOSED_CONNECTION, // 客户端关闭连接
};
#endif
