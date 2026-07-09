#pragma once
// 压测框架：统一驱动两套队列跑相同负载并对比
#include "logger.h"
#include <cstdint>
#include <string>
#include <vector>

struct BenchConfig {
    int         threads    = 8;
    int         per_thread = 50000;
    int         warmup     = 1000;
    QueueKind   queue      = QueueKind::Mutex;
    std::string dir        = "bench_logs";
};

struct BenchResult {
    std::string queue;
    int         threads    = 0;
    double      seconds    = 0.0;
    double      throughput = 0.0;   // 有效入队条数 / 秒
    uint64_t    p50_ns = 0, p90_ns = 0, p99_ns = 0, avg_ns = 0;
    double      drop_rate  = 0.0;
    uint64_t    enqueued = 0, dropped = 0, written = 0;
};

BenchResult bench_run(const BenchConfig& cfg);
void        bench_compare(const std::vector<int>& thread_counts, int per_thread);
