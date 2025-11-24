#include <Config.hpp>
#include <Theme.hpp>
#include <UI/MainScreen.hpp>
#include <clay.h>
#include <clay_renderer_C2D.hpp>

#define HSPACER_0()    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })
#define HSPACER_1(pix) CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(pix), .height = CLAY_SIZING_FIT() } } })

#define HSPACER_X(x, pix, FUNC, ...) FUNC
#define HSPACER(...)                 HSPACER_X(, ##__VA_ARGS__, HSPACER_1(__VA_ARGS__), HSPACER_0(__VA_ARGS__))

#define VSPACER_0()    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() } } })
#define VSPACER_1(pix) CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIXED(pix) } } })

#define VSPACER_X(x, pix, FUNC, ...) FUNC
#define VSPACER(...)                 VSPACER_X(, ##__VA_ARGS__, VSPACER_1(__VA_ARGS__), VSPACER_0(__VA_ARGS__))

#define _BADGE(...)                             \
    CLAY_AUTO_ID({                              \
        .layout = {                             \
            .sizing = {                         \
                .width  = CLAY_SIZING_FIXED(8), \
                .height = CLAY_SIZING_FIXED(8), \
            },                                  \
        },                                      \
        __VA_ARGS__,                            \
        .custom = {                             \
            &circleData,                        \
        },                                      \
    })

#define BADGE(...) _BADGE(.backgroundColor = __VA_ARGS__)

void MainScreen::updateTitleNames() {
    m_titleTexts.clear();

    for(auto title : m_loader->titles()) {
        m_titleTexts += title->longDescription();
    }
}

void MainScreen::updateLoadedText(size_t loaded) {
    m_loadedText   = std::format("{}/{} Loaded", loaded, m_loader->totalTitles());
    m_loadedString = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_loadedText.size()), .chars = m_loadedText.c_str() };
}

void MainScreen::updateQueuedText(size_t queueSize, bool processing) {
    m_networkQueueText   = std::format("{} Queued", queueSize - processing);
    m_networkQueueString = Clay_String{ .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_networkQueueText.size()), .chars = m_networkQueueText.c_str() };
}

void MainScreen::updateRequestStatusText(std::string status) {
    m_networkRequestText   = status;
    m_networkRequestString = Clay_String{ .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_networkRequestText.size()), .chars = m_networkRequestText.c_str() };
}

void MainScreen::onClientRequestFailed(std::string status) {
    m_okText   = status;
    m_okString = Clay_String{ .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_okText.size()), .chars = m_okText.c_str() };

    m_okActive = true;
}

MainScreen::MainScreen(Config* config, TitleLoader* loader, Client* client)
    : m_config(config)
    , m_loader(loader)
    , m_client(client)
    , m_settingsScreen(config)
    , m_selectedTitle(0) {
    // m_loader->titlesLoadedChangedSignal()->connect(this, &MainScreen::updateLoadedText);

    // m_client->networkQueueChangedSignal()->connect(this, &MainScreen::updateQueuedText);
    // m_client->requestStatusChangedSignal()->connect(this, &MainScreen::updateRequestStatusText);
    // m_client->requestFailedSignal()->connect(this, &MainScreen::onClientRequestFailed);

    // m_loader->titlesFinishedLoadingSignal()->connect(this, &MainScreen::updateTitleNames);
}

void MainScreen::scrollToCurrent() {
    u16 selectedRow = m_selectedTitle / m_cols;
    if(selectedRow < m_scroll) {
        m_scroll = selectedRow;
    }
    else if(selectedRow >= m_scroll + m_visibleRows) {
        m_scroll = selectedRow - m_visibleRows + 1;
    }

    m_scroll = std::clamp(m_scroll, static_cast<u16>(0), static_cast<u16>(m_rows - m_visibleRows));
}

