# 高并发日志系统架构说明（当前实现）

更新时间：2026-07-09

本文档描述仓库当前可运行实现（C++17），用于替代早期的 C 版本预案说明。

## 1. 架构目标

- 支持高并发业务线程写日志，降低业务线程阻塞时间。
- 使用异步落盘，避免业务线程直接执行慢速 I/O。
- 对日志进行简单加密存储（XOR + HEX）。
- 提供可对比的两种队列实现（加锁与无锁）。
- 提供基础测试与压测，验证功能正确性与性能趋势。

## 2. 总体架构

### 2.1 组件

- `Logger`：系统核心，负责初始化、入队、后台消费线程生命周期。
- `IQueue`：队列抽象接口。
- `MutexRingQueue`：互斥锁循环队列实现。
- `LockFreeRingQueue`：基于序号/CAS 的有界无锁队列实现。
- `LogWriter`：日志分类落盘、文件切分、flush/close。
- `log_crypto`：XOR 加密与十六进制编解码。
- `Metrics`：吞吐和延迟指标采集与汇总。

### 2.2 线程模型

- 生产者：多个业务线程调用 `log_app`/`log_operation`/`log_error`。
- 消费者：单后台线程 `Logger::thread_func`。
- 使用模型：实际业务路径为 MPSC（多生产者单消费者）。

### 2.3 主流程

1. 业务线程调用日志接口。
2. `Logger::vwrite` 完成等级过滤和字符串格式化。
3. 调用 `queue_->try_push` 入队。
4. 入队成功后唤醒后台线程；失败则累加 dropped。
5. 后台线程批量 `pop_batch`。
6. `LogWriter::write` 对文本加密并写入分类文件。
7. 达到 `MAX_LINES_PER_FILE` 后轮转到下一个文件。

## 3. 模块映射（当前代码）

| 文件 | 主要职责 |
| --- | --- |
| `src/log_common.h` | 日志等级/类别、容量和阈值常量、`LogRecord` |
| `src/log_queue.h` | `IQueue` 与 `MutexRingQueue` |
| `src/log_queue_lockfree.h` | `LockFreeRingQueue` |
| `src/logger.h` | `Logger` 类、`QueueKind`、全局 C 风格接口 |
| `src/logger.cpp` | 初始化/关闭、`vwrite`、后台线程、收尾 drain |
| `src/log_writer.h` | `LogWriter` 与分类文件状态 |
| `src/log_writer.cpp` | 创建目录、写密文、切分、flush、close |
| `src/log_crypto.h/.cpp` | XOR 加解密、HEX 合法性校验 |
| `src/metrics.h/.cpp` | 计数器、延迟直方图、P50/P90/P99 |
| `src/benchmark.h` + `benchmark.cpp` | 压测配置、执行、结果输出 |
| `test/test_business.cpp` | 业务模拟与 5 个测试用例 |
| `main.cpp` | 程序入口和三种模式切换 |

## 4. 关键设计点

### 4.1 入队前过滤与格式化

日志等级过滤和文本格式化都在入队前完成，减少队列热点路径停留时间。

### 4.2 队列可替换

`Logger` 通过 `IQueue` 抽象切换 `Mutex` 与 `LockFree` 实现，便于压测对比。

### 4.3 批量消费

后台线程每次最多消费 `BATCH_SIZE` 条，降低频繁唤醒与 I/O 调度开销。

### 4.4 收尾机制

`Logger::close()` 会先停止新日志处理，再 drain 队列剩余数据，最后 flush/close 文件。

### 4.5 加密边界

加密在后台写盘阶段执行，业务线程不承担加密开销。

## 5. 运行与输出路径

### 5.1 编译与运行

```bash
make
make run
make test
make bench
```

### 5.2 可执行文件与中间产物

- 可执行文件：`bin/logsys`
- 对象文件：`build/`

### 5.3 运行时日志目录

- 运行模式：`runtime/logs/`
- 压测模式：`runtime/bench_logs/`
- 测试模式：`runtime/test_logs_basic/`、`runtime/test_logs_filter/`、`runtime/test_logs_rotate/`、`runtime/test_logs_decrypt/`

## 6. 测试与压测现状

### 6.1 单元测试（`make test`）

- `test_basic_log`：基础写入与三分类文件落盘。
- `test_level_filter`：等级过滤是否生效。
- `test_file_rotate`：超过阈值后文件切分。
- `test_queue_full`：队列满时丢弃行为。
- `test_decrypt_one_line`：密文可正确解密。

### 6.2 压测（`make bench`）

- 对比维度：`mutex` vs `lockfree`。
- 线程集合：`{1, 4, 8, 16}`。
- 指标：吞吐、`p50/p90/p99`、`drop_rate`、`enqueued/dropped/written`。

## 7. 当前已知边界

- 系统是进程内本地日志库，未实现网络传输与远程聚合。
- 加密为 XOR，目标是演示加密链路，不提供高强度安全保证。
- 后台消费线程单线程，极端写盘压力下可能成为瓶颈。

## 8. 后续可演进方向

- 提供可配置的刷盘策略（时间窗口/批次阈值）。
- 增加结构化日志字段（JSON 或 key-value）。
- 增加更强加密方案与密钥管理。
- 引入多消费者写盘或分区写盘策略。
