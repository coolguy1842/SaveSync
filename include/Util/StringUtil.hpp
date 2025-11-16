#ifndef __STRING_UTIL_HPP__
#define __STRING_UTIL_HPP__

#include <3ds.h>

#include <codecvt>
#include <locale>
#include <string>

namespace StringUtil {
std::string toUTF8(const std::u16string& str);
std::string toUTF8(const u16* str);

std::u16string fromUTF8(const std::string& str);

// https://stackoverflow.com/a/7869639
constexpr u32 hash(const char* s, size_t off = 0) { return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off]; }

}; // namespace StringUtil

constexpr u32 operator""_h(const char* str, size_t) { return StringUtil::hash(str); }

#endif