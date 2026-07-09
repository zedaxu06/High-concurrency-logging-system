#pragma once
// 日志系统核心：业务线程入队，后台线程消费写盘
#include "log_common.h"
#include "log_queue.h"
#include "log_writer.h"
#include "metrics.h"
#include <atomic>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

enum class QueueKind { Mutex, LockFree };

class Logger {
public:
    struct Config {
        std::string dir       = "logs";
        LogLevel    min_level = LogLevel::DEBUG;
        QueueKind   queue     = QueueKind::Mutex;
    };

    Logger() = default;
    ~Logger() { close(); }

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    bool init(const Config& cfg);
    void close();

    void app(LogLevel level, const char* fmt, ...);
    void operation(LogLevel level, const char* fmt, ...);
    void error(const char* fmt, ...);

    // 统一内部入口（各对外函数都委托到这里）
    void vwrite(LogLevel level, LogCategory cat, const char* fmt, va_list ap);

    uint64_t dropped_count() const { return metrics_.dropped(); }
    Metrics& metrics() { return metrics_; }

private:
    void thread_func();   // 后台写盘线程

    std::unique_ptr<IQueue>    queue_;
    std::unique_ptr<LogWriter> writer_;
    Metrics                    metrics_;
    LogLevel                   min_level_ = LogLevel::DEBUG;
    std::atomic<bool>          running_{false};
    bool                       initialized_ = false;
    std::thread                thread_;
    std::mutex                 wake_mtx_;
    std::condition_variable    wake_cv_;
};

// 与预案一致的全局接口（内部委托给单例 Logger）
bool     log_init(const char* dir, LogLevel min_level = LogLevel::DEBUG,
                  QueueKind kind = QueueKind::Mutex);
void     log_app(LogLevel level, const char* fmt, ...);
void     log_operation(LogLevel level, const char* fmt, ...);
void     log_error(const char* fmt, ...);
void     log_close();
uint64_t log_get_dropped_count();
Logger&  global_logger();
