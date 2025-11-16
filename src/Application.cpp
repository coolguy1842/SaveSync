#include <Application.hpp>
#include <Util/Logger.hpp>
#include <Util/Profiler.hpp>
#include <clay_renderer_C2D.hpp>

void HandleClayErrors(Clay_ErrorData errorData) { Logger::error("Clay", "{}", errorData.errorText.chars); }
void Application::updateURL() {
    std::string url = std::format("{}:{}", m_config->serverURL()->value(), m_config->serverPort()->value());

    if(m_client->processingQueueRequest()) {
        m_pendingURL = url;
        return;
    }

    m_client->setURL(url);
    m_pendingURL.clear();
}

void Application::tryUpdateClientURL(size_t, bool processing) {
    if(m_pendingURL.empty() || processing) {
        return;
    }

    m_client->setURL(m_pendingURL);
    m_pendingURL.clear();
}

void Application::initClay() {
    Clay_SetMaxElementCount(1024);

    size_t minMemorySize = Clay_MinMemorySize();
    m_clayTopMemory      = malloc(minMemorySize);
    m_clayBottomMemory   = malloc(minMemorySize);

    m_rendererData = { .fonts = { C2D_FontLoadSystem(CFG_REGION_USA) } };

    m_topContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayTopMemory), { TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT }, { HandleClayErrors });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);

    m_bottomContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayBottomMemory), { BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT }, { HandleClayErrors });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);
}

