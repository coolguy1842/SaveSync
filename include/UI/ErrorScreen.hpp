#ifndef __ERROR_SCREEN_HPP__
#define __ERROR_SCREEN_HPP__

#include <UI/Screen.hpp>

class ErrorScreen {
public:
    ErrorScreen();
    ~ErrorScreen() = default;

    void update();

    void renderTop();
    void renderBottom();

private:
};

#endif