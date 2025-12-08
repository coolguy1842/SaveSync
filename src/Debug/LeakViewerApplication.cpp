#include <Debug/LeakViewerApplication.hpp>
#include <Debug/Logger.hpp>
#include <Debug/Profiler.hpp>
#include <FS/Archive.hpp>
#include <Theme.hpp>
#include <Util/ClayDefines.hpp>

void HandleClayErrorsLeak(Clay_ErrorData errorData) { Logger::error("Clay", "{}", errorData.errorText.chars); }
void LeakViewerApplication::initClay() {
    size_t minMemorySize = Clay_MinMemorySize();
    m_clayTopMemory      = linearAlloc(minMemorySize);
    m_clayBottomMemory   = linearAlloc(minMemorySize);

    m_rendererData = { .fonts = { C2D_FontLoadSystem(CFG_REGION_USA) } };

    m_topContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayTopMemory), { TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT }, { HandleClayErrorsLeak });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);

    m_bottomContext = Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(minMemorySize, m_clayBottomMemory), { BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT }, { HandleClayErrorsLeak });
    Clay_SetMeasureTextFunction(C2D_Clay_MeasureText, &m_rendererData.fonts);
}

void LeakViewerApplication::initLeakList() {
    // prevent profiler & sdmc from showing up on leak list
    Profiler::reset();
    Logger::closeLogFile();
    Archive::closeSDMC();

    m_leakBegin          = cloneCurrentList();
    leak_list_node* node = m_leakBegin;

    size_t totalLeaked = 0;
    std::shared_ptr<LeakInfo> info;

    while(node != nullptr) {
        // means untracked (ignored)
        if(node->tracesSize <= 0) {
            goto skip;
        }

        totalLeaked += node->dataSize;
        info = std::make_shared<LeakInfo>(LeakInfo{
            .node            = node,
            .ptrString       = std::format("0x{:X}", reinterpret_cast<uintptr_t>(node->data)),
            .sizeString      = std::format("{}", node->dataSize),
            .numTracesString = std::format("{}", node->tracesSize),
            .numTraces       = node->tracesSize,
        });

        switch(node->allocatedWith) {
        case MALLOC:   info->allocatorString = "malloc"; break;
        case CALLOC:   info->allocatorString = "calloc"; break;
        case STRDUP:   info->allocatorString = "strdup"; break;
        case STRNDUP:  info->allocatorString = "strndup"; break;
        case REALLOC:  info->allocatorString = "realloc"; break;
        case MEMALIGN: info->allocatorString = "memalign"; break;
        default:       info->allocatorString = "unknown"; break;
        }

        printf("num traces: %d\n", node->tracesSize);
        for(int i = 0; i < node->tracesSize; i++) {
            const leak_list_trace& trace = node->traces[i];

            info->traces[i] = {
                .symbolString            = trace.symbol,
                .labelString             = std::format("#{}", i + 1),
                .addressString           = std::format("{:X}", trace.address),
                .offsetString            = std::format("{:X}", trace.offset),
                .addressWithOffsetString = std::format("0x{:X}", trace.address + trace.offset),
            };

            printf("label: %s\n", info->traces[i].labelString.c_str());
        }

        m_leakInfo.push_back(info);
    skip:
        node = node->next;
    }

    m_totalLeakedString = std::format("{}", totalLeaked);
}

LeakViewerApplication::LeakViewerApplication()
    : m_shouldExit(false)
    , m_selectedLeak(0) {
    initLeakList();
    C2D_Clay_Init();

    m_top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    m_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    initClay();

    m_prevTime = osGetTime();
}

LeakViewerApplication::~LeakViewerApplication() {
    freeClonedList(m_leakBegin);

    if(m_clayTopMemory != nullptr) linearFree(m_clayTopMemory);
    if(m_clayBottomMemory != nullptr) linearFree(m_clayBottomMemory);

    for(auto& font : m_rendererData.fonts) {
        C2D_FontFree(font);
    }

    m_rendererData.fonts.clear();
    C2D_Clay_Exit();
}

void LeakViewerApplication::updateScroll() {
    if(m_selectedLeak < m_scroll) {
        m_scroll = m_selectedLeak;
    }
    else if(m_selectedLeak >= m_scroll + m_visibleRows) {
        m_scroll = m_selectedLeak - m_visibleRows + 1;
    }

    m_scroll = std::clamp(m_scroll, static_cast<u16>(0), static_cast<u16>(m_rows - m_visibleRows));
}

