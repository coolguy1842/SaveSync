#include <Theme.hpp>
#include <UI/SettingsScreen.hpp>
#include <Util/Keyboard.hpp>
#include <clay.h>
#include <clay_renderer_C2D.hpp>
#include <cmath>
#include <regex>

void SettingsScreen::updateDisplayURL() {
    const std::string& url = m_config->serverURL()->value();
    m_displayURL           = url.substr(0, std::min(static_cast<size_t>(50), url.size()));
}

void SettingsScreen::updateDisplayPort() {
    m_displayPort = std::format("{}", m_config->serverPort()->value());
}

SettingsScreen::SettingsScreen(Config* config)
    : m_active(false)
    , m_config(config)
    , m_scrollStart(0.0)
    , m_scrollTarget(0.0)
    , m_scrollCurrent(0.0)
    , m_scrollProgress(1.0) {

    updateDisplayURL();
    updateDisplayPort();

    // m_config->serverURL()->changedEmptySignal()->connect(this, &SettingsScreen::updateDisplayURL);
    // m_config->serverPort()->changedEmptySignal()->connect(this, &SettingsScreen::updateDisplayPort);
}

bool SettingsScreen::isActive() const { return m_active; }
void SettingsScreen::setActive(bool active) {
    m_editingLayout  = false;
    m_selectedLayout = m_config->layout()->value();

    m_scrollCurrent  = m_scrollTarget;
    m_scrollProgress = 1.0;

    m_active = active;
}

constexpr double easeInOutCubic(double x) { return x < 0.5 ? 4 * x * x * x : 1 - std::pow(-2 * x + 2, 3) / 2; }
void SettingsScreen::scrollLeft() {
    double newTarget = std::max(m_scrollTarget - 1.0, 0.0);
    if(newTarget == m_scrollTarget) {
        return;
    }

    if(m_scrollProgress < 0.9) {
        m_bufferedScroll      = Direction::LEFT;
        m_bufferedInputExpire = maxBufferedInputExpire;

        return;
    }
    else if(m_scrollProgress < 1.0) {
        m_scrollStart    = m_scrollCurrent;
        m_scrollProgress = 0.1;
    }
    else {
        m_scrollStart    = m_scrollTarget;
        m_scrollProgress = 0.0;
    }

    m_scrollTarget = newTarget;
}

void SettingsScreen::scrollRight() {
    double newTarget = std::min(m_scrollTarget + 1.0, 2.0);
    if(newTarget == m_scrollTarget) {
        return;
    }

    if(m_scrollProgress < 0.9) {
        m_bufferedScroll      = Direction::RIGHT;
        m_bufferedInputExpire = maxBufferedInputExpire;

        return;
    }
    else if(m_scrollProgress < 1.0) {
        m_scrollStart    = m_scrollCurrent;
        m_scrollProgress = 0.1;
    }
    else {
        m_scrollStart    = m_scrollTarget;
        m_scrollProgress = 0.0;
    }

    m_scrollTarget = newTarget;
}

void SettingsScreen::changeURL() {
    Keyboard keyboard(KeyboardOptions{
        .type     = SWKBD_TYPE_NORMAL,
        .inputLen = 0x100,

        .buttons = {
            .left = KeyboardButton{
                .text   = "Cancel",
                .submit = false,
            },
            .right = KeyboardButton{
                .text   = "Ok",
                .submit = true,
            },
        },
        .validation = {
            .filter = {
                .callback = [](const char** errMsg, const char* input, size_t size) {
                    if(size == 0) {
                        // emulator cancel behaves differently for some reason, dont show error message when cancelling
                        return SWKBD_CALLBACK_OK;
                    }

                    const std::regex urlRegex(R"~(^http:\/\/(?:www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b(?:[-a-zA-Z0-9()@:%_\+.~#?&\/=]*)$)~");
                    if(!std::regex_match(input, urlRegex)) {
                        *errMsg = "Not a valid URL";
                        return SWKBD_CALLBACK_CLOSE;
                    }

                    return SWKBD_CALLBACK_OK;
                },
            },
        },
    });

    if(R_FAILED(keyboard.show(m_config->serverURL()->value())) || keyboard.button() != SWKBD_BUTTON_RIGHT || keyboard.output().empty()) {
        return;
    }

    m_config->serverURL()->setValue(keyboard.output());
}

