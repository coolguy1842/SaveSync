#ifndef __LOGGER_HPP__
#define __LOGGER_HPP__

#include <FS/File.hpp>
#include <Util/Defines.hpp>
#include <Util/Mutex.hpp>
#include <format>
#include <memory>
#include <stdio.h>
#include <string>

// probably should clean this up sometime
class Logger {
public:
    template<typename... Args>
    using format_string = std::basic_format_string<char, std::type_identity_t<Args>...>;

#define LOGGER(name, level)                                                                                                                                    \
    template<typename... Args>                                                                                                                                 \
    static inline void name(std::string module, format_string<Args...> fmt, Args&&... args) { logMessageLM(level, module, fmt, std::forward<Args>(args)...); } \
    template<typename... Args>                                                                                                                                 \
    static inline void name##NoModule(format_string<Args...> fmt, Args&&... args) { logMessageL(level, fmt, std::forward<Args>(args)...); }                    \
                                                                                                                                                               \
    static inline void name(std::string module, Result res) { logMessageLM(level, module, "{}", formatResult(res)); }                                          \
    static inline void name##NoModule(Result res) { logMessageL(level, "{}", formatResult(res)); }

    template<typename... Args>
    static inline void log(std::string module, format_string<Args...> fmt, Args&&... args) { logMessageM(module, fmt, std::forward<Args>(args)...); }
    template<typename... Args>
    static inline void log(format_string<Args...> fmt, Args&&... args) { logMessage(fmt, std::forward<Args>(args)...); }

    static inline void log(std::string module, Result res) { logMessageM(module, "{}", formatResult(res)); }
    static inline void log(Result res) { logMessage("{}", formatResult(res)); }

    LOGGER(info, INFO)
    LOGGER(warn, WARN)
    LOGGER(error, ERROR)
    LOGGER(critical, CRITICAL)

    static void logProfiler();
    static void closeLogFile();

private:
    enum Level {
        INFO,
        WARN,
        ERROR,
        CRITICAL
    };

    static std::string formatResult(Result res);
    static const char* levelColor(Level level);
    static const char* levelName(Level level);
    static std::shared_ptr<File> openLogFile();

    template<typename... Args>
    static inline void logMessageLM(Level level, std::string module, format_string<Args...> fmt, Args&&... args) {
        fileLogMessage(std::format("{} [{}] {}\n", levelName(level), module, std::format(fmt, std::forward<Args>(args)...)));
#if defined(DEBUG)
        printf("%s%s%s [%s] %s\n", levelColor(level), levelName(level), resetColor, module.c_str(), std::format(fmt, std::forward<Args>(args)...).c_str());
#endif
    }

    template<typename... Args>
    static inline void logMessageL(Level level, format_string<Args...> fmt, Args&&... args) {
        fileLogMessage(std::format("{} {}\n", levelName(level), std::format(fmt, std::forward<Args>(args)...)));
#if defined(DEBUG)
        printf("%s%s%s %s\n", levelColor(level), levelName(level), resetColor, std::format(fmt, std::forward<Args>(args)...).c_str());
#endif
    }

    template<typename... Args>
    static inline void logMessageM(std::string module, format_string<Args...> fmt, Args&&... args) {
        fileLogMessage(std::format("[{}] {}\n", module.c_str(), std::format(fmt, std::forward<Args>(args)...)));
#if defined(DEBUG)
        printf("[%s] %s\n", module.c_str(), std::format(fmt, std::forward<Args>(args)...).c_str());
#endif
    }

    template<typename... Args>
    static inline void logMessage(format_string<Args...> fmt, Args&&... args) {
        fileLogMessage(std::format(fmt, std::forward<Args>(args)...) + "\n");
#if defined(DEBUG)
        printf("%s\n", std::format(fmt, std::forward<Args>(args)...).c_str());
#endif
    }

    static void fileLogMessage(const std::string& message);

    static constexpr const char* infoColor     = "\033[32m";
    static constexpr const char* warnColor     = "\033[33m";
    static constexpr const char* errorColor    = "\033[31m";
    static constexpr const char* criticalColor = "\033[31m";

    static constexpr const char* resetColor = "\033[0m";

    static bool s_dirInitialized;
    static bool s_dirExists;

    static Mutex s_fileMutex;
    static std::shared_ptr<File> s_file;
};

#endif