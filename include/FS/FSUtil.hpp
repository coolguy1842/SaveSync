#ifndef __FS_UTIL_HPP__
#define __FS_UTIL_HPP__

#include <3ds.h>
#include <optional>
#include <string>
#include <variant>

struct VarPath {
    VarPath();
    VarPath(FS_Path path);

    VarPath(std::u16string path);
    VarPath(const char16_t* path);

    VarPath(std::string path);
    VarPath(const char* path);

    FS_Path operator()() const;
    FS_Path path;
};

#endif