void SettingsScreen::changePort() {
    Keyboard keyboard(KeyboardOptions{
        .type     = SWKBD_TYPE_NUMPAD,
        .inputLen = 4,

        .buttons = {
            .left = KeyboardButton{
                .text   = "Cancel",
                .submit = false,
            },
            .right = KeyboardButton{
                .text   = "Ok",
                .submit = true,
            },
        },
        .validation = {
            .filter = {
                .callback = [](const char** errMsg, const char* input, size_t size) noexcept -> SwkbdCallbackResult {
                    if(size == 0) {
                        // emulator cancel behaves differently for some reason, dont show error message when cancelling
                        return SWKBD_CALLBACK_OK;
                    }

                    for(size_t i = 0; i < size; i++) {
                        if(!isdigit(input[i])) {
                            *errMsg = "Input can only have digits";
                            return SWKBD_CALLBACK_CLOSE;
                        }
                    }

                    if(std::atoi(input) <= 0) {
                        *errMsg = "Input can't be 0";
                        return SWKBD_CALLBACK_CLOSE;
                    }

                    return SWKBD_CALLBACK_OK;
                },
            },
        },
    });

    if(R_FAILED(keyboard.show(std::format("{}", m_config->serverPort()->value()))) || keyboard.button() != SWKBD_BUTTON_RIGHT || keyboard.output().empty()) {
        return;
    }

    m_config->serverPort()->setValue(std::atoi(keyboard.output().c_str()));
}

void SettingsScreen::update() {
    u32 kDown = hidKeysDown();
    u32 kUp   = hidKeysUp();
    u32 kHeld = hidKeysHeld();

    if(kUp & KEY_A) {
        m_layoutButtonHeld = false;
    }

    if(kDown & KEY_B) {
        if(m_editingLayout) {
            m_editingLayout = false;
        }
        else {
            setActive(false);
        }

        return;
    }

    if(kDown & KEY_Y) {
        setActive(false);
        return;
    }

    if(m_editingLayout) {
        u32 kRepeat = hidKeysDownRepeat();

        if(kDown & KEY_A) {
            m_config->layout()->setValue(m_selectedLayout);
            m_layoutButtonHeld = true;
        }

        if(kRepeat & KEY_LEFT || kRepeat & KEY_RIGHT) {
            m_layoutButtonHeld = false;

            switch(m_selectedLayout) {
            case GRID: m_selectedLayout = LIST; break;
            case LIST: m_selectedLayout = GRID; break;
            default:   break;
            }
        }

        return;
    }

    if(kDown & KEY_A && m_scrollProgress >= 0.9) {
        switch(static_cast<u8>(m_scrollTarget)) {
        case 0:  changeURL(); break;
        case 1:  changePort(); break;
        case 2:  m_editingLayout = true; break;
        default: break;
        }
    }

    bool scrolled = false;

    if(kDown & KEY_LEFT || kHeld & KEY_LEFT) {
        scrollLeft();
        scrolled = true;
    }

    if(kDown & KEY_RIGHT || kHeld & KEY_RIGHT) {
        scrollRight();
        scrolled = true;
    }

    if(!m_editingLayout && !scrolled && m_bufferedInputExpire != 0) {
        if(--m_bufferedInputExpire == 0) {
            switch(m_bufferedScroll) {
            case LEFT:  scrollLeft(); break;
            case RIGHT: scrollRight(); break;
            default:    break;
            }

            m_bufferedScroll = NONE;
        }
    }
}

