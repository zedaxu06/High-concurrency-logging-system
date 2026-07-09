#pragma once
// 后台写盘模块：按类别分文件、加密、切分
#include "log_common.h"
#include <fstream>
#include <string>
#include <utility>

class LogWriter {
public:
    explicit LogWriter(std::string root = "logs") : root_(std::move(root)) {}

    void init();                       // 创建三类日志目录
    void write(const LogRecord& rec);  // 写入单条（分类 + 加密 + 切分）
    void flush_all();
    void close_all();

private:
    struct FileState {
        std::ofstream ofs;
        int           index = 1;
        int           lines = 0;
        std::string   dir;
        std::string   prefix;
    };

    FileState& state_for(LogCategory c);
    void       open_file(FileState& fs);
    void       rotate(FileState& fs);

    std::string root_;
    FileState   app_, op_, err_;
};
