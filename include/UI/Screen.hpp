#ifndef __SCREEN_HPP__
#define __SCREEN_HPP__

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

class Screen {
public:
    Screen()          = default;
    virtual ~Screen() = default;

    virtual void update() = 0;

    // add clay elements, this is inside BeginLayout
    virtual void renderTop()    = 0;
    virtual void renderBottom() = 0;

private:
};

#endif