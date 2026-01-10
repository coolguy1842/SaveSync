#include <Application.hpp>
#include <Debug/ExceptionHandler.hpp>
#include <Debug/Logger.hpp>
#include <Debug/Profiler.hpp>
#include <clay_renderer_C2D.hpp>

void Application::updateURL() {
    std::string url = std::format("{}:{}", m_config->serverURL()->value(), m_config->serverPort()->value());
    if(m_client->processingQueueRequest()) {
        m_pendingURL = url;
        return;
    }

    m_client->setURL(url);
    m_pendingURL.clear();
}

void Application::tryUpdateClientURL(bool processing) {
    if(m_pendingURL.empty() || processing) {
        return;
    }

    m_client->setURL(m_pendingURL);
    m_pendingURL.clear();
}

bool filesEqual(std::vector<FileInfo> a, std::vector<FileInfo> b) { return std::equal(a.begin(), a.end(), b.begin(), b.end()); }
void Application::checkTitleOutOfDate(std::shared_ptr<Title> title, Container) {
    if(!m_client->cachedTitleInfoLoaded()) {
        title->setOutOfDate(0);
        return;
    }

    const std::unordered_map<u64, TitleInfo> cache = m_client->cachedTitleInfo();

    auto it = cache.find(title->id());
    if(it == cache.end()) {
        title->setOutOfDate((title->getContainerFiles(SAVE).empty() ? 0 : SAVE) | (title->getContainerFiles(EXTDATA).empty() ? 0 : EXTDATA));
        return;
    }

    u8 outOfDate = 0;

    TitleInfo info = it->second;
    if(!filesEqual(title->getContainerFiles(SAVE), it->second.save)) {
        outOfDate |= SAVE;
    }

    if(!filesEqual(title->getContainerFiles(EXTDATA), it->second.extdata)) {
        outOfDate |= EXTDATA;
    }

    title->setOutOfDate(outOfDate);
}

void Application::checkTitlesOutOfDate() {
    for(auto title : m_loader->titles()) {
        checkTitleOutOfDate(title);
    }
}

void HandleClayErrors(Clay_ErrorData errorData) { Logger::error("Clay", "{}", errorData.errorText.chars); }
void Application::initClay() {
    Logger::info("App Init Clay", "Creating Clay Instances");
    size_t minMemorySize = Clay_MinMemorySize();

    m_clayTopMemory    = linearAlloc(minMemorySize);
    m_clayBottomMemory = linearAlloc(minMemorySize);

    m_rendererData = { .fonts = { C2D_FontLoadSystem(CFG_REGION_USA) } };

    if(m_clayTopMemory == NULL) {
        Logger::critical("App Init Clay", "Failed to create top memory");
    }
    else {
        m_topContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayTopMemory), { TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT }, { HandleClayErrors });
        Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);
    }

    if(m_clayBottomMemory == NULL) {
        Logger::critical("App Init Clay", "Failed to create bottom memory");
    }
    else {
        m_bottomContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayBottomMemory), { BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT }, { HandleClayErrors });
        Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);
    }
}

Application::Application(bool consoleEnabled, gfxScreen_t consoleScreen)
    : m_shouldExit(false)
    , m_consoleEnabled(false)
    , m_consoleScreen(DefaultScreen)
#if defined(DEBUG)
    , m_dummyTopFramebuffer(TOP_SCREEN_WIDTH * TOP_SCREEN_HEIGHT)
    , m_dummyBottomFramebuffer(BOTTOM_SCREEN_WIDTH * BOTTOM_SCREEN_HEIGHT)
