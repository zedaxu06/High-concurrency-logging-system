#include "logger.h"
#include "log_queue_lockfree.h"
#include <chrono>
#include <ctime>

using clock_type = std::chrono::steady_clock;

bool Logger::init(const Config& cfg) {
    if (initialized_) return false;
    min_level_ = cfg.min_level;
    metrics_.reset();

    if (cfg.queue == QueueKind::LockFree)
        queue_ = std::make_unique<LockFreeRingQueue>(LOG_QUEUE_CAPACITY);
    else
        queue_ = std::make_unique<MutexRingQueue>(LOG_QUEUE_CAPACITY);

    writer_ = std::make_unique<LogWriter>(cfg.dir);
    writer_->init();

    running_.store(true);
    initialized_ = true;
    thread_ = std::thread(&Logger::thread_func, this);
    return true;
}

void Logger::close() {
    if (!initialized_) return;
    running_.store(false);
    wake_cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    writer_->close_all();
    queue_.reset();
    initialized_ = false;
}

void Logger::vwrite(LogLevel level, LogCategory cat, const char* fmt, va_list ap) {
    if (!initialized_) return;
    if ((int)level < (int)min_level_) return;    // 等级过滤（入队前完成）

    LogRecord rec;
    rec.level    = level;
    rec.category = cat;

    // 格式化在加锁/入队之前完成，避免多线程长时间抢占队列
    std::time_t now = std::time(nullptr);
    std::tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif

    char ts[24];
    std::strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);

    int off = std::snprintf(rec.text, LOG_TEXT_CAPACITY, "%s [%s] ", ts, level_name(level));
    if (off < 0 || off >= (int)LOG_TEXT_CAPACITY) off = (int)LOG_TEXT_CAPACITY - 1;
    std::vsnprintf(rec.text + off, LOG_TEXT_CAPACITY - (std::size_t)off, fmt, ap);

    auto t0 = clock_type::now();
    bool ok = queue_->try_push(rec);
    auto t1 = clock_type::now();
    uint64_t ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    metrics_.record_latency(ns);

    if (ok) {
        metrics_.inc_enqueued();
        wake_cv_.notify_one();      // 唤醒后台写盘线程
    } else {
        metrics_.inc_dropped();     // 队列满，丢弃计数
    }
}

void Logger::app(LogLevel level, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vwrite(level, LogCategory::APP, fmt, ap);
    va_end(ap);
}
void Logger::operation(LogLevel level, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vwrite(level, LogCategory::OPERATION, fmt, ap);
    va_end(ap);
}
void Logger::error(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vwrite(LogLevel::ERROR, LogCategory::ERROR, fmt, ap);
    va_end(ap);
}

void Logger::thread_func() {
    LogRecord batch[BATCH_SIZE];
    while (running_.load()) {
        std::size_t n = queue_->pop_batch(batch, BATCH_SIZE);
        if (n == 0) {
            std::unique_lock<std::mutex> lk(wake_mtx_);
            wake_cv_.wait_for(lk, std::chrono::milliseconds(5));
            continue;
        }
        for (std::size_t i = 0; i < n; ++i) {
            writer_->write(batch[i]);
            metrics_.inc_written();
        }
        writer_->flush_all();
    }
    // 收尾：把队列里剩余日志全部写完
    std::size_t n;
    while ((n = queue_->pop_batch(batch, BATCH_SIZE)) > 0) {
        for (std::size_t i = 0; i < n; ++i) {
            writer_->write(batch[i]);
            metrics_.inc_written();
        }
    }
    writer_->flush_all();
}

// ============ 全局单例接口 ============
Logger& global_logger() {
    static Logger inst;
    return inst;
}

bool log_init(const char* dir, LogLevel min_level, QueueKind kind) {
    Logger::Config cfg;
    cfg.dir       = dir ? dir : "logs";
    cfg.min_level = min_level;
    cfg.queue     = kind;
    return global_logger().init(cfg);
}

void log_app(LogLevel level, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    global_logger().vwrite(level, LogCategory::APP, fmt, ap);
    va_end(ap);
}
void log_operation(LogLevel level, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    global_logger().vwrite(level, LogCategory::OPERATION, fmt, ap);
    va_end(ap);
}
void log_error(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    global_logger().vwrite(LogLevel::ERROR, LogCategory::ERROR, fmt, ap);
    va_end(ap);
}
void log_close() { global_logger().close(); }
uint64_t log_get_dropped_count() { return global_logger().dropped_count(); }
