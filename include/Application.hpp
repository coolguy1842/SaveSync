#ifndef __APPLICATION_HPP__
#define __APPLICATION_HPP__

#include <Client.hpp>
#include <Config.hpp>
#include <TitleLoader.hpp>
#include <UI/MainScreen.hpp>
#include <clay_renderer_C2D.hpp>
#include <memory>
#include <rocket.hpp>

class Application : rocket::trackable {
public:
    static constexpr gfxScreen_t DefaultScreen = GFX_BOTTOM;
    Application(bool consoleEnabled = false, gfxScreen_t consoleScreen = DefaultScreen);
    ~Application();

    void update();
    void render();

    bool loop();

    bool shouldExit() const;
    void setShouldExit(bool shouldExit = true);

private:
    struct ExceptionData {
        Application* context;
        ERRF_ExceptionData data;
    };

    void cleanup();

    static void onException(ERRF_ExceptionInfo* excep, CpuRegisters* regs);
    void handleException(ERRF_ExceptionInfo* excep, CpuRegisters* regs);

    void updateURL();
    void tryUpdateClientURL(bool processing);

    void checkTitleOutOfDate(std::shared_ptr<Title> title, Container redundant = SAVE);
    void checkTitlesOutOfDate();

    void initClay();

    bool m_shouldExit;

    std::shared_ptr<Config> m_config;
    std::shared_ptr<TitleLoader> m_loader;
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
    u64 m_prevTime;

    std::string m_pendingURL;

    ExceptionData m_exceptionData;
    void* m_exceptionHandlerStack = nullptr;

    bool m_handlingException = false;
    rocket::scoped_connection_container m_connections;

    bool m_consoleEnabled;
    gfxScreen_t m_consoleScreen;

#if defined(DEBUG)
#if !defined(REDIRECT_CONSOLE)
    void setConsole(bool enabled, gfxScreen_t screen);

    PrintConsole m_console;
    bool m_consoleInitialized = false;

    std::vector<u16> m_dummyTopFramebuffer;
    std::vector<u16> m_dummyBottomFramebuffer;
#else
    int m_stdoutDup;
    int m_stderrDup;
#endif
#endif
};

#endif