#endif
{
    PROFILE_SCOPE("App Init");

    (void)consoleEnabled;
    (void)consoleScreen;
    if((m_exceptionHandlerStack = allocateHandlerStack(0x1000)) != nullptr) {
        threadOnException(DefaultExceptionHandler, m_exceptionHandlerStack, WRITE_DATA_TO_HANDLER_STACK);
    }
    else {
        Logger::warn("App Init", "Failed to allocate stack for exception handler");
    }

    C2D_Clay_Init();

#ifdef DEBUG
    if(consoleEnabled) {
        setConsole(consoleEnabled, consoleScreen);
    }
    else {
        setConsole(true, DefaultScreen);
        setConsole(false, DefaultScreen);
    }
#endif

    m_top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    m_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    initClay();

    m_config = std::make_shared<Config>();
    m_loader = std::make_shared<TitleLoader>();
    m_client = std::make_shared<Client>();

    updateURL();

    // add all objects to scope that are used to ensure lifetimes
    m_connections += {
        m_config->serverURL()->changedEmptySignal.connect([this, config = m_config, client = m_client]() noexcept { updateURL(); }),
        m_config->serverPort()->changedEmptySignal.connect([this, config = m_config, client = m_client]() noexcept { updateURL(); }),

        m_client->networkQueueChangedSignal.connect([this, client = m_client](const size_t&, const bool& processing) noexcept { tryUpdateClientURL(processing); }),
        m_client->titleCacheChangedSignal.connect([this, loader = m_loader, client = m_client]() noexcept { checkTitlesOutOfDate(); }),

        m_loader->titlesFinishedLoadingSignal.connect([this, loader = m_loader, client = m_client]() noexcept { checkTitlesOutOfDate(); }),
        m_loader->titleHashedSignal.connect([this, loader = m_loader, client = m_client](const std::shared_ptr<Title>& title, const Container&) noexcept { checkTitleOutOfDate(title); })
    };

    m_client->startQueueWorker();
    m_mainScreen = std::make_unique<MainScreen>(m_config, m_loader, m_client);

    m_prevTime = osGetTime();
}

Application::~Application() {
    m_client->stopQueueWorker(false);

    aptSetHomeAllowed(true);
    aptSetSleepAllowed(true);

    m_mainScreen.reset();
    m_client.reset();
    m_loader.reset();
    m_config.reset();

    m_connections.disconnect();

#ifdef DEBUG
    if(m_consoleInitialized && (!m_consoleEnabled || m_consoleScreen != DefaultScreen)) {
        // swap to top framebuffer to skip any data read errors
        setConsole(true, DefaultScreen);
    }
#endif

    if(m_clayTopMemory) linearFree(m_clayTopMemory);
    if(m_clayBottomMemory) linearFree(m_clayBottomMemory);

    for(auto& font : m_rendererData.fonts) {
        C2D_FontFree(font);
    }

    m_rendererData.fonts.clear();
    C2D_Clay_Exit();

    if(m_exceptionHandlerStack != nullptr) {
        threadOnException(NULL, NULL, NULL);
        free(m_exceptionHandlerStack);
    }
}

void Application::update() {
    PROFILE_SCOPE("App Update");
    hidScanInput();

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    if(aptIsHomeAllowed() && (kDown & KEY_START || kHeld & KEY_START) && !(kDown & KEY_L || kHeld & KEY_L)) {
        setShouldExit();
        return;
    }

    if(m_clayBottomMemory != nullptr && (!m_consoleEnabled || m_consoleScreen != GFX_BOTTOM)) {
        static u8 releaseTouch = 0;
        if(kDown & KEY_TOUCH || kHeld & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);

            Clay_SetPointerState({ static_cast<float>(touch.px), static_cast<float>(touch.py) }, true);

            m_prevTouch  = touch;
            releaseTouch = 2;
        }
        else {
            if(releaseTouch != 0) {
                releaseTouch--;
                Clay_SetPointerState({ static_cast<float>(m_prevTouch.px), static_cast<float>(m_prevTouch.py) }, false);
            }
            else {
                Clay_SetPointerState({ -1, -1 }, false);
            }
        }

        u64 currentTime = osGetTime();
        float deltaTime = (currentTime - m_prevTime) / 1000.f;
        Clay_UpdateScrollContainers(true, { 0.0f, 0.0f }, deltaTime);
        m_prevTime = currentTime;
    }
    else {
        m_prevTime = osGetTime();
    }

    m_mainScreen->update();

#if defined(DEBUG)
    if((kDown & KEY_L || kHeld & KEY_L) && kDown & KEY_X) {
        setConsole(!m_consoleEnabled, m_consoleScreen);
    }

    if((kDown & KEY_R || kHeld & KEY_R) && kDown & KEY_X) {
        switch(m_consoleScreen) {
        case GFX_TOP:    setConsole(m_consoleEnabled, GFX_BOTTOM); break;
        case GFX_BOTTOM: setConsole(m_consoleEnabled, GFX_TOP); break;
        default:         break;
        }
    }

    if((kDown & KEY_L || kHeld & KEY_L) && kDown & KEY_Y) {
        if(!m_consoleEnabled) {
            setConsole(true, m_consoleScreen);
        }

        Logger::logProfiler();
        return;
    }

    if((kDown & KEY_R || kHeld & KEY_R) && kDown & KEY_Y) {
        Logger::logProfiler();
        return;
    }
#endif
}

