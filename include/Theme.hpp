#ifndef __THEME_HPP__
#define __THEME_HPP__

#include <clay.h>

#define COLOR(name) Clay_Color name();

namespace Theme {
COLOR(OutOfSync)
COLOR(Bulk)
COLOR(Upload)
COLOR(Download)
COLOR(Unknown)

COLOR(Text)
COLOR(Subtext0)
COLOR(Subtext1)

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