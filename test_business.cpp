#include "test_business.h"
#include "logger.h"
#include "log_queue.h"
#include "log_crypto.h"
#include "log_common.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// 伪业务函数：模拟不同业务场景产生日志
void simulate_login(int id, int count) {
    for (int i = 0; i < count; ++i) {
        log_app(LogLevel::INFO, "thread %d login user #%d", id, i);
        log_operation(LogLevel::INFO, "thread %d login audit #%d", id, i);
    }
}

void simulate_order(int id, int count) {
    for (int i = 0; i < count; ++i) {
        log_app(LogLevel::INFO, "thread %d order created #%d", id, i);
        log_operation(LogLevel::INFO, "thread %d order updated #%d", id, i);
    }
}

void simulate_file_task(int id, int count) {
    for (int i = 0; i < count; ++i) {
        log_app(LogLevel::INFO, "thread %d file upload start #%d", id, i);
        log_operation(LogLevel::INFO, "thread %d file upload finish #%d", id, i);
    }
}

void simulate_error(int id, int count) {
    for (int i = 0; i < count; ++i) {
        log_app(LogLevel::WARN, "thread %d warning before error #%d", id, i);
        log_operation(LogLevel::INFO, "thread %d error handling #%d", id, i);
        log_error("thread %d simulated error #%d", id, i);
    }
}

// 每个业务线程循环产生不同业务场景日志
static void business_thread_func(int id, int count) {
    switch (id % 4) {
        case 0: simulate_login(id, count); break;
        case 1: simulate_order(id, count); break;
        case 2: simulate_file_task(id, count); break;
        default: simulate_error(id, count); break;
    }
}

void run_business_test(int num_threads, int per_thread) {
    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t)
        workers.emplace_back(business_thread_func, t, per_thread);
    for (auto& w : workers) w.join();
}

// ---------------- 单元测试 ----------------
static bool file_nonempty(const std::string& path) {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    return !ec && sz > 0;
}

bool test_basic_log() {
    fs::remove_all("test_logs_basic");
    log_init("test_logs_basic", LogLevel::DEBUG, QueueKind::Mutex);
    log_app(LogLevel::INFO, "hello basic %d", 1);
    log_operation(LogLevel::INFO, "op basic %d", 2);
    log_error("err basic %d", 3);
    log_close();
    return file_nonempty("test_logs_basic/application/application_001.log.enc") &&
           file_nonempty("test_logs_basic/operation/operation_001.log.enc") &&
           file_nonempty("test_logs_basic/error/error_001.log.enc");
}

bool test_level_filter() {
    fs::remove_all("test_logs_filter");
    log_init("test_logs_filter", LogLevel::WARN, QueueKind::Mutex);
    uint64_t before = global_logger().metrics().enqueued();
    log_app(LogLevel::INFO,  "should be filtered");   // < WARN，被过滤
    log_app(LogLevel::DEBUG, "should be filtered");   // < WARN，被过滤
    uint64_t mid = global_logger().metrics().enqueued();
    log_app(LogLevel::ERROR, "should pass");          // >= WARN，入队
    uint64_t after = global_logger().metrics().enqueued();
    log_close();
    return (mid == before) && (after == before + 1);
}

bool test_file_rotate() {
    fs::remove_all("test_logs_rotate");
    log_init("test_logs_rotate", LogLevel::DEBUG, QueueKind::Mutex);
    int total = (int)MAX_LINES_PER_FILE + 50;
    for (int i = 0; i < total; ++i)
        log_app(LogLevel::INFO, "rotate line %d", i);
    log_close();
    // 超过单文件行数上限后应切分出第二个文件
    return fs::exists("test_logs_rotate/application/application_002.log.enc");
}

bool test_queue_full() {
    MutexRingQueue q(4);
    LogRecord rec{};
    rec.level    = LogLevel::INFO;
    rec.category = LogCategory::APP;
    std::snprintf(rec.text, LOG_TEXT_CAPACITY, "x");
    int ok = 0, dropped = 0;
    for (int i = 0; i < 6; ++i) {
        if (q.try_push(rec)) ++ok; else ++dropped;
    }
    // 容量 4：应成功 4 条、丢弃 2 条
    return ok == 4 && dropped == 2;
}

bool test_decrypt_one_line() {
    fs::remove_all("test_logs_decrypt");
    log_init("test_logs_decrypt", LogLevel::DEBUG, QueueKind::Mutex);
    log_app(LogLevel::INFO, "decrypt marker ABC123");
    log_close();
    std::ifstream in("test_logs_decrypt/application/application_001.log.enc");
    std::string hexline;
    std::getline(in, hexline);
    std::string plain = xor_decrypt_from_hex(hexline);
    return plain.find("decrypt marker ABC123") != std::string::npos;
}

bool run_all_tests() {
    struct Case { const char* name; bool (*fn)(); };
    Case cases[] = {
        {"test_basic_log",        test_basic_log},
        {"test_level_filter",     test_level_filter},
        {"test_file_rotate",      test_file_rotate},
        {"test_queue_full",       test_queue_full},
        {"test_decrypt_one_line", test_decrypt_one_line},
    };
    bool all = true;
    std::printf("\n==== 单元测试 ====\n");
    for (auto& c : cases) {
        bool pass = c.fn();
        all = all && pass;
        std::printf("  [%s] %s\n", pass ? "PASS" : "FAIL", c.name);
    }
    std::printf("==== %s ====\n", all ? "全部通过" : "存在失败");
    return all;
}
