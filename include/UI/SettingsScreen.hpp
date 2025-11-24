#ifndef __SettingsScreen_SCREEN_HPP__
#define __SettingsScreen_SCREEN_HPP__

#include <Config.hpp>
#include <UI/Screen.hpp>
#include <clay.h>
#include <memory>

class SettingsScreen : public Screen {
public:
    SettingsScreen(Config* config);

    void update();

    void renderTop();
    void renderBottom();

    bool isActive() const;
    void setActive(bool active = true);

private:
    enum Direction {
        NONE,
        LEFT,
        RIGHT
    };

    void updateDisplayURL();
    void updateDisplayPort();

    void changeURL();
    void changePort();
    void changeLayout();

    void handleButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData);
    static void onButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData);

    void scrollLeft();
    void scrollRight();

    bool m_active;
    std::shared_ptr<Config> m_config;

    bool m_editingLayout = false;
    std::string m_displayURL;
    std::string m_displayPort;

    Layout m_selectedLayout;
    bool m_layoutButtonHeld = false;

    Direction m_bufferedScroll      = NONE;
    u8 m_bufferedInputExpire        = 0;
    const u8 maxBufferedInputExpire = 2;

    const double SCROLL_MIN = 0.0f;
    const double SCROLL_MAX = 2.0f;

    double m_scrollStart;
    double m_scrollTarget;
    double m_scrollCurrent;
    double m_scrollProgress;
};

#endif