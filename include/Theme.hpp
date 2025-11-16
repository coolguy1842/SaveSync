#ifndef __THEME_HPP__
#define __THEME_HPP__

#include <clay.h>

#define COLOR(name) Clay_Color name();

namespace Theme {
COLOR(Save)
COLOR(SaveAndExtdata)
COLOR(Unknown)
COLOR(Extdata)

COLOR(Text)
COLOR(Subtext0)

COLOR(Red)
COLOR(Green)

COLOR(Accent)

COLOR(Button)
COLOR(ButtonText)
COLOR(ButtonPressed)
COLOR(ButtonDisabled)

COLOR(Surface0)
COLOR(Surface1)
COLOR(Surface2)
COLOR(Background)
}; // namespace Theme

#undef COLOR

#endif