void LeakViewerApplication::update() {
    hidScanInput();

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    if(kDown & KEY_START && !(kDown & KEY_L || kHeld & KEY_L)) {
        setShouldExit();
        return;
    }

    u32 kRepeat = hidKeysDownRepeat();
    if(kRepeat & KEY_UP && m_selectedLeak != 0) {
        m_selectedLeak--;
        updateScroll();
    }

    if(kRepeat & KEY_DOWN && m_rows != 0 && m_selectedLeak < static_cast<size_t>(m_rows - 1)) {
        m_selectedLeak++;
        updateScroll();
    }

    if(m_clayBottomMemory != nullptr) {
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
}

void LeakViewerApplication::render() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    if(m_top != nullptr) {
        Clay_SetCurrentContext(m_topContext);
        Clay_BeginLayout();

        renderTop();

        C2D_TargetClear(m_top, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
        C2D_SceneBegin(m_top);

        C2D_Clay_RenderClayCommands(&m_rendererData, Clay_EndLayout(), GFX_TOP);
    }

    if(m_bottom != nullptr) {
        Clay_SetCurrentContext(m_bottomContext);
        Clay_BeginLayout();

        renderBottom();

        C2D_TargetClear(m_bottom, C2D_Color32(0x00, 0x00, 0x00, 0xFF));
        C2D_SceneBegin(m_bottom);

        C2D_Clay_RenderClayCommands(&m_rendererData, Clay_EndLayout(), GFX_BOTTOM);
    }

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

#define SHOW_STR(text)                                                                                                  \
    {                                                                                                                   \
        Clay_String str{ .isStaticallyAllocated = false, .length = static_cast<int32_t>(strlen(text)), .chars = text }; \
        CLAY_TEXT(str, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = textSize }));                         \
    }

void LeakViewerApplication::renderTop() {
    CLAY(
        CLAY_ID("Body"),
        {
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_FIT(TOP_SCREEN_WIDTH),
                    .height = CLAY_SIZING_FIXED(TOP_SCREEN_HEIGHT),
                },
                .childGap        = 2,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = Theme::Background(),
        }
    ) {
        CLAY(
            CLAY_ID("Header"),
            {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH),
                    },
                },
            }
        ) {
            const uint16_t textSize = 12;
            CLAY_TEXT(CLAY_STRING("Leak Summary"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12 }));

            HSPACER();

            CLAY_TEXT(CLAY_STRING("Total Leaked: "), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12 }));
            SHOW_STR(m_totalLeakedString.c_str());
        }

        CLAY(
            CLAY_ID("LeakListDisplay"),
            {
                .layout = {
                    .sizing = {
                        .width  = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH),
                        .height = CLAY_SIZING_GROW(),
                    },
                    .childGap        = 1,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .backgroundColor = Theme::Surface0(),
            }
        ) {
            const uint16_t textSize = 12;
            const uint16_t gap      = 1;

            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH) } } }) {
                SHOW_STR("Address")
                HSPACER();
                SHOW_STR("Size")
                HSPACER();
                SHOW_STR("Allocator")
            }

            // + 2 for padding
            float listChildHeight = textSize + 2 + gap;
            CLAY(
                CLAY_ID("ScrollContainer"),
                {
                    .layout = {
                        .sizing = {
                            .width  = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH),
                            .height = CLAY_SIZING_GROW(),
                        },
                        .childGap        = gap,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .clip = {
                        .vertical    = true,
                        .childOffset = {
                            .y = -static_cast<float>(listChildHeight * m_scroll),
                        },
                    },
                }
            ) {
                Clay_ElementData data = Clay_GetElementData(CLAY_ID("ScrollContainer"));
                if(!data.found || data.boundingBox.width == 0 || data.boundingBox.height == 0) {
                    goto skipList;
                }

                m_rows        = m_leakInfo.size();
                m_visibleRows = std::floor(data.boundingBox.height / listChildHeight);

                if(m_leakInfo.empty()) {
                    CLAY_AUTO_ID({
                        .layout = {
                            .sizing = {
                                .width  = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_GROW(),
                            },
                            .childAlignment = {
                                .x = CLAY_ALIGN_X_CENTER,
                                .y = CLAY_ALIGN_Y_CENTER,
                            },
                        },
                    }) {
                        CLAY_TEXT(CLAY_STRING("Nothing Leaked!"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 25 }));
                    }

                    goto skipList;
                }

                for(size_t i = 0; i < m_leakInfo.size(); i++) {
                    std::shared_ptr<LeakInfo> info = m_leakInfo[i];

                    Clay_Color backgroundColor = Theme::Surface1();
                    if(m_selectedLeak == i) {
                        backgroundColor = Theme::Surface2();
                    }

                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH) }, .padding = { 0, 0, 1, 1 } }, .backgroundColor = backgroundColor }) {
                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH / 3) } } }){
                            SHOW_STR(info->ptrString.c_str())
                        }

                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH / 3) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } } }){
                            SHOW_STR(info->sizeString.c_str())
                        }

                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH / 3) }, .childAlignment = { .x = CLAY_ALIGN_X_RIGHT } } }) {
                            SHOW_STR(info->allocatorString)
                        }
                    }
                }

            skipList:
            }
        }
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
                .childGap        = 2,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = Theme::Background(),

            .clip = {
                .vertical    = true,
                .childOffset = {
                    .y = Clay_GetScrollOffset().y,
                },
            },
        }
    ) {
        const uint16_t textSize = 12;
        if(m_leakInfo.size() <= m_selectedLeak) {
            goto skipInfo;
        }

        {
            std::shared_ptr<LeakInfo> info = m_leakInfo[m_selectedLeak];
            for(int i = 0; i < info->numTraces; i++) {
                CLAY_AUTO_ID({ .layout = { .childGap = 4 } }) {
                    SHOW_STR(info->traces[i].labelString.c_str());
                    SHOW_STR(info->traces[i].addressWithOffsetString.c_str());
                    SHOW_STR(info->traces[i].symbolString);
                }
            }
        }

    skipInfo:
    }
}

#undef SHOW_STR