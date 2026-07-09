#include "metrics.h"
#include <cstdio>

static int bucket_of(uint64_t ns) {
    if (ns == 0) return 0;
    int b = 63 - __builtin_clzll(ns);   // floor(log2(ns))
    if (b < 0) b = 0;
    if (b > 63) b = 63;
    return b;
}

void Metrics::reset() {
    enqueued_.store(0);
    dropped_.store(0);
    written_.store(0);
    latency_sum_.store(0);
    latency_cnt_.store(0);
    for (int i = 0; i < BUCKETS; ++i)
        hist_[i].store(0, std::memory_order_relaxed);
}

void Metrics::record_latency(uint64_t ns) {
    hist_[bucket_of(ns)].fetch_add(1, std::memory_order_relaxed);
    latency_sum_.fetch_add(ns, std::memory_order_relaxed);
    latency_cnt_.fetch_add(1, std::memory_order_relaxed);
}

Metrics::Summary Metrics::compute() const {
    Summary s;
    s.enqueued = enqueued();
    s.dropped  = dropped();
    s.written  = written();
    uint64_t attempts = s.enqueued + s.dropped;
    s.drop_rate = attempts ? (double)s.dropped / (double)attempts : 0.0;

    uint64_t cnt = latency_cnt_.load(std::memory_order_relaxed);
    uint64_t sum = latency_sum_.load(std::memory_order_relaxed);
    s.avg_ns = cnt ? sum / cnt : 0;

    auto percentile = [&](double p) -> uint64_t {
        if (cnt == 0) return 0;
        uint64_t target = (uint64_t)(p * (double)cnt);
        uint64_t acc = 0;
        for (int b = 0; b < BUCKETS; ++b) {
            acc += hist_[b].load(std::memory_order_relaxed);
            if (acc >= target) return (b == 0) ? 0 : (1ull << b);
        }
        return 1ull << 63;
    };
    s.p50_ns = percentile(0.50);
    s.p90_ns = percentile(0.90);
    s.p99_ns = percentile(0.99);
    return s;
}

void Metrics::report(const std::string& tag) const {
    Summary s = compute();
    std::printf("[metrics %s] enqueued=%llu dropped=%llu written=%llu drop=%.3f%% "
                "avg=%lluns p50=%lluns p90=%lluns p99=%lluns\n",
                tag.c_str(),
                (unsigned long long)s.enqueued, (unsigned long long)s.dropped,
                (unsigned long long)s.written, s.drop_rate * 100.0,
                (unsigned long long)s.avg_ns, (unsigned long long)s.p50_ns,
                (unsigned long long)s.p90_ns, (unsigned long long)s.p99_ns);
}
