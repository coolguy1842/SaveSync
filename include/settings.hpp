#ifndef __SETTINGS_HPP__
#define __SETTINGS_HPP__

#include <3ds.h>

#include <string>

class Settings {
public:
    ~Settings();

    static Settings* instance();
    static void close();

    std::string serverIP();
    u16 serverPort();

private:
    Settings();
};

#endif