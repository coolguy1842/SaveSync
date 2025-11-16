#include <Util/StringUtil.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convertor;
#pragma GCC diagnostic pop

std::string StringUtil::toUTF8(const std::u16string& str) { return convertor.to_bytes(str); }
std::string StringUtil::toUTF8(const u16* str) { return toUTF8(reinterpret_cast<const char16_t*>(str)); }

std::u16string StringUtil::fromUTF8(const std::string& source) { return convertor.from_bytes(source); }