void SettingsScreen::onButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData) { reinterpret_cast<SettingsScreen*>(userData)->handleButtonHover(elementId, pointerData); }
void SettingsScreen::handleButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData) {
    static bool clickingLayoutButton = false;
    if(elementId.id == CLAY_ID("URL").id && pointerData.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
        changeURL();
    }
    else if(elementId.id == CLAY_ID("Port").id && pointerData.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
        changePort();
    }
    else if(elementId.id == CLAY_ID("Layout").id && pointerData.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
        m_selectedLayout = m_config->layout()->value();
        m_editingLayout  = true;
    }
    else if(elementId.id == CLAY_ID("Grid").id) {
        switch(pointerData.state) {
        case CLAY_POINTER_DATA_PRESSED_THIS_FRAME:
        case CLAY_POINTER_DATA_PRESSED:
            m_selectedLayout     = GRID;
            clickingLayoutButton = true;
            break;
        case CLAY_POINTER_DATA_RELEASED_THIS_FRAME:
            m_config->layout()->setValue(GRID);
            // fall through
        case CLAY_POINTER_DATA_RELEASED:
            clickingLayoutButton = false;
            break;
        default: break;
        }
    }
    else if(elementId.id == CLAY_ID("List").id) {
        switch(pointerData.state) {
        case CLAY_POINTER_DATA_PRESSED_THIS_FRAME:
        case CLAY_POINTER_DATA_PRESSED:
            m_selectedLayout     = LIST;
            clickingLayoutButton = true;
            break;
        case CLAY_POINTER_DATA_RELEASED_THIS_FRAME:
            m_config->layout()->setValue(LIST);
            // fall through
        case CLAY_POINTER_DATA_RELEASED:
            clickingLayoutButton = false;
            break;
        default: break;
        }
    }
    else if(elementId.id == CLAY_ID("LayoutOptions").id) {
        switch(pointerData.state) {
        case CLAY_POINTER_DATA_PRESSED_THIS_FRAME: clickingLayoutButton = false; break;
        case CLAY_POINTER_DATA_RELEASED_THIS_FRAME:
            if(!clickingLayoutButton) {
                m_editingLayout = false;
            }

            break;
        default: break;
        }
    }
    else if(elementId.id == CLAY_ID("Exit").id && pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        setActive(false);
    }
}

void SettingsScreen::renderTop() {}
void SettingsScreen::renderBottom() {
    if(m_scrollProgress != 1.0) {
        m_scrollCurrent  = m_scrollStart + easeInOutCubic(m_scrollProgress) * (m_scrollTarget - m_scrollStart);
        m_scrollProgress = std::min(m_scrollProgress + 0.05, 1.0);
    }

    CLAY(
        CLAY_ID("Body"),
        {
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_FIXED(BOTTOM_SCREEN_WIDTH),
                    .height = CLAY_SIZING_FIXED(BOTTOM_SCREEN_HEIGHT),
                },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = Theme::Background(),
        }
    ) {
        CLAY(
            CLAY_ID("Header"),
            {
                .layout = {
                    .sizing         = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(16) },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            CLAY_TEXT(CLAY_STRING("Settings"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));
        }

        CLAY(
            CLAY_ID("Options"),
            {
                .layout = { .sizing = { .width = CLAY_SIZING_FIXED(BOTTOM_SCREEN_WIDTH), .height = CLAY_SIZING_GROW() } },

                .clip = {
                    .horizontal  = true,
                    .vertical    = true,
                    .childOffset = {
                        .x = static_cast<float>(m_scrollCurrent) * -BOTTOM_SCREEN_WIDTH,
                    },
                },
            }
        ) {
#define OPTION(id, display, ...)                                                                                     \
    CLAY_AUTO_ID({                                                                                                   \
        .layout = {                                                                                                  \
            .sizing = {                                                                                              \
                .width  = CLAY_SIZING_FIXED(BOTTOM_SCREEN_WIDTH),                                                    \
                .height = CLAY_SIZING_GROW(),                                                                        \
            },                                                                                                       \
            .childGap       = 8,                                                                                     \
            .childAlignment = {                                                                                      \
                .x = CLAY_ALIGN_X_CENTER,                                                                            \
                .y = CLAY_ALIGN_Y_CENTER,                                                                            \
            },                                                                                                       \
            .layoutDirection = CLAY_TOP_TO_BOTTOM,                                                                   \
        },                                                                                                           \
    }) {                                                                                                             \
        CLAY(                                                                                                        \
            CLAY_ID(id),                                                                                             \
            {                                                                                                        \
                .layout          = { .padding = CLAY_PADDING_ALL(4) },                                               \
                .backgroundColor = Clay_Hovered() ? Theme::ButtonPressed() : Theme::Button(),                        \
                .cornerRadius    = CLAY_CORNER_RADIUS(4),                                                            \
            }                                                                                                        \
        ) {                                                                                                          \
            Clay_OnHover(&SettingsScreen::onButtonHover, reinterpret_cast<intptr_t>(this));                          \
            CLAY_TEXT(CLAY_STRING(display), CLAY_TEXT_CONFIG({ .textColor = Theme::ButtonText(), .fontSize = 14 })); \
        }                                                                                                            \
                                                                                                                     \
        __VA_ARGS__                                                                                                  \
    }

            OPTION("URL", "Server URL", {
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_PERCENT(0.8) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } }, .clip = { .horizontal = true } }) {
                    Clay_String str = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_displayURL.size()), .chars = m_displayURL.c_str() };
                    CLAY_TEXT(str, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12 }));
                };
            })

            OPTION("Port", "Server Port", {
                Clay_String str = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_displayPort.size()), .chars = m_displayPort.c_str() };
                CLAY_TEXT(str, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_WORDS }));
            })

            OPTION("Layout", "Layout")

