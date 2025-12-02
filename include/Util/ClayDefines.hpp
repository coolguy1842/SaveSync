#ifndef __CLAY_MACROS_HPP__
#define __CLAY_MACROS_HPP__

#define HSPACER_0()    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })
#define HSPACER_1(pix) CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(pix), .height = CLAY_SIZING_FIT() } } })

#define HSPACER_X(x, pix, FUNC, ...) FUNC
#define HSPACER(...)                 HSPACER_X(, ##__VA_ARGS__, HSPACER_1(__VA_ARGS__), HSPACER_0(__VA_ARGS__))

#define VSPACER_0()    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() } } })
#define VSPACER_1(pix) CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIXED(pix) } } })

#define VSPACER_X(x, pix, FUNC, ...) FUNC
#define VSPACER(...)                 VSPACER_X(, ##__VA_ARGS__, VSPACER_1(__VA_ARGS__), VSPACER_0(__VA_ARGS__))

#endif