void MainScreen::tryUpload(Container container) {
    if(m_selectedTitle >= m_loader->titles().size()) {
        return;
    }

    auto title = m_loader->titles()[m_selectedTitle];
    if(!title->containerAccessible(container) || title->getContainerFiles(container).empty()) {
        return;
    }

    switch(container) {
    case SAVE:    showYesNo("Upload Save", QueuedRequest::UPLOAD_SAVE, title); return;
    case EXTDATA: showYesNo("Upload Extdata", QueuedRequest::UPLOAD_EXTDATA, title); return;
    default:      return;
    }
}

void MainScreen::tryDownload(Container container) {
    if(m_selectedTitle >= m_loader->titles().size()) {
        return;
    }

    auto title = m_loader->titles()[m_selectedTitle];
    if(!title->containerAccessible(container)) {
        return;
    }

    switch(container) {
    case SAVE:    showYesNo("Download Save", QueuedRequest::DOWNLOAD_SAVE, title); return;
    case EXTDATA: showYesNo("Download Extdata", QueuedRequest::DOWNLOAD_EXTDATA, title); return;
    default:      return;
    }
}

void MainScreen::update() {
    if(m_settingsScreen.isActive()) {
        m_settingsScreen.update();
        return;
    }

    u32 kDown   = hidKeysDown();
    u32 kRepeat = hidKeysDownRepeat();
    u32 kHeld   = hidKeysHeld();

    if(m_yesNoActive) {
        if(kRepeat & KEY_RIGHT || kRepeat & KEY_LEFT) {
            m_yesSelected = !m_yesSelected;
        }

        if(kDown & KEY_A) {
            if(m_yesSelected) {
                m_client->queueAction(m_yesNoAction);
            }

            m_yesNoActive = false;
        }
        else if(kDown & KEY_B) {
            if(m_yesSelected) {
                m_yesSelected = false;
            }
            else {
                m_yesNoActive = false;
            }
        }

        return;
    }

    if(m_okActive) {
        if(kDown & KEY_A || kDown & KEY_B) {
            m_okActive = false;
            m_client->setProcessRequests();
        }

        return;
    }

    if(m_loader->isLoadingTitles()) {
        return;
    }

    if(kDown & KEY_Y) {
        m_settingsScreen.setActive();
        return;
    }

    bool changed = false;
    if(kRepeat & KEY_UP && m_selectedTitle >= m_cols) {
        m_selectedTitle -= m_cols;
        changed = true;
    }

    if(kRepeat & KEY_DOWN && m_selectedTitle / m_cols != static_cast<size_t>(m_rows - 1)) {
        m_selectedTitle += m_cols;
        changed = true;
    }

    if(kRepeat & KEY_RIGHT && (m_selectedTitle + 1) % m_cols != 0) {
        m_selectedTitle++;
        changed = true;
    }

    if(kRepeat & KEY_LEFT && m_selectedTitle % m_cols != 0) {
        m_selectedTitle--;
        changed = true;
    }

    if(changed) {
        m_selectedTitle = std::clamp(m_selectedTitle, static_cast<size_t>(0), m_loader->titles().size() - 1);
    }

    std::shared_ptr<Title> title = nullptr;
    if(m_selectedTitle < m_loader->titles().size()) {
        title = m_loader->titles()[m_selectedTitle];
    }
    else if(!m_loader->titles().empty()) {
        m_selectedTitle = 0;
        title           = m_loader->titles().front();
    }

    if(kHeld & KEY_L && kDown & KEY_A && title != nullptr) {
        tryUpload(SAVE);
    }
    else if(kHeld & KEY_L && kDown & KEY_B && title != nullptr) {
        tryDownload(SAVE);
    }
    else if(kHeld & KEY_R && kDown & KEY_A && title != nullptr) {
        tryUpload(EXTDATA);
    }
    else if(kHeld & KEY_R && kDown & KEY_B && title != nullptr) {
        tryDownload(EXTDATA);
    }

    if(m_updateTitleInfo) {
        if(title != nullptr) {
            m_titleText = title->longDescription();
            m_idText    = std::format("ID: {:08X} ({})", title->lowID(), title->productCode());

            const char* mediaType;
            switch(title->mediaType()) {
            case MEDIATYPE_NAND:      mediaType = "NAND"; break;
            case MEDIATYPE_SD:        mediaType = "SD Card"; break;
            case MEDIATYPE_GAME_CARD: mediaType = "Game Card"; break;
            default:                  mediaType = ""; break;
            }

            m_mediaTypeText = std::format("Media Type: {}", mediaType);
        }
        else {
            m_titleText = "No Title";
            m_idText.clear();
            m_mediaTypeText.clear();
        }

        m_titleString     = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_titleText.size()), .chars = m_titleText.c_str() };
        m_idString        = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_idText.size()), .chars = m_idText.c_str() };
        m_mediaTypeString = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_mediaTypeText.size()), .chars = m_mediaTypeText.c_str() };
    }
}

