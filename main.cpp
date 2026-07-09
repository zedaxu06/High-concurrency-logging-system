#include "logger.h"
#include "test_business.h"
#include "benchmark.h"
#include "log_crypto.h"

#include <clocale>
#include <cstdio>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

static void init_console_utf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    std::setlocale(LC_ALL, ".UTF8");
}

// 从应用日志读取一行密文并解密，验证 .log.enc 内容可还原
static void demo_decrypt_preview() {
    std::ifstream in("logs/application/application_001.log.enc");
    if (!in) return;
    std::string line;
    if (std::getline(in, line)) {
        std::printf("\n[解密预览] 密文行: %.40s...\n", line.c_str());
        std::printf("[解密预览] 明文:   %s\n", xor_decrypt_from_hex(line).c_str());
    }
}

int main(int argc, char** argv) {
    init_console_utf8();

    std::string mode = (argc > 1) ? argv[1] : "run";

    if (mode == "bench") {
        bench_compare({1, 4, 8, 16}, 20000);
        return 0;
    }
    if (mode == "test") {
        return run_all_tests() ? 0 : 1;
    }

    // 默认：基础功能演示（预案主流程）
    std::printf("==== 高并发异步日志系统 ====\n");
    log_init("logs", LogLevel::DEBUG, QueueKind::Mutex);

    std::printf("启动 5 个业务线程，每线程 1000 条日志...\n");
    run_business_test(5, 1000);

    std::printf("因队列满被丢弃的日志数: %llu\n",
                (unsigned long long)log_get_dropped_count());

    log_close();
    std::printf("日志系统已关闭，剩余日志已写完。\n");

    demo_decrypt_preview();
    run_all_tests();
    return 0;
}
