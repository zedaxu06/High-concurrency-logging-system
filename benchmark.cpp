#include "benchmark.h"
#include "log_common.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using clk = std::chrono::steady_clock;

BenchResult bench_run(const BenchConfig& cfg) {
    fs::remove_all(cfg.dir);

    Logger logger;
    Logger::Config lc;
    lc.dir       = cfg.dir;
    lc.min_level = LogLevel::DEBUG;
    lc.queue     = cfg.queue;
    logger.init(lc);

    // 预热：稳定缓存与分支预测，避免冷启动干扰
    for (int i = 0; i < cfg.warmup; ++i)
        logger.app(LogLevel::INFO, "warmup %d", i);

    auto produce = [&](int id) {
        for (int i = 0; i < cfg.per_thread; ++i)
            logger.app(LogLevel::INFO, "t%d bench message #%d payload", id, i);
    };

    auto t0 = clk::now();
    std::vector<std::thread> workers;
    workers.reserve(cfg.threads);
    for (int t = 0; t < cfg.threads; ++t)
        workers.emplace_back(produce, t);
    for (auto& w : workers) w.join();
    auto t1 = clk::now();

    double seconds = std::chrono::duration<double>(t1 - t0).count();

    logger.close();   // 等后台把剩余日志写完

    auto s = logger.metrics().compute();
    BenchResult r;
    r.queue      = (cfg.queue == QueueKind::LockFree) ? "lockfree" : "mutex";
    r.threads    = cfg.threads;
    r.seconds    = seconds;
    r.enqueued   = s.enqueued;
    r.dropped    = s.dropped;
    r.written    = s.written;
    r.drop_rate  = s.drop_rate;
    r.p50_ns     = s.p50_ns;
    r.p90_ns     = s.p90_ns;
    r.p99_ns     = s.p99_ns;
    r.avg_ns     = s.avg_ns;
    r.throughput = seconds > 0 ? (double)s.enqueued / seconds : 0.0;
    return r;
}

static void print_header() {
    std::printf("\n%-9s %8s %15s %10s %10s %10s %9s\n",
                "queue", "threads", "throughput/s", "p50(ns)", "p90(ns)", "p99(ns)", "drop%");
    std::printf("---------------------------------------------------------------------------------\n");
}

static void print_row(const BenchResult& r) {
    std::printf("%-9s %8d %15.0f %10llu %10llu %10llu %8.2f%%\n",
                r.queue.c_str(), r.threads, r.throughput,
                (unsigned long long)r.p50_ns, (unsigned long long)r.p90_ns,
                (unsigned long long)r.p99_ns, r.drop_rate * 100.0);
}

void bench_compare(const std::vector<int>& thread_counts, int per_thread) {
    std::printf("压测对比：每线程 %d 条日志，队列容量 %zu，单文件上限 %zu 行\n",
                per_thread, LOG_QUEUE_CAPACITY, MAX_LINES_PER_FILE);
    print_header();
    for (int tc : thread_counts) {
        for (QueueKind kind : {QueueKind::Mutex, QueueKind::LockFree}) {
            BenchConfig cfg;
            cfg.threads    = tc;
            cfg.per_thread = per_thread;
            cfg.queue      = kind;
            cfg.dir        = "bench_logs";
            print_row(bench_run(cfg));
        }
        std::printf("---------------------------------------------------------------------------------\n");
    }
}