void MainScreen::showYesNo(std::string text, QueuedRequest::RequestType action, std::shared_ptr<Title> title) {
    m_yesNoActive = true;
    m_yesSelected = false;

    m_yesNoAction = {
        .type  = action,
        .title = title
    };

    m_yesNoText   = text;
    m_yesNoString = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_yesNoText.size()), .chars = m_yesNoText.c_str() };
}

void MainScreen::onButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData) { reinterpret_cast<MainScreen*>(userData)->handleButtonHover(elementId, pointerData); }
void MainScreen::handleButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData) {
    if(pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        return;
    }

    if(elementId.id == CLAY_ID("SaveUpload").id) {
        tryUpload(SAVE);
    }
    else if(elementId.id == CLAY_ID("SaveDownload").id) {
        tryDownload(SAVE);
    }
    else if(elementId.id == CLAY_ID("ExtdataUpload").id) {
        tryUpload(EXTDATA);
    }
    else if(elementId.id == CLAY_ID("ExtdataDownload").id) {
        tryDownload(EXTDATA);
    }
    else if(elementId.id == CLAY_ID("Yes").id) {
        m_client->queueAction(m_yesNoAction);
        m_yesNoActive = false;
    }
    else if(elementId.id == CLAY_ID("No").id) {
        m_yesNoActive = false;
    }
    else if(elementId.id == CLAY_ID("Ok").id) {
        m_okActive = false;
        m_client->setProcessRequests();
    }
    else if(elementId.id == CLAY_ID("Settings").id && !m_loader->isLoadingTitles()) {
        m_settingsScreen.setActive();
    }
}

Clay_Color borderColor(u8 timerID) {
    static float timers[3] = { 0.0f, 0.0f, 0.0f };
    timers[timerID] += 0.025f;

    float highlight_multiplier = fmax(0.0, fabs(fmod(timers[timerID], 1.0) - 0.5) / 0.5);

    Clay_Color accent = Theme::Accent();
    return { accent.r + (255 - accent.r) * highlight_multiplier, accent.g + (0xFF - accent.g) * highlight_multiplier, accent.b + (0xFF - accent.b) * highlight_multiplier, 0xFF };
}

#define YES_NO_OVERLAY                                     \
    CLAY(                                                  \
        CLAY_ID("YesNoOverlay"),                           \
        {                                                  \
            .layout = {                                    \
                .sizing = {                                \
                    .width  = CLAY_SIZING_PERCENT(1.0),    \
                    .height = CLAY_SIZING_PERCENT(1.0),    \
                },                                         \
                .childAlignment = {                        \
                    .x = CLAY_ALIGN_X_CENTER,              \
                    .y = CLAY_ALIGN_Y_CENTER,              \
                },                                         \
            },                                             \
            .backgroundColor = { 0x00, 0x00, 0x00, 0xB2 }, \
                                                           \
            .floating = {                                  \
                .attachPoints = {                          \
                    .element = CLAY_ATTACH_POINT_LEFT_TOP, \
                    .parent  = CLAY_ATTACH_POINT_LEFT_TOP, \
                },                                         \
                .attachTo = CLAY_ATTACH_TO_PARENT,         \
            },                                             \
        }                                                  \
    )

