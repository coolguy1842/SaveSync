#include <settings.hpp>

Settings::Settings() {}
Settings::~Settings() {}

std::string Settings::serverIP() { return "http://192.168.1.4"; }
u16 Settings::serverPort() { return 8000; }

static Settings* settingsInstance = nullptr;
Settings* Settings::instance() {
    if(settingsInstance == nullptr) {
        settingsInstance = new Settings();
    }

    return settingsInstance;
}

void Settings::close() {
    if(settingsInstance == nullptr) {
        return;
    }

    delete settingsInstance;
}