明白，你们现在不用纠结代码细节，先把**要实现哪些函数、每个函数负责什么**定下来就行。你们接口修订版已经明确：这是本地 C 语言异步日志库，主流程是业务线程写日志请求，日志进入异步队列，后台线程批量出队、分类、加密、写入 `.log.enc` 文件，并支持关闭收尾和丢弃统计。

# 一、公共类型模块：`log_common.h`

这个模块不实现函数，只放公共定义。

需要定义这些内容：

| 名称                   | 作用                                           |
| -------------------- | -------------------------------------------- |
| `LogLevel`           | 表示日志等级：`DEBUG / INFO / WARN / ERROR`         |
| `LogCategory`        | 表示日志类别：`APP_LOG / OPERATION_LOG / ERROR_LOG` |
| `LogRecord`          | 队列中真正保存的一条日志，包括日志等级、日志类别、格式化后的文本             |
| `LOG_QUEUE_CAPACITY` | 队列容量，建议按文档使用 `8192`                          |
| `BATCH_SIZE`         | 后台线程每次最多取多少条日志，建议 `32`                       |
| `MAX_LINES_PER_FILE` | 单个文件最大行数，达到后切分                               |
| `LOG_XOR_KEY`        | XOR 加密密钥                                     |

你们文档里已经明确 `LogRecord` 保存 `level`、`category` 和 `text[512]`，队列容量、批量大小、切分行数和 XOR 密钥也已经列出来了。

---

# 二、队列模块：`log_queue.h / log_queue.c`

队列模块只负责维护循环队列本身，**不负责加锁**。加锁交给 `logger.c` 和 `log_writer.c`。

需要实现这些函数：

| 函数名               | 功能                                      |
| ----------------- | --------------------------------------- |
| `queue_init`      | 初始化循环队列，把 `front`、`rear`、`count` 设为初始状态 |
| `queue_empty`     | 判断队列是否为空                                |
| `queue_full`      | 判断队列是否已满                                |
| `queue_push`      | 向队尾写入一条 `LogRecord`                     |
| `queue_pop_batch` | 从队头批量取出最多 `BATCH_SIZE` 条日志              |

这里要强调一点：你们文档已经写明，`queue_push` 和 `queue_pop_batch` 调用者必须在外部加锁。

所以队列模块只管数据结构，不管线程同步。

---

# 三、日志接口模块：`logger.h / logger.c`

这个模块是业务代码真正调用的入口。

需要实现这些对外函数：

| 函数名                     | 功能                                        |
| ----------------------- | ----------------------------------------- |
| `log_init`              | 初始化日志系统，包括日志目录、最低日志等级、队列、锁、条件变量、后台写盘线程    |
| `log_app`               | 写应用日志，类别固定为 `APP_LOG`                     |
| `log_operation`         | 写操作日志，类别固定为 `OPERATION_LOG`               |
| `log_error`             | 写错误日志，等级固定为 `LOG_ERROR`，类别通常为 `ERROR_LOG` |
| `log_close`             | 关闭日志系统，通知后台线程退出，并等待剩余日志写完                 |
| `log_get_dropped_count` | 返回因为队列满而被丢弃的日志数量                          |

这些函数在你们文档中已经作为核心接口列出。

`logger.c` 内部还需要一个统一的内部函数：

| 内部函数                 | 功能                                               |
| -------------------- | ------------------------------------------------ |
| `log_write_internal` | 统一处理 `log_app`、`log_operation`、`log_error` 的共同逻辑 |

`log_write_internal` 要完成这些事情：

```text
1. 判断系统是否已经初始化
2. 判断日志等级是否低于 min_level
3. 如果低于等级，直接过滤
4. 格式化日志内容
5. 生成 LogRecord
6. 加锁
7. 判断队列是否满
8. 队列满则 dropped_count++
9. 队列未满则写入队列
10. 通知后台写盘线程
11. 解锁
```

注意：**格式化要在加锁前完成**，否则多个业务线程会长时间抢锁。

---

# 四、写盘模块：`log_writer.h / log_writer.c`

这个模块是后台消费者，负责真正写文件。

需要实现这些函数：

| 函数名                      | 功能                                |
| ------------------------ | --------------------------------- |
| `writer_init`            | 初始化写盘模块，创建日志目录，初始化三类日志文件状态，启动后台线程 |
| `writer_thread_func`     | 后台写盘线程函数，等待通知、批量出队、分类、加密、写入文件     |
| `writer_close`           | 设置停止标志，唤醒后台线程，等待线程退出              |
| `writer_write_record`    | 写入单条日志，内部完成分类、加密、写盘、行数统计          |
| `writer_open_file`       | 打开某一类日志对应的当前文件                    |
| `writer_rotate_file`     | 当前文件达到最大行数后切分到下一个文件               |
| `writer_flush_all`       | 刷新所有已打开的日志文件                      |
| `writer_close_all_files` | 关闭所有日志文件                          |

文档里明确后台线程负责等待条件变量、批量出队、分类、加密、写入 `.log.enc` 文件，并达到阈值后切分。

后台线程的逻辑应该是：

```text
1. 等待队列非空
2. 如果队列为空且 running=1，就继续等待
3. 如果队列为空且 running=0，说明可以退出
4. 如果队列非空，就批量取出日志
5. 释放队列锁
6. 对每条日志进行分类
7. 加密
8. 写入对应文件
9. 检查是否需要切分文件
10. 退出前刷新并关闭所有文件
```

