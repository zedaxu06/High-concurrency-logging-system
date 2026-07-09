#include "log_crypto.h"

static const char* HEX = "0123456789abcdef";

std::string xor_encrypt_to_hex(const std::string& plain, unsigned char key) {
    std::string out;
    out.reserve(plain.size() * 2);
    for (unsigned char ch : plain) {
        unsigned char e = static_cast<unsigned char>(ch ^ key);
        out.push_back(HEX[(e >> 4) & 0xF]);
        out.push_back(HEX[e & 0xF]);
    }
    return out;
}

int hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool is_valid_hex_string(const std::string& s) {
    if (s.empty() || (s.size() % 2) != 0) return false;
    for (char c : s)
        if (hex_char_to_value(c) < 0) return false;
    return true;
}

std::string xor_decrypt_from_hex(const std::string& hex, unsigned char key) {
    if (!is_valid_hex_string(hex)) return {};
    std::string out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        int hi = hex_char_to_value(hex[i]);
        int lo = hex_char_to_value(hex[i + 1]);
        unsigned char e = static_cast<unsigned char>((hi << 4) | lo);
        out.push_back(static_cast<char>(e ^ key));
    }
    return out;
}
