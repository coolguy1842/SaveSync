#include <Application.hpp>
#include <Util/Logger.hpp>
#include <Util/Profiler.hpp>
#include <clay_renderer_C2D.hpp>

void HandleClayErrors(Clay_ErrorData errorData) { Logger::error("Clay", "{}", errorData.errorText.chars); }
void Application::initClay() {
    Clay_SetMaxElementCount(1024);

    size_t minMemorySize = Clay_MinMemorySize();
    m_clayTopMemory      = malloc(minMemorySize);
    m_clayBottomMemory   = malloc(minMemorySize);

    Logger::info("app init", "clay min: {}", minMemorySize);
    if(m_clayTopMemory == NULL) {
        Logger::critical("App Init Clay", "Failed to create top memory");
    }

    if(m_clayBottomMemory == NULL) {
        Logger::critical("App Init Clay", "Failed to create bottom memory");
    }

    m_rendererData = { .fonts = { C2D_FontLoadSystem(CFG_REGION_USA) } };

    m_topContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayTopMemory), { TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT }, { HandleClayErrors });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);

    m_bottomContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayBottomMemory), { BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT }, { HandleClayErrors });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);
}

Application::Application()
    : m_shouldExit(false) {
    // PROFILE_SCOPE("App Init");
    aptInit();

    C2D_Clay_Init();
    m_top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    m_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    initClay();

    // m_client = std::make_shared<Client>();
    // m_client->startQueueWorker();

    m_prevTime = osGetTime();
}

Application::~Application() {
    if(m_clayTopMemory) free(m_clayTopMemory);
    if(m_clayBottomMemory) free(m_clayBottomMemory);

    for(auto& font : m_rendererData.fonts) {
        C2D_FontFree(font);
    }

    m_rendererData.fonts.clear();

    C2D_Clay_Exit();
    aptExit();
}

void Application::update() {
    // PROFILE_SCOPE("App Update");
    hidScanInput();

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    if(aptIsHomeAllowed() && (kDown & KEY_START || kHeld & KEY_START) && !(kDown & KEY_L || kHeld & KEY_L)) {
        setShouldExit();
        return;
    }

    // static u8 releaseTouch = 0;
    // if(kDown & KEY_TOUCH || kHeld & KEY_TOUCH) {
    //     touchPosition touch;
    //     hidTouchRead(&touch);

    //     Clay_SetPointerState({ static_cast<float>(touch.px), static_cast<float>(touch.py) }, true);

    //     m_prevTouch  = touch;
    //     releaseTouch = 2;
    // }
    // else {
    //     if(releaseTouch != 0) {
    //         releaseTouch--;
    //         Clay_SetPointerState({ static_cast<float>(m_prevTouch.px), static_cast<float>(m_prevTouch.py) }, false);
    //     }
    //     else {
    //         Clay_SetPointerState({ -1, -1 }, false);
    //     }
    // }

    // u64 currentTime = osGetTime();
    // float deltaTime = (currentTime - m_prevTime) / 1000.f;
    // Clay_UpdateScrollContainers(true, { 0.0f, 0.0f }, deltaTime);
    // m_prevTime = currentTime;
}

void Application::render() {
    // PROFILE_SCOPE("App Render");

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    {
        Clay_SetCurrentContext(m_topContext);
        Clay_BeginLayout();

        C2D_TargetClear(m_top, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
        C2D_SceneBegin(m_top);

        C2D_Clay_RenderClayCommands(&m_rendererData, Clay_EndLayout(), GFX_TOP);
    }

    {
        Clay_SetCurrentContext(m_bottomContext);
        Clay_BeginLayout();

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