const u16 iconGap = 2;

static CustomElementData circleData = { .type = CUSTOM_ELEMENT_TYPE_CIRCLE };
void MainScreen::TitleIcon(std::shared_ptr<Title> title, float width, float height, Clay_BorderElementConfig border) {
    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height) } }, .image = { .imageData = title->icon() }, .border = border }) {
        Clay_Color color = Theme::Unknown();
        if(m_client->cachedTitleInfoLoaded()) {
            switch(title->outOfDate()) {
            case SAVE | EXTDATA: color = Theme::SaveAndExtdata(); break;
            case SAVE:           color = Theme::Save(); break;
            case EXTDATA:        color = Theme::Extdata(); break;
            default:             goto skipBadge;
            }
        }

        _BADGE(
                .backgroundColor = color,

                .floating = {
                    .offset = {
                        .x = -3,
                        .y = 3,
                    },
                    .attachPoints = {
                        .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                        .parent  = CLAY_ATTACH_POINT_RIGHT_TOP,
                    },
                    .attachTo = CLAY_ATTACH_TO_PARENT,
                    .clipTo   = CLAY_CLIP_TO_ATTACHED_PARENT,
                }
        );

    skipBadge:
    }
}

void MainScreen::GridLayout() {
    Clay_ElementData data = Clay_GetElementData(CLAY_ID("Titles"));
    if(!data.found || data.boundingBox.width == 0.0f || data.boundingBox.height == 0.0f) {
        return;
    }

    m_cols        = floor((data.boundingBox.width + iconGap) / (SMDH::ICON_WIDTH + iconGap));
    m_rows        = ceil(m_loader->titles().size() / static_cast<float>(m_cols));
    m_visibleRows = floor((data.boundingBox.height + iconGap) / (SMDH::ICON_HEIGHT + iconGap));
    scrollToCurrent();

    for(size_t titleIdx = 0; titleIdx < m_loader->titles().size();) {
        CLAY_AUTO_ID({ .layout = { .padding = { 1, 1, 0, 0 } } }) {
            CLAY_AUTO_ID({ .layout = { .childGap = iconGap } }) {
                for(bool first = true; titleIdx < m_loader->titles().size() && !(!first && titleIdx % m_cols == 0); titleIdx++, first = false) {
                    TitleIcon(m_loader->titles()[titleIdx], SMDH::ICON_WIDTH, SMDH::ICON_HEIGHT, titleIdx == m_selectedTitle ? Clay_BorderElementConfig{ .color = borderColor(0), .width = CLAY_BORDER_OUTSIDE(1) } : Clay_BorderElementConfig{});
                }
            }
        }
    }
}

void MainScreen::ListLayout() {
    Clay_ElementData data = Clay_GetElementData(CLAY_ID("Titles"));
    if(!data.found || data.boundingBox.width == 0.0f || data.boundingBox.height == 0.0f) {
        return;
    }

    m_cols        = 1;
    m_rows        = m_loader->titles().size();
    m_visibleRows = floor((data.boundingBox.height + iconGap) / (SMDH::ICON_HEIGHT + iconGap));

    scrollToCurrent();

    const u16 padding      = 2;
    size_t titleTextOffset = 0;

    for(size_t i = 0; i < m_loader->titles().size(); i++) {
        auto title = m_loader->titles()[i];

        CLAY_AUTO_ID({
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_PERCENT(1.0f),
                    .height = CLAY_SIZING_FIXED(SMDH::ICON_HEIGHT),
                },
                .padding        = CLAY_PADDING_ALL(padding),
                .childGap       = 5,
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor = (i == m_selectedTitle ? Theme::Surface1() : Theme::Surface0()),
        }) {
            const std::string& text = title->longDescription();
            Clay_String str         = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(text.size()), .chars = m_titleTexts.c_str() + titleTextOffset };

            if(m_titleTexts.size() < titleTextOffset + text.size()) {
                i = m_loader->titles().size();
                goto skipTitle;
            }

            TitleIcon(title, SMDH::ICON_WIDTH - padding, SMDH::ICON_HEIGHT - padding, {});
            titleTextOffset += text.size();

            // only render text if in view as the clipping messes up the parent clipping
            if(i >= m_scroll && i < m_scroll + m_visibleRows) {
                CLAY_AUTO_ID({ .clip = { .horizontal = true } }) {
                    CLAY_TEXT(str, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));
                }
            }

            HSPACER();

            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(68) }, .childAlignment = { .x = CLAY_ALIGN_X_RIGHT } } }) {
                if(m_client->cachedTitleInfoLoaded()) {
                    switch(title->outOfDate()) {
                    case 0:
                        CLAY_TEXT(CLAY_STRING("Synced"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));

                        break;
                    default:
                        CLAY_TEXT(CLAY_STRING("Not Synced"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));
                        break;
                    }
                }
                else {
                    CLAY_TEXT(CLAY_STRING("Unknown"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));
                }
            };

        skipTitle:
        }
    }
}

