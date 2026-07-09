# 高并发异步日志系统（C++ 实现）

一个本地 C++ 异步日志库：业务线程写日志请求 → 进入异步队列 → 后台线程批量出队、分类、XOR 加密、写入 `.log.enc` 文件，并支持关闭收尾与丢弃统计。

在基础功能之上，额外实现了 **加锁队列 vs 无锁队列** 两套实现，配套压测框架量化两者在不同并发度下的吞吐量与延迟差异。

## 项目功能

- 多业务线程模拟真实业务场景，产生日志请求。
- 后台线程批量消费日志，按类别写入不同文件。
- 日志写入前完成等级过滤和格式化，降低锁竞争。
- 支持日志文件按行数切分，自动生成 `.log.enc` 文件。
- 支持关闭收尾，尽量把队列中的剩余日志写完。
- 支持压测对比加锁队列和无锁队列。

## 目录结构

| 文件 | 说明 |
| --- | --- |
| `log_common.h` | 公共类型与常量 |
| `log_queue.h` | 加锁队列抽象与实现 |
| `log_queue_lockfree.h` | 无锁队列实现 |
| `log_crypto.h/.cpp` | XOR 加密与解密 |
| `metrics.h/.cpp` | 统计指标 |
| `log_writer.h/.cpp` | 后台写盘与文件切分 |
| `logger.h/.cpp` | 日志系统核心接口 |
| `test_business.h/.cpp` | 伪业务线程与单元测试 |
| `benchmark.h/.cpp` | 压测对比 |
| `main.cpp` | 程序入口 |
| `Makefile` | 编译与运行命令 |

## 日志输出位置

默认日志输出到 `logs/` 目录下：

- `logs/application/application_001.log.enc`
- `logs/operation/operation_001.log.enc`
- `logs/error/error_001.log.enc`

压测输出默认写到 `bench_logs/`，单元测试输出写到 `test_logs_*` 目录。

## 快速开始

```bash
make            # 编译，产物为 ./logsys
make run        # 基础功能演示（5 线程 × 1000 条）+ 单元测试
make test       # 只跑单元测试
make bench      # 加锁队列 vs 无锁队列 压测对比
make clean      # 清理
```

## 编译命令

```bash
make
```

## 运行命令

```bash
make run
```

## 测试方法

```bash
make test
```

测试会自动检查：基础写日志、等级过滤、文件切分、队列满丢弃、密文解密。

## 模块结构

| 文件 | 职责 |
| --- | --- |
| `log_common.h` | 公共类型（`LogLevel` / `LogCategory` / `LogRecord`）与常量 |
| `log_queue.h` | 队列抽象接口 `IQueue` + 加锁循环队列 `MutexRingQueue` |
| `log_queue_lockfree.h` | 有界无锁队列 `LockFreeRingQueue`（Vyukov bounded MPMC 算法） |
| `log_crypto.{h,cpp}` | XOR 加密 / 十六进制编解码 |
| `metrics.{h,cpp}` | 原子计数器 + 延迟直方图，计算 P50/P90/P99 |
| `log_writer.{h,cpp}` | 后台写盘：按类别分文件、加密、按行数切分 |
| `logger.{h,cpp}` | 日志系统核心 + 与预案一致的全局接口 |
| `test_business.{h,cpp}` | 高并发压测线程 + 5 个单元测试 |
| `benchmark.{h,cpp}` | 压测框架，驱动两套队列跑相同负载并对比 |
| `main.cpp` | 入口：`run` / `test` / `bench` 三种模式 |

## 设计要点

- **模型**：多业务线程生产、单后台线程消费，即 **MPSC（多生产者单消费者）**。
- **锁的边界**：`MutexRingQueue` 内部用互斥量保证线程安全；后台线程 **取出日志后立即让出队列**，`fopen/fwrite/fflush` 等慢速磁盘操作不持有队列锁。
- **无锁对照**：`LockFreeRingQueue` 用每槽序号 + CAS 实现有界无锁入队/出队，容量取 2 的幂用位与取模。
- **等级过滤与格式化在入队前完成**，避免多线程长时间抢占队列。
- **关闭收尾**：`log_close` 设置停止标志、唤醒后台线程、把队列剩余日志写完，再关闭文件。
- **加密**：仅在后台线程调用，写入 `.log.enc`；`xor_decrypt_from_hex` 可还原验证。

## 压测结论（示例，机器不同数值会变）

无锁队列在各并发度下 **入队延迟与尾延迟（P99）明显更低、吞吐更高**；但由于瓶颈在单消费者磁盘写入，生产者更快反而会更快填满队列，**丢弃率略高**——这正说明高并发日志系统的真正瓶颈往往在消费端 I/O，而非入队路径。
