#pragma once
// 有界无锁队列：Vyukov bounded MPMC 算法，适用于本项目的 MPSC 场景
#include "log_common.h"
#include "log_queue.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

class LockFreeRingQueue : public IQueue {
public:
    explicit LockFreeRingQueue(std::size_t cap = LOG_QUEUE_CAPACITY) {
        std::size_t n = 1;
        while (n < cap) n <<= 1;                       // 向上取整到 2 的幂
        cap_  = n;
        mask_ = n - 1;
        buffer_.reset(new Cell[cap_]());               // 值初始化，原子清零
        for (std::size_t i = 0; i < cap_; ++i)
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    // 多生产者入队：fetch/CAS 抢占槽位，写完后用 release 提交序号
    bool try_push(const LogRecord& rec) override {
        Cell* cell;
        std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & mask_];
            std::size_t seq = cell->seq.load(std::memory_order_acquire);
            std::intptr_t dif = (std::intptr_t)seq - (std::intptr_t)pos;
            if (dif == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed))
                    break;
            } else if (dif < 0) {
                return false;                          // 队列已满
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
        cell->data = rec;
        cell->seq.store(pos + 1, std::memory_order_release);
        return true;
    }

    // 单消费者批量出队
    std::size_t pop_batch(LogRecord* out, std::size_t max) override {
        std::size_t n = 0;
        while (n < max) {
            Cell* cell;
            std::size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
            for (;;) {
                cell = &buffer_[pos & mask_];
                std::size_t seq = cell->seq.load(std::memory_order_acquire);
                std::intptr_t dif = (std::intptr_t)seq - (std::intptr_t)(pos + 1);
                if (dif == 0) {
                    if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                            std::memory_order_relaxed))
                        break;
                } else if (dif < 0) {
                    return n;                          // 队列已空
                } else {
                    pos = dequeue_pos_.load(std::memory_order_relaxed);
                }
            }
            out[n++] = cell->data;
            cell->seq.store(pos + mask_ + 1, std::memory_order_release);
        }
        return n;
    }

    std::size_t approx_size() const override {
        std::size_t e = enqueue_pos_.load(std::memory_order_relaxed);
        std::size_t d = dequeue_pos_.load(std::memory_order_relaxed);
        return e - d;
    }

    const char* name() const override { return "lockfree"; }

private:
    struct Cell {
        std::atomic<std::size_t> seq;
        LogRecord                data;
    };
    std::unique_ptr<Cell[]>              buffer_;
    std::size_t                          cap_  = 0;
    std::size_t                          mask_ = 0;
    alignas(64) std::atomic<std::size_t> enqueue_pos_{0};
    alignas(64) std::atomic<std::size_t> dequeue_pos_{0};
};
