#include <FS/FSUtil.hpp>

VarPath::VarPath()
    : path(fsMakePath(PATH_EMPTY, nullptr)) {}

VarPath::VarPath(FS_Path p)
    : path(p) {}

VarPath::VarPath(std::u16string p)
    : path(fsMakePath(PATH_UTF16, p.c_str())) {}

VarPath::VarPath(const char16_t* p)
    : path(fsMakePath(PATH_UTF16, p)) {}

VarPath::VarPath(std::string p)
    : path(fsMakePath(PATH_ASCII, p.c_str())) {}

VarPath::VarPath(const char* p)
    : path(fsMakePath(PATH_ASCII, p)) {}

FS_Path VarPath::operator()() const { return path; }