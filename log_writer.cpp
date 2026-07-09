#include "log_writer.h"
#include "log_crypto.h"
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

void LogWriter::init() {
    app_.dir = root_ + "/application"; app_.prefix = "application";
    op_.dir  = root_ + "/operation";   op_.prefix  = "operation";
    err_.dir = root_ + "/error";       err_.prefix = "error";
    fs::create_directories(app_.dir);
    fs::create_directories(op_.dir);
    fs::create_directories(err_.dir);
}

LogWriter::FileState& LogWriter::state_for(LogCategory c) {
    switch (c) {
        case LogCategory::APP:       return app_;
        case LogCategory::OPERATION: return op_;
        case LogCategory::ERROR:     return err_;
    }
    return app_;
}

static std::string file_path(const std::string& dir, const std::string& prefix, int index) {
    char name[64];
    std::snprintf(name, sizeof name, "%s_%03d.log.enc", prefix.c_str(), index);
    return dir + "/" + name;
}

void LogWriter::open_file(FileState& st) {
    std::string path = file_path(st.dir, st.prefix, st.index);
    st.ofs.open(path, std::ios::out | std::ios::trunc);
    st.lines = 0;
}

void LogWriter::rotate(FileState& st) {
    if (st.ofs.is_open()) { st.ofs.flush(); st.ofs.close(); }
    ++st.index;
    open_file(st);
}

void LogWriter::write(const LogRecord& rec) {
    FileState& st = state_for(rec.category);
    if (!st.ofs.is_open()) open_file(st);
    std::string enc = xor_encrypt_to_hex(rec.text);   // 后台线程中加密
    st.ofs << enc << '\n';
    if (++st.lines >= (int)MAX_LINES_PER_FILE) rotate(st);   // 达到上限后切分
}

void LogWriter::flush_all() {
    if (app_.ofs.is_open()) app_.ofs.flush();
    if (op_.ofs.is_open())  op_.ofs.flush();
    if (err_.ofs.is_open()) err_.ofs.flush();
}

void LogWriter::close_all() {
    if (app_.ofs.is_open()) { app_.ofs.flush(); app_.ofs.close(); }
    if (op_.ofs.is_open())  { op_.ofs.flush(); op_.ofs.close(); }
    if (err_.ofs.is_open()) { err_.ofs.flush(); err_.ofs.close(); }
}