bool filesEqual(std::vector<FileInfo> a, std::vector<FileInfo> b) { return std::equal(a.begin(), a.end(), b.begin(), b.end()); }
void checkTitleOutOfDate(std::shared_ptr<Title> title, const bool& hasCache, const std::unordered_map<u64, TitleInfo>& cache) {
    if(!hasCache) {
        title->setOutOfDate(0);
        return;
    }

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

void checkTitlesOutOfDate(std::vector<std::shared_ptr<Title>> titles, const bool& hasCache, const std::unordered_map<u64, TitleInfo>& cache) {
    for(auto title : titles) {
        checkTitleOutOfDate(title, hasCache, cache);
    }
}

Application::Application(bool consoleEnabled, gfxScreen_t consoleScreen)
    : m_shouldExit(false)
    , m_consoleEnabled(false)
    , m_consoleInitialized(false) {
    PROFILE_SCOPE("App Init");
    aptInit();

#if !defined(DEBUG)
    (void)consoleEnabled;
    (void)consoleScreen;
#endif

#if defined(DEBUG) && defined(REDIRECT_CONSOLE)
    // copy stdout & stderr handle to grab back after closing 3dslink socket
    stdoutDup = dup(STDOUT_FILENO);
    stderrDup = dup(STDERR_FILENO);

    link3dsStdio();
#endif

    C2D_Clay_Init();

    // init the static s_glyphSheets in C2Di_TextEnsureLoad, as it will show in the leak list
    C2D_TextBufNew(0);
    clearLeaks();

    m_top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    m_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

#if defined(DEBUG) && !defined(REDIRECT_CONSOLE)
    if(!consoleEnabled) {
        setConsole(true, consoleScreen);
        setConsole(false, consoleScreen);
    }
    else {
        setConsole(consoleEnabled, consoleScreen);
    }
#endif

    initClay();

    m_config = std::make_shared<Config>();
    m_loader = std::make_shared<TitleLoader>();
    m_client = std::make_shared<Client>();

    updateURL();
    m_config->serverURL()->changedEmptySignal()->connect(this, &Application::updateURL);
    m_config->serverPort()->changedEmptySignal()->connect(this, &Application::updateURL);

    m_client->networkQueueChangedSignal()->connect(this, &Application::tryUpdateClientURL);

    m_client->titleCacheChangedSignal()->connect([this]() { checkTitlesOutOfDate(m_loader->titles(), m_client->cachedTitleInfoLoaded(), m_client->cachedTitleInfo()); });
    m_loader->titlesFinishedLoadingSignal()->connect([this]() { checkTitlesOutOfDate(m_loader->titles(), m_client->cachedTitleInfoLoaded(), m_client->cachedTitleInfo()); });
    m_loader->titleHashedSignal()->connect([this](std::shared_ptr<Title> title, Container) { checkTitleOutOfDate(title, m_client->cachedTitleInfoLoaded(), m_client->cachedTitleInfo()); });

    m_client->startQueueWorker();
    m_mainScreen = std::make_unique<MainScreen>(m_config, m_loader, m_client);
    m_prevTime   = osGetTime();
}

#if defined(DEBUG) && !defined(REDIRECT_CONSOLE)
void resetScreen(gfxScreen_t screen) {
    gfxSetScreenFormat(screen, GSP_BGR8_OES);
    gfxSetDoubleBuffering(screen, true);
}

void resetConsole(PrintConsole& console, u16* dummy, gfxScreen_t prevScreen) {
    console.frameBuffer = dummy;
    resetScreen(prevScreen);
}
#endif

Application::~Application() {
    m_mainScreen.reset();
    m_loader.reset();
    m_client.reset();
    m_config.reset();

    if(m_clayTopMemory != nullptr) {
        free(m_clayTopMemory);
    }

    if(m_clayBottomMemory != nullptr) {
        free(m_clayBottomMemory);
    }

    for(auto& font : m_rendererData.fonts) {
        C2D_FontFree(font);
    }

    m_rendererData.fonts.clear();

#if defined(DEBUG)
#if !defined(REDIRECT_CONSOLE)
    if(m_consoleInitialized && !(m_consoleEnabled && m_consoleScreen == GFX_TOP)) {
        if(m_consoleEnabled && m_consoleScreen != GFX_TOP) {
            resetScreen(m_consoleScreen);
        }

        // swap to top framebuffer to skip any data read errors
        consoleInit(GFX_TOP, NULL);
    }
#endif
    leak_list_node* prevBegin = cloneCurrentList();
#endif

    C2D_Clay_Exit();

#if defined(DEBUG)
    leak_list_node* begin = leakListBegin();
    leak_list_node* node  = begin;

    leak_list_node* prevNode = NULL;
    while(node != NULL && prevBegin != NULL) {
        // if memory node is new then remove from the list
        leak_list_node* next  = node->next;
        leak_list_node* node2 = prevBegin;

        while(node2 != NULL) {
            if(node->data == node2->data && node->dataSize == node2->dataSize && node->allocatedWith == node2->allocatedWith) {
                goto skip;
            }

            node2 = node2->next;
        }

        if(prevNode != NULL) {
            prevNode->next = next;
        }
        else {
            if(next == NULL) {
                clearLeaks();
            }

            setLeakList(next);
        }

        node->next = prevBegin;
        prevBegin  = node;

        node = next;
        continue;
    skip:
        prevNode = node;
        node     = node->next;
    }

    freeClonedList(prevBegin);
#endif

    aptExit();

#if defined(DEBUG) && defined(REDIRECT_CONSOLE)
    if(dup2(stdoutDup, STDOUT_FILENO) >= 0) {
        close(stdoutDup);
    }

    if(dup2(stderrDup, STDERR_FILENO) >= 0) {
        close(stdoutDup);
    }
#endif
}

void Application::update() {
    PROFILE_SCOPE("App Update");
    hidScanInput();

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    if(aptIsHomeAllowed() && kDown & KEY_START && !(kDown & KEY_L || kHeld & KEY_L)) {
        setShouldExit();
        return;
    }

    if(!m_consoleEnabled || m_consoleScreen != GFX_BOTTOM) {
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

#if defined(DEBUG) && !defined(REDIRECT_CONSOLE)
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

    m_mainScreen->update();
}

void Application::render() {
    PROFILE_SCOPE("App Render");

    Clay_RenderCommandArray topCommands, bottomCommands;
    {
        PROFILE_SCOPE("App Layout Top");

        Clay_SetCurrentContext(m_topContext);
        Clay_BeginLayout();

        m_mainScreen->renderTop();
        topCommands = Clay_EndLayout();
    }

    {
        PROFILE_SCOPE("App Layout Bottom");

        Clay_SetCurrentContext(m_bottomContext);
        Clay_BeginLayout();

        m_mainScreen->renderBottom();
        bottomCommands = Clay_EndLayout();
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    if(!m_consoleEnabled || m_consoleScreen != GFX_TOP) {
        PROFILE_SCOPE("App Render Top");

        C2D_TargetClear(m_top, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
        C2D_SceneBegin(m_top);

        C2D_Clay_RenderClayCommands(&m_rendererData, topCommands, GFX_TOP);
    }

    if(!m_consoleEnabled || m_consoleScreen != GFX_BOTTOM) {
        PROFILE_SCOPE("App Render Bottom");

        C2D_TargetClear(m_bottom, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
        C2D_SceneBegin(m_bottom);

        C2D_Clay_RenderClayCommands(&m_rendererData, bottomCommands, GFX_BOTTOM);
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

void Application::setConsole(bool enabled, gfxScreen_t screen) {
#if defined(DEBUG) && !defined(REDIRECT_CONSOLE)
    if(m_consoleEnabled == enabled && (!enabled || m_consoleScreen == screen)) {
        m_consoleScreen = screen;

        return;
    }

    gfxScreen_t prevScreen = m_consoleScreen;
    m_consoleEnabled       = enabled;
    m_consoleScreen        = screen;

    if(m_consoleEnabled && !m_consoleInitialized) {
        consoleInit(m_consoleScreen, &m_console);
        m_consoleInitialized = true;
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
            memset(m_console.frameBuffer, 0, width * height * gspGetBytesPerPixel(GSP_RGB565_OES));

            consoleClear();
            return;
        }

        u16 width, height;
        m_console.frameBuffer = reinterpret_cast<u16*>(gfxGetFramebuffer(m_consoleScreen, GFX_LEFT, &width, &height));

        u16* dummyFramebuffer = nullptr;
        switch(m_consoleScreen) {
        case GFX_TOP:    dummyFramebuffer = m_dummyTopFramebuffer.data(); break;
        case GFX_BOTTOM: dummyFramebuffer = m_dummyBottomFramebuffer.data(); break;
        default:         return;
        }

        memcpy(m_console.frameBuffer, dummyFramebuffer, width * height * sizeof(u16));
    }
    else {
        size_t framebufferSize = 0;
        u16* framebuffer       = nullptr;
        switch(m_consoleScreen) {
        case GFX_TOP:
            framebufferSize = m_dummyTopFramebuffer.size();
            framebuffer     = m_dummyTopFramebuffer.data();

            break;
        case GFX_BOTTOM:
            framebufferSize = m_dummyBottomFramebuffer.size();
            framebuffer     = m_dummyBottomFramebuffer.data();

            break;
        default: return;
        }

        memcpy(framebuffer, m_console.frameBuffer, framebufferSize * sizeof(u16));
        resetConsole(m_console, framebuffer, m_consoleScreen);
    }
#else
    (void)enabled;
    (void)screen;
#endif
}