#undef OPTION

            const uint16_t hoverPad        = 1;
            const Clay_Padding leftDefault = { 2, 8, 12, 12 };
            const Clay_Padding leftHovered = {
                static_cast<uint16_t>(leftDefault.left + hoverPad),
                static_cast<uint16_t>(leftDefault.right + hoverPad),
                static_cast<uint16_t>(leftDefault.top + hoverPad),
                static_cast<uint16_t>(leftDefault.bottom + hoverPad)
            };

            const Clay_SizingAxis triangleSize = CLAY_SIZING_FIXED(10);

            CLAY(
                CLAY_ID("PreviousOption"),
                {
                    .layout = {
                        .padding        = Clay_Hovered() ? leftHovered : leftDefault,
                        .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = Theme::Surface2(),
                    .cornerRadius    = { 0, 12, 0, 12 },

                    .floating = {
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_LEFT_CENTER,
                            .parent  = CLAY_ATTACH_POINT_LEFT_CENTER,
                        },
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                }
            ) {
                if(Clay_Hovered()) {
                    scrollLeft();
                }

                static CustomElementData leftPolygon = {
                    .type    = CUSTOM_ELEMENT_TYPE_POLYGON,
                    .polygon = {
                        .a = { 1.0f, 0.0f },
                        .b = { 0.0f, 0.5f },
                        .c = { 1.0f, 1.0f },
                    },
                };

                Clay_Color background = m_scrollTarget <= SCROLL_MIN ? Theme::ButtonDisabled() : Theme::Text();
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = triangleSize, .height = triangleSize } }, .backgroundColor = background, .custom = { .customData = &leftPolygon } });
            }

            const Clay_Padding rightDefault = { leftDefault.right, leftDefault.left, leftDefault.top, leftDefault.bottom };
            const Clay_Padding rightHovered = { leftHovered.right, leftHovered.left, leftHovered.top, leftHovered.bottom };

            CLAY(
                CLAY_ID("NextOption"),
                {
                    .layout = {
                        .padding        = Clay_Hovered() ? rightHovered : rightDefault,
                        .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = Theme::Surface2(),
                    .cornerRadius    = { 12, 0, 12, 0 },

                    .floating = {
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_RIGHT_CENTER,
                            .parent  = CLAY_ATTACH_POINT_RIGHT_CENTER,
                        },
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                }
            ) {
                if(Clay_Hovered()) {
                    scrollRight();
                }

                static CustomElementData rightPolygon = {
                    .type    = CUSTOM_ELEMENT_TYPE_POLYGON,
                    .polygon = {
                        .a = { 0.0f, 0.0f },
                        .b = { 1.0f, 0.5f },
                        .c = { 0.0f, 1.0f },
                    },
                };

                Clay_Color background = m_scrollTarget >= SCROLL_MAX ? Theme::ButtonDisabled() : Theme::Text();
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = triangleSize, .height = triangleSize } }, .backgroundColor = background, .custom = { .customData = &rightPolygon } });
            }
        }

        CLAY(
            CLAY_ID("Footer"),
            {
                .layout = {
                    .sizing         = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(16) },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            CLAY(CLAY_ID("Exit")) {
                Clay_OnHover(&SettingsScreen::onButtonHover, reinterpret_cast<intptr_t>(this));
                CLAY_TEXT(CLAY_STRING("Exit with \uE003 or \uE001"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));
            }
        }

        if(m_editingLayout) {
            CLAY(
                CLAY_ID("LayoutOptions"),
                {
                    .layout = {
                        .sizing         = { .width = CLAY_SIZING_PERCENT(1.0), .height = CLAY_SIZING_PERCENT(1.0) },
                        .childGap       = 20,
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = { 0x00, 0x00, 0x00, 0xB2 },

                    .floating = {
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_LEFT_TOP,
                            .parent  = CLAY_ATTACH_POINT_LEFT_TOP,
                        },
                        .attachTo = CLAY_ATTACH_TO_PARENT,
                    },
                }
            ) {
                Clay_OnHover(&SettingsScreen::onButtonHover, reinterpret_cast<intptr_t>(this));
                const u16 gridGap = 2;

                Clay_BorderElementConfig notSelectedBorder, selectedBorder = {
                    .color = Theme::ButtonText(),
                    .width = CLAY_BORDER_OUTSIDE(1)
                };

                Clay_Sizing buttonSizing        = { .width = CLAY_SIZING_FIXED(48), .height = CLAY_SIZING_FIXED(48) };
                Clay_Sizing buttonPressedSizing = { .width = CLAY_SIZING_FIXED(50), .height = CLAY_SIZING_FIXED(50) };

                CLAY(
                    CLAY_ID("Grid"),
                    {
                        .layout = {
                            .sizing          = (Clay_Hovered() || (m_selectedLayout == GRID && m_layoutButtonHeld)) ? buttonPressedSizing : buttonSizing,
                            .padding         = CLAY_PADDING_ALL(3),
                            .childGap        = gridGap,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = (Clay_Hovered() || (m_selectedLayout == GRID && m_layoutButtonHeld)) ? Theme::ButtonPressed() : Theme::Button(),
                        .cornerRadius    = CLAY_CORNER_RADIUS(4),
                        .border          = m_selectedLayout == GRID ? selectedBorder : notSelectedBorder,
                    }
                ) {
                    Clay_OnHover(&SettingsScreen::onButtonHover, reinterpret_cast<intptr_t>(this));

                    for(u8 i = 0; i < 2; i++) {
                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() }, .childGap = gridGap } }) {
                            for(u8 j = 0; j < 2; j++) {
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } }, .backgroundColor = Theme::Surface1(), .cornerRadius = CLAY_CORNER_RADIUS(2) });
                            }
                        }
                    }
                }

                CLAY(
                    CLAY_ID("List"),
                    {
                        .layout = {
                            .sizing          = (Clay_Hovered() || (m_selectedLayout == LIST && m_layoutButtonHeld)) ? buttonPressedSizing : buttonSizing,
                            .padding         = CLAY_PADDING_ALL(3),
                            .childGap        = 4,
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = (Clay_Hovered() || (m_selectedLayout == LIST && m_layoutButtonHeld)) ? Theme::ButtonPressed() : Theme::Button(),
                        .cornerRadius    = CLAY_CORNER_RADIUS(4),
                        .border          = m_selectedLayout == LIST ? selectedBorder : notSelectedBorder,
                    }
                ) {
                    Clay_OnHover(&SettingsScreen::onButtonHover, reinterpret_cast<intptr_t>(this));

                    for(u8 i = 0; i < 3; i++) {
                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() }, .childGap = 2, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }) {
                            static CustomElementData circleData = {
                                .type = CUSTOM_ELEMENT_TYPE_CIRCLE
                            };

                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(11), .height = CLAY_SIZING_FIXED(11) } }, .backgroundColor = Theme::Surface1(), .custom = { .customData = &circleData } });
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } }, .backgroundColor = Theme::Surface1(), .cornerRadius = CLAY_CORNER_RADIUS(2) });
                        }
                    }
                }
            }
        }
    }
}