#pragma once
// 性能指标采集：原子计数器 + 延迟直方图
#include <atomic>
#include <cstdint>
#include <string>

class Metrics {
public:
    Metrics() { reset(); }

    void reset();
    void record_latency(uint64_t ns);

    void inc_enqueued() { enqueued_.fetch_add(1, std::memory_order_relaxed); }
    void inc_dropped()  { dropped_.fetch_add(1, std::memory_order_relaxed); }
    void inc_written()  { written_.fetch_add(1, std::memory_order_relaxed); }

    uint64_t enqueued() const { return enqueued_.load(std::memory_order_relaxed); }
    uint64_t dropped()  const { return dropped_.load(std::memory_order_relaxed); }
    uint64_t written()  const { return written_.load(std::memory_order_relaxed); }

    struct Summary {
        uint64_t enqueued = 0, dropped = 0, written = 0;
        double   drop_rate = 0.0;
        uint64_t p50_ns = 0, p90_ns = 0, p99_ns = 0, avg_ns = 0;
    };

    Summary compute() const;
    void    report(const std::string& tag) const;

private:
    static constexpr int BUCKETS = 64;   // 按 floor(log2(ns)) 分桶
    std::atomic_uint_fast64_t enqueued_{0}, dropped_{0}, written_{0};
    std::atomic_uint_fast64_t latency_sum_{0}, latency_cnt_{0};
    std::atomic_uint_fast64_t hist_[BUCKETS];
};
