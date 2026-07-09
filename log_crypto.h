#pragma once
// XOR 加密 / 十六进制编码模块
#include <string>
#include "log_common.h"

// 明文 -> XOR -> 十六进制字符串
std::string xor_encrypt_to_hex(const std::string& plain, unsigned char key = LOG_XOR_KEY);
// 十六进制字符串 -> 还原明文（用于测试与验证）
std::string xor_decrypt_from_hex(const std::string& hex, unsigned char key = LOG_XOR_KEY);
// 校验是否为合法十六进制字符串
bool        is_valid_hex_string(const std::string& s);
// 单个十六进制字符转数值，非法返回 -1
int         hex_char_to_value(char c);