最关键的是：**后台线程取出日志后必须释放锁，再写文件**。你们文档图 1 的说明也强调，队列锁只保护入队和出队，不覆盖 `fprintf`、`fflush`、`fopen`、`fclose` 等慢速磁盘操作。

---

# 五、加密模块：`log_crypto.h / log_crypto.c`

这个模块负责 XOR 加密和解密验证。

需要实现这些函数：

| 函数名                    | 功能                       |
| ---------------------- | ------------------------ |
| `xor_encrypt_to_hex`   | 把明文日志先 XOR 加密，再转成十六进制字符串 |
| `xor_decrypt_from_hex` | 把十六进制密文还原成明文，用于测试和验证     |
| `hex_char_to_value`    | 内部辅助函数，把十六进制字符转成数值       |
| `is_valid_hex_string`  | 检查密文是否是合法十六进制字符串         |

加密模块不要在业务线程里调用，应该在后台写盘线程中调用。你们文档里的整体流程也是：后台线程批量出队后，按类别选择文件，然后 XOR 加密并转十六进制，最后写入 `.log.enc` 文件。

---

# 六、测试业务模块：`test_business.h / test_business.c`

这个模块模拟高并发业务线程。

需要实现这些函数：

| 函数名                     | 功能                       |
| ----------------------- | ------------------------ |
| `run_business_test`     | 创建多个业务线程，启动压测            |
| `business_thread_func`  | 每个业务线程循环产生日志             |
| `test_basic_log`        | 测试普通日志写入                 |
| `test_level_filter`     | 测试日志等级过滤                 |
| `test_file_rotate`      | 测试文件切分                   |
| `test_queue_full`       | 测试队列满和丢弃计数               |
| `test_decrypt_one_line` | 测试从 `.log.enc` 文件读取一行并解密 |

你们文档里默认测试规模是 5 个业务线程，每个线程产生 1000 条日志，总共 5000 次日志调用。

---

# 七、主程序模块：`main.c`

`main.c` 不要写太多逻辑，只负责串流程。

需要实现这些功能：

| 函数/逻辑                      | 功能                      |
| -------------------------- | ----------------------- |
| 调用 `log_init`              | 初始化日志系统                 |
| 调用 `run_business_test`     | 启动并发测试                  |
| 调用 `log_get_dropped_count` | 输出丢弃日志数量                |
| 调用 `log_close`             | 关闭系统，确保剩余日志写完           |
| 可选调用解密测试                   | 验证 `.log.enc` 文件内容可以被还原 |

主流程就是：

```text
初始化日志系统
↓
运行并发测试
↓
打印 dropped_count
↓
关闭日志系统
↓
检查日志文件
```

---

# 八、文件管理要实现的功能

你们最终需要生成三类目录和文件：

| 日志类别 | 目录                  | 文件名                       |
| ---- | ------------------- | ------------------------- |
| 应用日志 | `logs/application/` | `application_001.log.enc` |
| 操作日志 | `logs/operation/`   | `operation_001.log.enc`   |
| 错误日志 | `logs/error/`       | `error_001.log.enc`       |

文档中也明确三类日志分别写入不同目录，并且每一类日志独立计数、独立切分、独立编号。

所以文件管理部分要实现：

```text
1. 创建 logs 根目录
2. 创建 application 子目录
3. 创建 operation 子目录
4. 创建 error 子目录
5. 每类日志维护自己的 file_index
6. 每类日志维护自己的 line_count
7. 达到 MAX_LINES_PER_FILE 后切换到下一个文件
```

---

# 九、关闭流程要实现的功能

`log_close` 不是简单退出，而是要保证日志尽量不丢。

需要实现：

```text
1. 判断系统是否已经初始化
2. 设置 running = 0
3. 唤醒后台线程
4. 后台线程继续处理队列中剩余日志
5. 队列为空后后台线程退出
6. 主线程等待后台线程结束
7. 刷新文件
8. 关闭文件
9. 销毁锁和条件变量
10. 标记系统未初始化
```

文档里也写明 `log_close` 要设置停止标志、唤醒后台线程、等待剩余日志写完，关闭文件并销毁同步对象。

---

# 十、最终实现清单

你们可以直接把下面这个作为小组开发任务表。

| 模块              | 必须实现的函数                                                                                                                                                     |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `log_queue`     | `queue_init`、`queue_empty`、`queue_full`、`queue_push`、`queue_pop_batch`                                                                                      |
| `logger`        | `log_init`、`log_app`、`log_operation`、`log_error`、`log_close`、`log_get_dropped_count`、`log_write_internal`                                                   |
| `log_writer`    | `writer_init`、`writer_thread_func`、`writer_close`、`writer_write_record`、`writer_open_file`、`writer_rotate_file`、`writer_flush_all`、`writer_close_all_files` |
| `log_crypto`    | `xor_encrypt_to_hex`、`xor_decrypt_from_hex`、`hex_char_to_value`、`is_valid_hex_string`                                                                       |
| `test_business` | `run_business_test`、`business_thread_func`、`test_basic_log`、`test_level_filter`、`test_file_rotate`、`test_queue_full`、`test_decrypt_one_line`                |
| `main`          | 初始化、运行测试、打印统计、关闭系统                                                                                                                                          |

你们实现时就按这个顺序来：

```text
1. 先实现 log_common 和 log_queue
2. 再实现 logger 的初始化和写日志接口
3. 再实现后台 writer 线程
4. 再实现分类存储和文件切分
5. 再加入 XOR 加密和解密
6. 最后写 test_business 做并发压测
```

这样分工最清楚，也最适合答辩讲解。
