#pragma once
// 公共类型与常量定义（本模块不实现业务函数）
#include <cstddef>
#include <cstdint>

// 日志等级
enum class LogLevel : int {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
};

// 日志类别
enum class LogCategory : int {
    APP       = 0,   // 应用日志
    OPERATION = 1,   // 操作日志
    ERROR     = 2,   // 错误日志
};

// 单条日志文本容量
inline constexpr std::size_t   LOG_TEXT_CAPACITY  = 512;
// 队列容量（需为 2 的幂，供无锁队列取模用）
inline constexpr std::size_t   LOG_QUEUE_CAPACITY = 8192;
// 后台线程单次最多批量出队条数
inline constexpr std::size_t   BATCH_SIZE         = 32;
// 单个日志文件最大行数，超过后切分
inline constexpr std::size_t   MAX_LINES_PER_FILE = 1000;
// XOR 加密密钥
inline constexpr unsigned char LOG_XOR_KEY        = 0x5A;

// 队列中真正保存的一条日志
struct LogRecord {
    LogLevel    level;
    LogCategory category;
    char        text[LOG_TEXT_CAPACITY];
};

inline const char* level_name(LogLevel lv) {
    switch (lv) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

inline const char* category_name(LogCategory c) {
    switch (c) {
        case LogCategory::APP:       return "application";
        case LogCategory::OPERATION: return "operation";
        case LogCategory::ERROR:     return "error";
    }
    return "unknown";
}
