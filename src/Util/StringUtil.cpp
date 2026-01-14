#include <Util/StringUtil.hpp>
#include <cmath>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convertor;
#pragma GCC diagnostic pop

std::string StringUtil::toUTF8(const std::u16string& str) { return convertor.to_bytes(str); }
std::string StringUtil::toUTF8(const u16* str) { return toUTF8(reinterpret_cast<const char16_t*>(str)); }

std::u16string StringUtil::fromUTF8(const std::string& source) { return convertor.from_bytes(source); }

int StringUtil::formatFileSize(u64 fileSize, char* buf, size_t bufSize) {
    // https://stackoverflow.com/a/77278639
    u64 usedSize            = fileSize;
    const char* sizeNames[] = { "B", "KB", "MB", "GB", "TB" };

    size_t i        = static_cast<size_t>(floor(log(usedSize) / log(1024)));
    float humanSize = static_cast<float>(usedSize) / pow(1024, i);

    return snprintf(buf, bufSize, "%.1f %s", humanSize, sizeNames[i]);
}