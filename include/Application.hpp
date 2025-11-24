#ifndef __APPLICATION_HPP__
#define __APPLICATION_HPP__

#include <Client.hpp>
#include <Config.hpp>
#include <TitleLoader.hpp>
#include <UI/MainScreen.hpp>
#include <UI/SettingsScreen.hpp>
#include <array>
#include <clay_renderer_C2D.hpp>

class Application {
public:
    Application(bool consoleEnabled = false, gfxScreen_t consoleScreen = GFX_BOTTOM);
    ~Application();

    void update();
    void render();

    bool loop();

    bool shouldExit() const;
    void setShouldExit(bool shouldExit = true);

    void setConsole(bool enabled, gfxScreen_t screen);

private:
    void updateTitlesOutOfDate();
    void updateTitleOutOfDate(std::shared_ptr<Title> title, Container container);

    void tryUpdateClientURL(size_t, bool);

private:
    void updateURL();
    void initClay();

    bool m_shouldExit;

    Config m_config;
    TitleLoader m_loader;
    std::shared_ptr<Client> m_client;

    std::unique_ptr<MainScreen> m_mainScreen;

    Clay_Context* m_topContext    = nullptr;
    Clay_Context* m_bottomContext = nullptr;

    void* m_clayTopMemory    = nullptr;
    void* m_clayBottomMemory = nullptr;
    Clay_C2DRendererData m_rendererData;

    C3D_RenderTarget* m_top    = nullptr;
    C3D_RenderTarget* m_bottom = nullptr;

    touchPosition m_prevTouch;

    bool m_consoleEnabled;
    bool m_consoleInitialized;
    gfxScreen_t m_consoleScreen;

    u64 m_prevTime;

    std::string m_pendingURL;

#ifdef DEBUG
    PrintConsole m_console;
    // std::array<u16, TOP_SCREEN_WIDTH * TOP_SCREEN_HEIGHT> m_dummyTopFramebuffer;
    // std::array<u16, BOTTOM_SCREEN_WIDTH * BOTTOM_SCREEN_HEIGHT> m_dummyBottomFramebuffer;

    std::unique_ptr<u16> m_dummyTopFramebuffer;
    std::unique_ptr<u16> m_dummyBottomFramebuffer;

#ifdef REDIRECT_CONSOLE
    int stdoutDup;
    int stderrDup;
#endif
#endif
};

#endif