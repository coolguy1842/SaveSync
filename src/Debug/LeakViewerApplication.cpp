#include <Debug/LeakViewerApplication.hpp>
#include <Theme.hpp>
#include <Util/Logger.hpp>
#include <Util/Profiler.hpp>

void HandleClayErrorsLeak(Clay_ErrorData errorData) { Logger::error("Clay", "{}", errorData.errorText.chars); }
void LeakViewerApplication::initClay() {
    Clay_SetMaxElementCount(128);

    size_t minMemorySize = Clay_MinMemorySize();
    m_clayTopMemory      = malloc(minMemorySize);
    m_clayBottomMemory   = malloc(minMemorySize);

    m_rendererData = { .fonts = { C2D_FontLoadSystem(CFG_REGION_USA) } };

    m_topContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayTopMemory), { TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT }, { HandleClayErrorsLeak });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);

    m_bottomContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayBottomMemory), { BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT }, { HandleClayErrorsLeak });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);
}

LeakViewerApplication::LeakViewerApplication()
    : m_shouldExit(false) {
    // prevent profiler from showing up on leak list
    Profiler::reset();

    m_leakBegin = cloneCurrentList();

    C2D_Clay_Init();
    m_top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    m_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    initClay();
}

LeakViewerApplication::~LeakViewerApplication() {
    freeClonedList(m_leakBegin);

    if(m_clayTopMemory != nullptr) free(m_clayTopMemory);
    if(m_clayBottomMemory != nullptr) free(m_clayBottomMemory);

    for(auto& font : m_rendererData.fonts) {
        C2D_FontFree(font);
    }

    m_rendererData.fonts.clear();
    C2D_Clay_Exit();
}

void LeakViewerApplication::update() {
    hidScanInput();

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    if(aptIsHomeAllowed() && kDown & KEY_START && !(kDown & KEY_L || kHeld & KEY_L)) {
        setShouldExit();
        return;
    }
}

void LeakViewerApplication::render() {
    Clay_RenderCommandArray topCommands, bottomCommands;
    {
        Clay_SetCurrentContext(m_topContext);
        Clay_BeginLayout();

        renderTop();
        topCommands = Clay_EndLayout();
    }

    {
        Clay_SetCurrentContext(m_bottomContext);
        Clay_BeginLayout();

        renderBottom();
        bottomCommands = Clay_EndLayout();
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(m_top, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
    C2D_SceneBegin(m_top);

    C2D_Clay_RenderClayCommands(&m_rendererData, topCommands, GFX_TOP);

    C2D_TargetClear(m_bottom, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
    C2D_SceneBegin(m_bottom);

    C2D_Clay_RenderClayCommands(&m_rendererData, bottomCommands, GFX_BOTTOM);

    C3D_FrameEnd(0);
}

bool LeakViewerApplication::loop() {
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

bool LeakViewerApplication::shouldExit() const { return m_shouldExit; }
void LeakViewerApplication::setShouldExit(bool shouldExit) { m_shouldExit = shouldExit; }

void LeakViewerApplication::renderTop() {
    CLAY(
        CLAY_ID("Body"),
        {
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_FIT(TOP_SCREEN_WIDTH),
                    .height = CLAY_SIZING_FIXED(TOP_SCREEN_HEIGHT),
                },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = Theme::Background(),
        }
    ) {
    }
}

void LeakViewerApplication::renderBottom() {
    CLAY(
        CLAY_ID("Body"),
        {
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_FIT(TOP_SCREEN_WIDTH),
                    .height = CLAY_SIZING_FIXED(TOP_SCREEN_HEIGHT),
                },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = Theme::Background(),
        }
    ) {
    }
}
