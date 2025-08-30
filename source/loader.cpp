#include <loader.hpp>

static TitleLoader* _instance = nullptr;
TitleLoader* TitleLoader::instance() {
    if(_instance == nullptr) {
        _instance = new TitleLoader();
    }

    return _instance;
}

void TitleLoader::closeInstance() {
    if(_instance != nullptr) {
        delete _instance;
    }
}