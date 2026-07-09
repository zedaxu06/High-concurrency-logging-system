#pragma once
// 队列抽象接口 + 加锁循环队列实现
#include "log_common.h"
#include <cstddef>
#include <mutex>
#include <vector>

// 队列抽象接口：让 logger 在加锁队列与无锁队列之间切换
class IQueue {
public:
    virtual ~IQueue() = default;
    virtual bool        try_push(const LogRecord& rec) = 0;              // 满则返回 false
    virtual std::size_t pop_batch(LogRecord* out, std::size_t max) = 0;  // 单消费者批量出队
    virtual std::size_t approx_size() const = 0;
    virtual const char* name() const = 0;
};

// 加锁循环队列（互斥量保护，作为性能对照组的基线实现）
class MutexRingQueue : public IQueue {
public:
    explicit MutexRingQueue(std::size_t cap = LOG_QUEUE_CAPACITY)
        : buf_(cap), cap_(cap) {}

    bool try_push(const LogRecord& rec) override {
        std::lock_guard<std::mutex> lk(mtx_);
        if (count_ == cap_) return false;   // 队列满，丢弃
        buf_[rear_] = rec;
        rear_ = (rear_ + 1) % cap_;
        ++count_;
        return true;
    }

    std::size_t pop_batch(LogRecord* out, std::size_t max) override {
        std::lock_guard<std::mutex> lk(mtx_);
        std::size_t n = 0;
        while (n < max && count_ > 0) {
            out[n++] = buf_[front_];
            front_ = (front_ + 1) % cap_;
            --count_;
        }
        return n;
    }

    std::size_t approx_size() const override {
        std::lock_guard<std::mutex> lk(mtx_);
        return count_;
    }

    const char* name() const override { return "mutex"; }

private:
    std::vector<LogRecord> buf_;
    std::size_t            cap_;
    std::size_t            front_ = 0, rear_ = 0, count_ = 0;
    mutable std::mutex     mtx_;
};
