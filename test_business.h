#pragma once
// 测试业务模块：模拟高并发业务线程 + 单元测试

// 高并发压测：创建 num_threads 个业务线程，每个产生 per_thread 条日志
void run_business_test(int num_threads = 5, int per_thread = 1000);

// 单元测试（返回 true 表示通过）
bool test_basic_log();
bool test_level_filter();
bool test_file_rotate();
bool test_queue_full();
bool test_decrypt_one_line();

// 运行全部单元测试
bool run_all_tests();