void MainScreen::renderTop() {
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
        CLAY(
            CLAY_ID("Header"),
            {
                .layout = {
                    .sizing = {
                        .width  = CLAY_SIZING_GROW(),
                        .height = CLAY_SIZING_FIXED(16),
                    },
                    .padding        = CLAY_PADDING_ALL(4),
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            CLAY_TEXT(CLAY_STRING("SaveSync"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            HSPACER();
            CLAY_TEXT(CLAY_STRING("v1.0.0"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_NONE }));
        }

        CLAY(
            CLAY_ID("Titles"),
            {
                .layout = {
                    .sizing = {
                        .width  = CLAY_SIZING_PERCENT(1.0),
                        .height = CLAY_SIZING_GROW(),
                    },
                    .padding         = { 0, 0, 5, 5 },
                    .childGap        = iconGap,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .clip = {
                    .vertical    = true,
                    .childOffset = {
                        .y = (m_scroll * -static_cast<float>(SMDH::ICON_HEIGHT + iconGap)),
                    },
                },
            }
        ) {
            if(m_loader->isLoadingTitles()) {
                CLAY(
                    CLAY_ID("LoadingIndicator"),
                    {
                        .layout = {
                            .sizing = {
                                .width  = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_GROW(),
                            },
                            .childAlignment = {
                                .x = CLAY_ALIGN_X_CENTER,
                                .y = CLAY_ALIGN_Y_CENTER,
                            },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                    }
                ) {
                    CLAY_TEXT(CLAY_STRING("Loading Titles..."), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 20 }));
                    CLAY_TEXT(m_loadedString, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));
                }
            }
            else {
                switch(m_config->layout()->value()) {
                case GRID: GridLayout(); break;
                case LIST: ListLayout(); break;
                default:   break;
                }
            }
        }

        CLAY(
            CLAY_ID("Footer"),
            {
                .layout = {
                    .sizing = {
                        .width  = CLAY_SIZING_GROW(),
                        .height = CLAY_SIZING_FIXED(16),
                    },
                    .padding        = CLAY_PADDING_ALL(4),
                    .childGap       = 1,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            Clay_TextElementConfig* textConfig = CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12 });

            BADGE(Theme::Save());
            CLAY_TEXT(CLAY_STRING("Save"), textConfig);

            HSPACER();

            BADGE(Theme::SaveAndExtdata());
            CLAY_TEXT(CLAY_STRING("Both"), textConfig);

            HSPACER();

            BADGE(Theme::Unknown());
            CLAY_TEXT(CLAY_STRING("Unknown"), textConfig);

            HSPACER();

            BADGE(Theme::Extdata());
            CLAY_TEXT(CLAY_STRING("Extdata"), textConfig);
        }

        if(m_yesNoActive || m_okActive || m_settingsScreen.isActive()) {
            YES_NO_OVERLAY;
        }
    }
}

void MainScreen::renderBottom() {
    if(m_settingsScreen.isActive()) {
        m_settingsScreen.renderBottom();
        return;
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
                    .sizing = {
                        .width  = CLAY_SIZING_GROW(),
                        .height = CLAY_SIZING_FIXED(16),
                    },
                    .padding        = CLAY_PADDING_ALL(4),
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            if(m_loader->isLoadingTitles()) {
                CLAY_TEXT(CLAY_STRING("No Title"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
            else {
                CLAY_TEXT(m_titleString, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
        }

        std::shared_ptr<Title> title = nullptr;
        if(m_loader->titles().size() > m_selectedTitle) {
            title = m_loader->titles()[m_selectedTitle];
        }

        if(m_loader->isLoadingTitles() || title == nullptr) {
            VSPACER();
            goto skipTitle;
        }

        CLAY(CLAY_ID("Details"), { .layout = { .sizing = { .width = CLAY_SIZING_PERCENT(1.0) }, .padding = CLAY_PADDING_ALL(2) } }) {
            CLAY(CLAY_ID("Info"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                CLAY_AUTO_ID({ .layout = { .childGap = 1, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }) {
                    if(m_client->serverOnline()) {
                        BADGE(Theme::Green());
                        CLAY_TEXT(CLAY_STRING("Server Online"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));
                    }
                    else {
                        BADGE(Theme::Red());
                        CLAY_TEXT(CLAY_STRING("Server Offline"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));
                    }
                }

                if(m_client->serverOnline() && (!m_client->cachedTitleInfoLoaded() || title->outOfDate() != 0)) {
                    CLAY_AUTO_ID({ .layout = { .childGap = 1, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }) {
                        if(!m_client->cachedTitleInfoLoaded()) {
                            BADGE(Theme::Unknown());
                            CLAY_TEXT(CLAY_STRING("Unknown if Out of Date"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));

                            goto skipOutOfDate;
                        }

                        switch(title->outOfDate()) {
                        case SAVE | EXTDATA:
                            BADGE(Theme::SaveAndExtdata());
                            CLAY_TEXT(CLAY_STRING("Save and Extdata Out of Date"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));

                            break;
                        case SAVE:
                            BADGE(Theme::Save());
                            CLAY_TEXT(CLAY_STRING("Save Out of Date"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));

                            break;
                        case EXTDATA:
                            BADGE(Theme::Extdata());
                            CLAY_TEXT(CLAY_STRING("Extdata Out of Date"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));

                            break;
                        default: break;
                        }

                    skipOutOfDate:
                    }
                }

                CLAY_TEXT(m_idString, CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));
                CLAY_TEXT(m_mediaTypeString, CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));
            }

            HSPACER();

            CLAY_AUTO_ID({ .layout = { .padding = { 0, 3, 3, 0 } } }) {
                CLAY(
                    CLAY_ID("Icon"),
                    {
                        .layout = {
                            .sizing = {
                                .width  = CLAY_SIZING_FIXED(SMDH::ICON_WIDTH),
                                .height = CLAY_SIZING_FIXED(SMDH::ICON_HEIGHT),
                            },
                        },
                        .image = {
                            .imageData = title->icon(),
                        },
                    }
                );
            };
        }

        VSPACER();

        CLAY(CLAY_ID("Buttons"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW() }, .padding = { 5, 5, 10, 10 } } }) {
            auto BUTTON = [this](Clay_ElementId id, Clay_String text, Clay_Color backgroundColor) {
                CLAY(
                    id,
                    {
                        .layout = {
                            .sizing         = { .width = CLAY_SIZING_GROW() },
                            .padding        = { 4, 4, 6, 6 },
                            .childAlignment = {
                                .x = CLAY_ALIGN_X_CENTER,
                                .y = CLAY_ALIGN_Y_CENTER,
                            },
                        },
                        .backgroundColor = backgroundColor,
                        .cornerRadius    = CLAY_CORNER_RADIUS(4),
                    }
                ) {
                    Clay_OnHover(&MainScreen::onButtonHover, reinterpret_cast<intptr_t>(this));
                    CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .textColor = Theme::ButtonText(), .fontSize = 14 }));
                }
            };

            CLAY(CLAY_ID("SaveButtons"), { .layout = { .childGap = 2, .childAlignment = { .x = CLAY_ALIGN_X_CENTER }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                CLAY_TEXT(CLAY_STRING("Save"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 13 }));

                BUTTON(CLAY_ID("SaveUpload"), CLAY_STRING("Upload \uE004+\uE000"), !title->getContainerFiles(SAVE).empty() ? Theme::Save() : Theme::ButtonDisabled());
                BUTTON(CLAY_ID("SaveDownload"), CLAY_STRING("Download \uE004+\uE001"), title->containerAccessible(SAVE) ? Theme::Save() : Theme::ButtonDisabled());
            }

            CLAY(
                CLAY_ID("NetworkDisplay"),
                {
                    .layout = {
                        .sizing = {
                            .width  = CLAY_SIZING_GROW(),
                            .height = CLAY_SIZING_GROW(),
                        },
                        .childGap       = 2,
                        .childAlignment = {
                            .x = CLAY_ALIGN_X_CENTER,
                            .y = CLAY_ALIGN_Y_BOTTOM,
                        },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                }
            ) {
                Clay_Color color = Theme::Accent();
                switch(m_client->currentRequest().type) {
                case QueuedRequest::UPLOAD_SAVE:
                case QueuedRequest::DOWNLOAD_SAVE:
                    color = Theme::Save();
                    break;
                case QueuedRequest::UPLOAD_EXTDATA:
                case QueuedRequest::DOWNLOAD_EXTDATA:
                    color = Theme::Extdata();
                    break;
                default: break;
                }

                if(!m_client->processingQueueRequest() || !m_client->showRequestProgress()) {
                    goto skipProgress;
                }

                CLAY(CLAY_ID("RequestText"), { .layout = { .sizing = { .width = CLAY_SIZING_PERCENT(0.8), .height = CLAY_SIZING_FIXED(24) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } }, .clip = { .vertical = true } }) {
                    CLAY_TEXT(m_networkRequestString, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
                }

                CLAY(
                    CLAY_ID("RequestProgressOuter"),
                    {
                        .layout = {
                            .sizing = {
                                .width  = CLAY_SIZING_PERCENT(0.8),
                                .height = CLAY_SIZING_FIXED(12),
                            },
                            .padding        = { 6, 6, 2, 2 },
                            .childAlignment = {
                                .x = CLAY_ALIGN_X_CENTER,
                                .y = CLAY_ALIGN_Y_CENTER,
                            },
                        },
                        .cornerRadius = CLAY_CORNER_RADIUS(4),

                        .border = {
                            .color = color,
                            .width = CLAY_BORDER_OUTSIDE(2),
                        },
                    },
                ) {
                    CLAY(
                        CLAY_ID("RequestProgress"),
                        {
                            .layout = {
                                .sizing = {
                                    .width  = CLAY_SIZING_PERCENT((m_client->requestProgressCurrent() / (float)CLAY__MAX(1, m_client->requestProgressMax()))),
                                    .height = CLAY_SIZING_FIXED(4),
                                },
                            },
                            .backgroundColor = color,
                        }
                    );
                }

            skipProgress:
                if(m_client->requestQueueSize() > m_client->processingQueueRequest() ? 1 : 0) {
                    CLAY_TEXT(m_networkQueueString, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 10 }));
                }
                else {
                    VSPACER(10);
                }
            }

            CLAY(CLAY_ID("ExtdataButtons"), { .layout = { .childGap = 2, .childAlignment = { .x = CLAY_ALIGN_X_CENTER }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                CLAY_TEXT(CLAY_STRING("Extdata"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 13 }));

                BUTTON(CLAY_ID("ExtdataUpload"), CLAY_STRING("Upload \uE005+\uE000"), !title->getContainerFiles(EXTDATA).empty() ? Theme::Extdata() : Theme::ButtonDisabled());
                BUTTON(CLAY_ID("ExtdataDownload"), CLAY_STRING("Download \uE005+\uE001"), title->containerAccessible(EXTDATA) ? Theme::Extdata() : Theme::ButtonDisabled());
            }
        }

    skipTitle:
        CLAY(
            CLAY_ID("Footer"),
            {
                .layout = {
                    .sizing = {
                        .width  = CLAY_SIZING_GROW(),
                        .height = CLAY_SIZING_FIXED(16),
                    },
                    .childAlignment = {
                        .x = CLAY_ALIGN_X_CENTER,
                        .y = CLAY_ALIGN_Y_CENTER,
                    },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            CLAY(CLAY_ID("Settings")) {
                Clay_OnHover(&MainScreen::onButtonHover, reinterpret_cast<intptr_t>(this));
                CLAY_TEXT(CLAY_STRING("Press \uE003 for Settings"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));
            }
        }

        auto BUTTON = [this](Clay_ElementId id, Clay_String text, Clay_Color backgroundColor, Clay_BorderElementConfig border = {}) {
            CLAY(
                id,
                {
                    .layout = {
                        .sizing = {
                            .width  = CLAY_SIZING_FIXED(25),
                            .height = CLAY_SIZING_FIT(),
                        },
                        .padding        = { 4, 4, 4, 4 },
                        .childAlignment = {
                            .x = CLAY_ALIGN_X_CENTER,
                            .y = CLAY_ALIGN_Y_CENTER,
                        },
                    },
                    .backgroundColor = backgroundColor,
                    .cornerRadius    = CLAY_CORNER_RADIUS(4),
                    .border          = border,
                }
            ) {
                Clay_OnHover(&MainScreen::onButtonHover, reinterpret_cast<intptr_t>(this));
                CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .textColor = Theme::ButtonText(), .fontSize = 12 }));
            }
        };

        if(m_yesNoActive) {
            YES_NO_OVERLAY {
                CLAY(
                    CLAY_ID("YesNoBody"),
                    {
                        .layout = {
                            .padding         = CLAY_PADDING_ALL(8),
                            .childGap        = 12,
                            .childAlignment  = { .x = CLAY_ALIGN_X_CENTER },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = Theme::Surface2(),
                        .cornerRadius    = CLAY_CORNER_RADIUS(4),
                    }
                ) {
                    CLAY_TEXT(m_yesNoString, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 14 }));

                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW() } } }) {
                        Clay_BorderElementConfig selectedBorder    = { .color = borderColor(1), .width = CLAY_BORDER_ALL(2) };
                        Clay_BorderElementConfig notSelectedBorder = {};

                        BUTTON(CLAY_ID("Yes"), CLAY_STRING("Yes"), { 0x5C, 0x92, 0x51, 0xFF }, m_yesSelected ? selectedBorder : notSelectedBorder);
                        HSPACER();
                        BUTTON(CLAY_ID("No"), CLAY_STRING("No"), { 0xC9, 0x55, 0x55, 0xFF }, m_yesSelected ? notSelectedBorder : selectedBorder);
                    }
                }
            }
        }
        else if(m_okActive) {
            YES_NO_OVERLAY {
                CLAY(
                    CLAY_ID("OkBody"),
                    {
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_FIT(0, BOTTOM_SCREEN_WIDTH / 1.5f),
                            },
                            .padding         = CLAY_PADDING_ALL(8),
                            .childGap        = 12,
                            .childAlignment  = { .x = CLAY_ALIGN_X_CENTER },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = Theme::Surface2(),
                        .cornerRadius    = CLAY_CORNER_RADIUS(4),
                    }
                ) {
                    CLAY_TEXT(m_okString, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12 }));
                    BUTTON(CLAY_ID("Ok"), CLAY_STRING("Ok"), { 0x5C, 0x92, 0x51, 0xFF }, { .color = borderColor(1), .width = CLAY_BORDER_ALL(2) });
                }
            }
        }
    }
}