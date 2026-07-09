# 高并发异步日志系统（C++17）

这是一个本地高并发异步日志系统：业务线程负责格式化并入队，后台线程批量出队后按类别写文件，写入前做 XOR 加密并保存为 `.log.enc`。

项目支持两种队列实现（`MutexRingQueue` / `LockFreeRingQueue`），并提供统一压测框架比较吞吐、延迟分位和丢弃率。

## 核心能力

- 多线程并发写日志，单后台线程异步落盘。
- 按类别分目录：`application` / `operation` / `error`。
- 日志文件按行数切分（`MAX_LINES_PER_FILE`）。
- 入队失败统计丢弃数，支持关闭收尾（drain 队列）。
- 支持 `run` / `test` / `bench` 三种运行模式。

## 当前架构

### 线程模型

- 生产者：多个业务线程调用 `log_app` / `log_operation` / `log_error`。
- 消费者：`Logger::thread_func` 单线程批量消费并写盘。
- 队列模型：项目实际使用场景为 MPSC；无锁实现采用 Vyukov bounded MPMC 算法。

### 数据流

1. 业务线程调用日志接口。
2. `Logger::vwrite` 做等级过滤与格式化。
3. 尝试 `queue_->try_push`，成功则唤醒后台线程，失败则计入 dropped。
4. 后台线程 `pop_batch` 取数据并写入 `LogWriter`。
5. `LogWriter` 对文本执行 `xor_encrypt_to_hex` 后写入 `.log.enc`。
6. 达到行数阈值时自动 rotate 文件。

## 模块说明

| 路径 | 作用 |
| --- | --- |
| `src/log_common.h` | 公共类型与常量（等级、类别、队列容量、切分阈值等） |
| `src/log_queue.h` | 队列抽象 `IQueue` 与加锁实现 `MutexRingQueue` |
| `src/log_queue_lockfree.h` | 无锁实现 `LockFreeRingQueue` |
| `src/logger.h` / `src/logger.cpp` | 日志核心、后台线程、全局接口封装 |
| `src/log_writer.h` / `src/log_writer.cpp` | 分类写盘、切分、flush/close |
| `src/log_crypto.h` / `src/log_crypto.cpp` | XOR + 十六进制编解码 |
| `src/metrics.h` / `src/metrics.cpp` | 指标采集（enqueued/dropped/written、P50/P90/P99） |
| `src/benchmark.h` + `benchmark.cpp` | 压测配置与对比执行 |
| `test/test_business.cpp` | 业务模拟与 5 个测试用例 |
| `main.cpp` | 程序入口（run/test/bench） |

## 目录与产物

### 编译产物

- 可执行文件：`bin/logsys`
- 中间文件：`build/`

### 运行期输出

- 默认运行日志：`runtime/logs/`
- 压测日志：`runtime/bench_logs/`
- 测试日志：`runtime/test_logs_basic/`、`runtime/test_logs_filter/`、`runtime/test_logs_rotate/`、`runtime/test_logs_decrypt/`

示例文件：

- `runtime/logs/application/application_001.log.enc`
- `runtime/logs/operation/operation_001.log.enc`
- `runtime/logs/error/error_001.log.enc`

## 快速开始

```bash
make           # 编译
make run       # 默认演示：业务并发写日志 + 解密预览 + 单测
make test      # 仅执行单测
make bench     # 队列对比压测
make clean     # 清理 build/bin
```

## 运行模式

- `run`：初始化 `runtime/logs`，运行 5 线程 × 1000 条日志，打印丢弃计数，关闭后做解密预览并执行单测。
- `test`：运行 `test_basic_log`、`test_level_filter`、`test_file_rotate`、`test_queue_full`、`test_decrypt_one_line`。
- `bench`：在线程数 `{1, 4, 8, 16}` 下分别比较 `mutex` 与 `lockfree`。

## 设计说明

- 过滤与格式化在入队前完成，减少队列临界区开销。
- 后台线程批量消费，降低唤醒与 I/O 调度开销。
- `close()` 会先停接收，再清空队列并 flush，尽量避免退出丢日志。
- 统计指标面向“入队路径性能”与“系统丢弃行为”，便于压测分析瓶颈。