void Application::render() {
    PROFILE_SCOPE("App Render");

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    if(m_clayTopMemory != nullptr && (!m_consoleEnabled || m_consoleScreen != GFX_TOP)) {
        Clay_SetCurrentContext(m_topContext);
        Clay_BeginLayout();

        m_mainScreen->renderTop();

        C2D_TargetClear(m_top, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
        C2D_SceneBegin(m_top);

        C2D_Clay_RenderClayCommands(&m_rendererData, Clay_EndLayout(), GFX_TOP);
    }

    if(m_clayBottomMemory != nullptr && (!m_consoleEnabled || m_consoleScreen != GFX_BOTTOM)) {
        Clay_SetCurrentContext(m_bottomContext);
        Clay_BeginLayout();

        m_mainScreen->renderBottom();

        C2D_TargetClear(m_bottom, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
        C2D_SceneBegin(m_bottom);

        C2D_Clay_RenderClayCommands(&m_rendererData, Clay_EndLayout(), GFX_BOTTOM);
    }

    C3D_FrameEnd(0);
}

bool Application::loop() {
    if(!aptMainLoop() || shouldExit()) {
        setShouldExit();

        return false;
    }

    update();
    if(!shouldExit()) {
        render();
    }

    return !m_shouldExit;
}

bool Application::shouldExit() const { return m_shouldExit; }
void Application::setShouldExit(bool shouldExit) { m_shouldExit = shouldExit; }

#if defined(DEBUG)
void resetScreen(gfxScreen_t screen) {
    gfxSetScreenFormat(screen, GSP_BGR8_OES);
    gfxSetDoubleBuffering(screen, true);
}

void resetConsole(PrintConsole& console, u16* dummy, gfxScreen_t prevScreen) {
    console.frameBuffer = dummy;
    resetScreen(prevScreen);
}

void Application::setConsole(bool enabled, gfxScreen_t screen) {
    if(m_consoleEnabled == enabled && (!enabled || m_consoleScreen == screen)) {
        m_consoleScreen = screen;

        return;
    }

    gfxScreen_t prevScreen = m_consoleScreen;
    m_consoleEnabled       = enabled;
    m_consoleScreen        = screen;

    (void)prevScreen;
    if(!m_consoleInitialized) {
        if(m_consoleEnabled) {
            consoleInit(m_consoleScreen, &m_console);
            m_consoleInitialized = true;
        }
    }
    else if(m_consoleEnabled) {
        resetScreen(prevScreen);

        m_console.consoleHeight = m_console.windowHeight = 30;
        if(m_consoleScreen == GFX_TOP) {
            m_console.consoleWidth = m_console.windowWidth = gfxIsWide() ? 100 : 50;
        }
        else {
            m_console.consoleWidth = m_console.windowWidth = 40;
        }

        gfxSetScreenFormat(m_consoleScreen, GSP_RGB565_OES);
        gfxSetDoubleBuffering(m_consoleScreen, false);
        gfxSwapBuffers();
        gspWaitForVBlank();

        if(m_consoleScreen != prevScreen) {
            u16 width, height;
            m_console.frameBuffer = reinterpret_cast<u16*>(gfxGetFramebuffer(m_consoleScreen, GFX_LEFT, &width, &height));
            if(m_console.frameBuffer != nullptr) {
                memset(m_console.frameBuffer, 0, width * height * gspGetBytesPerPixel(GSP_RGB565_OES));
            }

            consoleClear();
            return;
        }

        u16 width, height;
        m_console.frameBuffer = reinterpret_cast<u16*>(gfxGetFramebuffer(m_consoleScreen, GFX_LEFT, &width, &height));

        std::vector<u16>* framebuffer;
        switch(m_consoleScreen) {
        case GFX_TOP:    framebuffer = &m_dummyTopFramebuffer; break;
        case GFX_BOTTOM: framebuffer = &m_dummyBottomFramebuffer; break;
        default:         return;
        }

        memcpy(m_console.frameBuffer, framebuffer->data(), width * height * sizeof(u16));
    }
    else {
        std::vector<u16>* framebuffer;
        switch(m_consoleScreen) {
        case GFX_TOP:    framebuffer = &m_dummyTopFramebuffer; break;
        case GFX_BOTTOM: framebuffer = &m_dummyBottomFramebuffer; break;
        default:         return;
        }

        memcpy(framebuffer->data(), m_console.frameBuffer, framebuffer->size() * sizeof(u16));
        resetConsole(m_console, framebuffer->data(), m_consoleScreen);
    }
}
#endif