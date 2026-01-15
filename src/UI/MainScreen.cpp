#include <Config.hpp>
#include <SaveSync_gfx.h>
#include <Theme.hpp>
#include <UI/MainScreen.hpp>
#include <Util/ClayDefines.hpp>
#include <Util/EmuUtil.hpp>
#include <Util/StringUtil.hpp>
#include <clay.h>
#include <clay_renderer_C2D.hpp>

#define BADGE(...)                                                                          \
    CLAY_AUTO_ID({                                                                          \
        .layout = {                                                                         \
            .sizing = {                                                                     \
                .width  = CLAY_SIZING_FIXED(8),                                             \
                .height = CLAY_SIZING_FIXED(8),                                             \
            },                                                                              \
        },                                                                                  \
        .backgroundColor = __VA_ARGS__,                                                     \
        .cornerRadius    = CLAY_CORNER_RADIUS(8),                                           \
        .border          = { .color = { 0, 0, 0, 0xFF }, .width = CLAY_BORDER_OUTSIDE(1) }, \
    })

void MainScreen::onClientRequestFailed(std::string status) {
    m_okText   = status;
    m_okString = Clay_String{ .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_okText.size()), .chars = m_okText.c_str() };

    m_okActive = true;
}

void MainScreen::resetScroll() {
    m_scroll = 0;
}

MainScreen::MainScreen(std::shared_ptr<Config> config, std::shared_ptr<TitleLoader> loader, std::shared_ptr<Client> client)
    : m_config(config)
    , m_loader(loader)
    , m_client(client)
    , m_settingsScreen(std::make_unique<SettingsScreen>(config))
    , m_selectedTitle(0)
    , m_sprites("romfs:/SaveSync_gfx.t3x") {
    m_client->requestFailedSignal.connect<&MainScreen::onClientRequestFailed>(this);
    m_config->layout()->changedEmptySignal.connect<&MainScreen::resetScroll>(this);
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

void MainScreen::tryUpload() {
    if(m_selectedTitle >= m_loader->titles().size()) {
        return;
    }

    auto title = m_loader->titles()[m_selectedTitle];
    if(title->getContainerFiles(SAVE).empty() && title->getContainerFiles(EXTDATA).empty()) {
        return;
    }

    showYesNo("Upload Files", QueuedRequest::UPLOAD, title);
}

void MainScreen::tryDownload() {
    if(m_selectedTitle >= m_loader->titles().size()) {
        return;
    }

    auto title = m_loader->titles()[m_selectedTitle];
    showYesNo("Download", QueuedRequest::DOWNLOAD, title);
}

void MainScreen::update() {
    if(m_settingsScreen->isActive()) {
        m_settingsScreen->update();
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

    if(
        kDown & KEY_Y &&
        !(kDown & KEY_L || kHeld & KEY_L) &&
        !(kDown & KEY_R || kHeld & KEY_R)
    ) {
        m_settingsScreen->setActive();
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
        tryUpload();
    }
    else if(kHeld & KEY_L && kDown & KEY_B && title != nullptr) {
        tryDownload();
    }
    // TODO: bulk here
    else if(kHeld & KEY_R && kDown & KEY_A && title != nullptr) {
    }
    else if(kHeld & KEY_R && kDown & KEY_B && title != nullptr) {
    }

    if(m_updateTitleInfo) {
        if(title != nullptr) {
            m_titleText = title->name();
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

    if(elementId.id == CLAY_ID("TitleUpload").id) {
        tryUpload();
    }
    else if(elementId.id == CLAY_ID("TitleDownload").id) {
        tryDownload();
    }
    else if(elementId.id == CLAY_ID("BulkUpload").id) {
        // tryUpload();
    }
    else if(elementId.id == CLAY_ID("BulkDownload").id) {
        // tryDownload();
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
        m_settingsScreen->setActive();
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
void MainScreen::TitleIcon(std::shared_ptr<Title> title, float width, float height, Clay_BorderElementConfig border) {
    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height) } }, .image = { .imageData = title->icon() }, .border = border }) {
        Clay_Color color = Theme::Unknown();
        if(!title->invalidHash() && m_client->cachedTitleInfoLoaded()) {
            if(title->isOutOfSync()) {
                color = Theme::OutOfSync();
            }
            else {
                goto skipBadge;
            }
        }

        CLAY_AUTO_ID({
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
            },
        }) {
            BADGE(color);
        }

    skipBadge:
    }
}

inline void MainScreen::GridLayout() {
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

    const u16 padding = 2;
    for(size_t i = m_renderedScroll; i < CLAY__MIN(m_renderedScroll + m_visibleRows + 1U, m_loader->titles().size()); i++) {
        auto title = m_loader->titles()[i];

        Clay_Color networkColor = Theme::Background();
        if(m_client->processingQueueRequest()) {
            QueuedRequest request = m_client->currentRequest();
            if(request.title != title) {
                goto skipNetworkProgress;
            }

            switch(request.type) {
            case QueuedRequest::UPLOAD:   networkColor = Theme::Upload(); break;
            case QueuedRequest::DOWNLOAD: networkColor = Theme::Download(); break;
            default:                      break;
            }
        }

    skipNetworkProgress:
        CLAY_AUTO_ID({
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_PERCENT(1.0f),
                    .height = CLAY_SIZING_FIXED(SMDH::ICON_HEIGHT),
                },
                .padding        = { padding, padding + 3, padding, padding },
                .childGap       = 5,
                .childAlignment = {
                    .y = CLAY_ALIGN_Y_CENTER,
                },
            },
            .backgroundColor = (i == m_selectedTitle ? Theme::Surface1() : Theme::Surface0()),
        }) {
            const char* text = title->staticName();
            Clay_String str  = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(strlen(text)), .chars = text };

            QueuedRequest request = m_client->currentRequest();

            TitleIcon(title, SMDH::ICON_WIDTH - padding, SMDH::ICON_HEIGHT - padding, {});
            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW() }, .childGap = 2, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                CLAY_TEXT(str, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 16 }));
                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() }, .padding = { 3, 0, 0, 0 } } }) {
                    Clay_TextElementConfig* textConfig = CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext1(), .fontSize = 14 });

                    if(request.title == title) {
                        switch(request.type) {
                        case QueuedRequest::UPLOAD:   CLAY_TEXT(CLAY_STRING("Uploading"), textConfig); break;
                        case QueuedRequest::DOWNLOAD: CLAY_TEXT(CLAY_STRING("Downloading"), textConfig); break;
                        default:                      break;
                        }

                        CLAY_AUTO_ID({
                            .layout = {
                                .sizing         = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
                                .padding        = { 20, 20, 0, 0 },
                                .childAlignment = {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                },
                            },
                        }) {
                            CLAY_AUTO_ID(
                                {
                                    .layout = {
                                        .sizing = {
                                            .width  = CLAY_SIZING_PERCENT(1.0),
                                            .height = CLAY_SIZING_FIXED(3),
                                        },
                                    },
                                    .backgroundColor = Theme::ButtonDisabled(),
                                },
                            ) {
                                CLAY_AUTO_ID(
                                    {
                                        .layout = {
                                            .sizing = {
                                                .width  = CLAY_SIZING_PERCENT(m_client->requestProgressCurrent() / CLAY__MAX(static_cast<float>(m_client->requestProgressMax()), 1.0f)),
                                                .height = CLAY_SIZING_GROW(),
                                            },
                                        },
                                        .backgroundColor = networkColor,
                                    },
                                );
                            }
                        }
                    }
                    else {
                        if(m_client->cachedTitleInfoLoaded()) {
                            if(title->isOutOfSync()) {
                                CLAY_TEXT(CLAY_STRING("Not Synced"), textConfig);
                            }
                            else {
                                CLAY_TEXT(CLAY_STRING("Synced"), textConfig);
                            }
                        }
                        else {
                            CLAY_TEXT(CLAY_STRING("Unknown"), textConfig);
                        }

                        HSPACER();
                        for(auto req : m_client->requestQueue()) {
                            if(req.title != title) {
                                continue;
                            }

                            switch(req.type) {
                            case QueuedRequest::UPLOAD:
                                CLAY_TEXT(CLAY_STRING("Upload Queued"), textConfig);
                                break;
                            case QueuedRequest::DOWNLOAD:
                                CLAY_TEXT(CLAY_STRING("Download Queued"), textConfig);
                                break;
                            default: break;
                            }

                            HSPACER();
                            break;
                        }

                        Clay_String sizeStr = { .isStaticallyAllocated = true, .length = static_cast<int32_t>(strlen(title->totalSizeStr())), .chars = title->totalSizeStr() };
                        CLAY_TEXT(sizeStr, textConfig);
                    }
                }
            }

            if(request.title == title) {
                CLAY_AUTO_ID({ .layout = { .padding = { 4, 4, 0, 0 }, .childGap = 2, .childAlignment = { .x = CLAY_ALIGN_X_CENTER }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                    Clay_TextElementConfig* textConfig = CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext1(), .fontSize = 12 });

                    char sizeText[32];

                    int lenCur = StringUtil::formatFileSize(m_client->requestProgressCurrent(), sizeText, sizeof(sizeText));
                    int lenMax = StringUtil::formatFileSize(m_client->requestProgressMax(), sizeText + lenCur, sizeof(sizeText) - static_cast<size_t>(lenCur));

                    Clay_String curStr = { .isStaticallyAllocated = true, .length = lenCur, .chars = sizeText };
                    CLAY_TEXT(curStr, textConfig);

                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2) } }, .backgroundColor = Theme::ButtonDisabled() });

                    Clay_String maxStr = { .isStaticallyAllocated = false, .length = lenMax, .chars = sizeText + lenCur };
                    CLAY_TEXT(maxStr, textConfig);
                }
            }
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
                        .width  = CLAY_SIZING_FIXED(TOP_SCREEN_WIDTH),
                        .height = CLAY_SIZING_FIXED(16),
                    },
                    .padding        = { 8, 8, 0, 0 },
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            Clay_TextElementConfig* textConfig = CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_NONE, .textAlignment = CLAY_TEXT_ALIGN_CENTER });
            CLAY(CLAY_ID("HeaderLeft"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW() } } }) {
                CLAY_AUTO_ID({ .layout = { .childGap = 2, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } } }) {
                    switch(m_client->serverOnline()) {
                    case true:
                        BADGE(Theme::Green());
                        CLAY_TEXT(CLAY_STRING("Server Online"), textConfig);

                        break;
                    default:
                        BADGE(Theme::Red());
                        CLAY_TEXT(CLAY_STRING("Server Offline"), textConfig);

                        break;
                    }
                }
            }

            CLAY(CLAY_ID("HeaderMiddle"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW() }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } } }) {
#define _STRINGIFY(a) #a
#define STRINGIFY(a)  _STRINGIFY(a)
                CLAY_TEXT(CLAY_STRING(EXE_NAME " v" STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)), textConfig);
#undef _STRINGIFY
#undef STRINGIFY
            }

            CLAY(CLAY_ID("HeaderRight"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW() }, .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER } } }) {
                time_t timeInfo;
                time(&timeInfo);

                char clockText[6];
                size_t clockLen = strftime(clockText, sizeof(clockText), "%H:%M", localtime(&timeInfo));

                Clay_String clockStr = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(clockLen), .chars = clockText };
                CLAY_TEXT(clockStr, textConfig);

                if(!EmulatorUtil::isEmulated()) {
                    HSPACER(5);
                    C2D_Image* wifiImage = m_sprites.image(SaveSync_gfx_wifi_off_idx);

                    bool wifiEnabled = m_client->wifiEnabled();
                    if(wifiEnabled) {
                        int wifiStrength = osGetWifiStrength();

                        switch(wifiStrength) {
                        case 0:  wifiImage = m_sprites.image(SaveSync_gfx_wifi_terrible_idx); break;
                        case 1:  wifiImage = m_sprites.image(SaveSync_gfx_wifi_bad_idx); break;
                        case 2:  wifiImage = m_sprites.image(SaveSync_gfx_wifi_decent_idx); break;
                        case 3:
                        default: wifiImage = m_sprites.image(SaveSync_gfx_wifi_good_idx); break;
                        }
                    }

                    CLAY(
                        CLAY_ID("WifiStrength"),
                        {
                            .layout = {
                                .sizing = {
                                    .width  = CLAY_SIZING_FIXED(static_cast<float>(wifiImage->subtex->width) / 2.0f),
                                    .height = CLAY_SIZING_FIXED(static_cast<float>(wifiImage->subtex->height) / 2.0f),
                                },
                            },
                            .image = { .imageData = wifiImage },
                        }
                    );

                    size_t batteryImageIDX = SaveSync_gfx_battery_0_idx;
                    static u8 prevPercent  = std::numeric_limits<u8>::max();

                    u8 charging;
                    u8 percent;
                    u8 level;

                    if(R_FAILED(PTMU_GetBatteryChargeState(&charging))) {
                        charging = 0;
                    }

                    if(R_FAILED(PTMU_GetBatteryLevel(&level))) {
                        level = 0;
                    }

                    if(R_FAILED(MCUHWC_GetBatteryLevel(&percent))) {
                        percent = 0;
                    }

                    batteryImageIDX += level + (charging * 6U);
                    C2D_Image* batteryImage = m_sprites.image(batteryImageIDX);

                    CLAY(
                        CLAY_ID("BatteryLevel"),
                        {
                            .layout = {
                                .sizing = {
                                    .width  = CLAY_SIZING_FIXED(static_cast<float>(batteryImage->subtex->width) / 2.0f),
                                    .height = CLAY_SIZING_FIXED(static_cast<float>(batteryImage->subtex->height) / 2.0f),
                                },
                            },
                            .image = { .imageData = batteryImage },
                        }
                    );

                    if(prevPercent != percent) {
                        m_percentTextLen = snprintf(m_percentText, sizeof(m_percentText), "%d%%", percent);
                        prevPercent      = percent;
                    }

                    Clay_String percentStr = { .isStaticallyAllocated = true, .length = m_percentTextLen, .chars = m_percentText };
                    CLAY_TEXT(percentStr, textConfig);
                }
            }
        }

        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } }) {
            QueuedRequest request = m_client->currentRequest();
            bool showSidebar      = m_config->layout()->value() == GRID && request.title != nullptr;

            const float sidebarWidth     = 0.37f;
            const float gridSidebarWidth = 1.0f - sidebarWidth;
            m_renderedScroll             = m_scroll;

            CLAY(
                CLAY_ID("Titles"),
                {
                    .layout = {
                        .sizing = {
                            .width  = CLAY_SIZING_PERCENT(showSidebar ? gridSidebarWidth : 1.0f),
                            .height = CLAY_SIZING_GROW(),
                        },
                        .padding         = { 0, 0, 5, 5 },
                        .childGap        = iconGap,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                    .clip = {
                        .vertical    = true,
                        .childOffset = {
                            .y = (m_config->layout()->value() != LIST ? m_renderedScroll * -static_cast<float>(SMDH::ICON_HEIGHT + iconGap) : 0),
                        },
                    },
                }
            ) {
                if(m_loader->isLoadingTitles()) {
                    size_t loadedTitles = m_loader->titlesLoaded();
                    if(m_prevLoadedTitles != loadedTitles) {
                        m_loadedText   = std::format("{}/{} Loaded", loadedTitles, m_loader->totalTitles());
                        m_loadedString = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(m_loadedText.size()), .chars = m_loadedText.c_str() };

                        m_prevLoadedTitles = loadedTitles;
                    }

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

            if(showSidebar) {
                CLAY(
                    CLAY_ID("Sidebar"),
                    {
                        .layout = {
                            .sizing = {
                                .width  = CLAY_SIZING_PERCENT(sidebarWidth),
                                .height = CLAY_SIZING_GROW(),
                            },
                            .padding        = { 4, 4, 20, 20 },
                            .childGap       = 4,
                            .childAlignment = {
                                .x = CLAY_ALIGN_X_CENTER,
                            },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = Theme::Surface1(),
                    }
                ) {
                    Clay_TextElementConfig* largeTextConfig  = CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 18, .textAlignment = CLAY_TEXT_ALIGN_CENTER });
                    Clay_TextElementConfig* textConfig       = CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .textAlignment = CLAY_TEXT_ALIGN_CENTER });
                    Clay_TextElementConfig* textConfigDarker = CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext2(), .fontSize = 12, .textAlignment = CLAY_TEXT_ALIGN_CENTER });
                    Clay_TextElementConfig* smallTextConfig  = CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 11, .textAlignment = CLAY_TEXT_ALIGN_CENTER });

                    const char* text = request.title->staticName();
                    Clay_String str  = { .isStaticallyAllocated = false, .length = static_cast<int32_t>(strlen(text)), .chars = text };

                    CLAY_TEXT(str, largeTextConfig);
                    Clay_Color networkColor = Theme::Unknown();

                    switch(request.type) {
                    case QueuedRequest::UPLOAD:
                        CLAY_TEXT(CLAY_STRING("Uploading"), textConfigDarker);
                        networkColor = Theme::Upload();

                        break;
                    case QueuedRequest::DOWNLOAD:
                        CLAY_TEXT(CLAY_STRING("Downloading"), textConfigDarker);
                        networkColor = Theme::Download();

                        break;
                    default: break;
                    }

                    VSPACER();

                    CLAY_AUTO_ID({ .layout = { .childGap = 2 } }) {
                        const char* separatorText = " / ";

                        char networkProgressText[32];
                        int lenCur = StringUtil::formatFileSize(m_client->requestProgressCurrent(), networkProgressText, sizeof(networkProgressText));
                        int offset = lenCur;

                        if(offset < static_cast<int>(sizeof(networkProgressText))) {
                            strcpy(networkProgressText + offset, separatorText);
                            offset += static_cast<int32_t>(sizeof(separatorText)) - 1;
                            offset += StringUtil::formatFileSize(m_client->requestProgressMax(), networkProgressText + offset, sizeof(networkProgressText) - static_cast<size_t>(offset));

                            Clay_String networkProgressStr = { .isStaticallyAllocated = true, .length = offset, .chars = networkProgressText };
                            CLAY_TEXT(networkProgressStr, smallTextConfig);
                        }
                    }

                    float percent = m_client->requestProgressCurrent() / static_cast<float>(CLAY__MAX(m_client->requestProgressMax(), 1.0f));
                    CLAY(
                        CLAY_ID("RequestProgressOuter"),
                        {
                            .layout = {
                                .sizing = {
                                    .width  = CLAY_SIZING_PERCENT(0.8),
                                    .height = CLAY_SIZING_FIXED(12),
                                },
                                .padding        = { 6, 6, 2, 2 },
                                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                            },
                            .cornerRadius = CLAY_CORNER_RADIUS(4),

                            .border = {
                                .color = networkColor,
                                .width = CLAY_BORDER_OUTSIDE(2),
                            },
                        },
                    ) {
                        CLAY(
                            CLAY_ID("RequestProgressInner"),
                            {
                                .layout = {
                                    .sizing = {
                                        .width  = CLAY_SIZING_PERCENT(1.0),
                                        .height = CLAY_SIZING_FIXED(4),
                                    },
                                },
                                .backgroundColor = Theme::ButtonDisabled(),
                            }
                        ) {
                            CLAY(
                                CLAY_ID("RequestProgress"),
                                {
                                    .layout = {
                                        .sizing = {
                                            .width  = CLAY_SIZING_PERCENT(percent),
                                            .height = CLAY_SIZING_FIXED(4),
                                        },
                                    },
                                    .backgroundColor = networkColor,
                                }
                            );
                        }
                    }

                    char progressPercentText[5];
                    int lenPercent = snprintf(progressPercentText, sizeof(progressPercentText), "%d%%", static_cast<int>(percent * 100));

                    Clay_String percentStr = { .isStaticallyAllocated = true, .length = lenPercent, .chars = progressPercentText };
                    CLAY_TEXT(percentStr, textConfig);
                }
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
        HSPACER();

        BADGE(Theme::OutOfSync());
        CLAY_TEXT(CLAY_STRING("Unsynced"), textConfig);

        HSPACER();

        BADGE(Theme::Unknown());
        CLAY_TEXT(CLAY_STRING("Unknown"), textConfig);

        HSPACER();
    }

    if(m_yesNoActive || m_okActive || m_settingsScreen->isActive()) {
        YES_NO_OVERLAY;
    }
}

void MainScreen::renderBottom() {
    if(m_settingsScreen->isActive()) {
        m_settingsScreen->renderBottom();
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
                    .padding        = CLAY_PADDING_ALL(2),
                    .childGap       = 4,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = Theme::Surface2(),
            }
        ) {
            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW() } }, .clip = { .horizontal = true } }) {
                if(m_loader->isLoadingTitles()) {
                    CLAY_TEXT(CLAY_STRING("No Title"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_NONE }));
                }
                else {
                    CLAY_TEXT(m_titleString, CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 12, .wrapMode = CLAY_TEXT_WRAP_NONE }));
                }
            }

            C2D_Image* image = nullptr;
            switch(m_client->currentRequest().type) {
            case QueuedRequest::UPLOAD:   image = m_sprites.image(SaveSync_gfx_sync_upload_idx); break;
            case QueuedRequest::DOWNLOAD: image = m_sprites.image(SaveSync_gfx_sync_download_idx); break;
            default:                      break;
            }

            if(image != nullptr) {
                CLAY(
                    CLAY_ID("SyncStatus"),
                    {
                        .layout = {
                            .sizing = {
                                .width  = CLAY_SIZING_FIXED(image->subtex->width / 2.0f),
                                .height = CLAY_SIZING_FIXED(image->subtex->height / 2.0f),
                            },
                        },
                        .image = { .imageData = image },
                    }
                );
            }
        }

        std::shared_ptr<Title> title;
        if(
            m_loader->isLoadingTitles() ||
            m_loader->titles().size() <= m_selectedTitle ||
            m_loader->titles()[m_selectedTitle] == nullptr
        ) {
            VSPACER();
            goto skipTitle;
        }

        title = m_loader->titles()[m_selectedTitle];
        CLAY(CLAY_ID("Details"), { .layout = { .sizing = { .width = CLAY_SIZING_PERCENT(1.0) }, .padding = CLAY_PADDING_ALL(2) } }) {
            CLAY(CLAY_ID("Info"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                CLAY_AUTO_ID({ .layout = { .childGap = 1, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }) {
                    if(!m_client->cachedTitleInfoLoaded()) {
                        BADGE(Theme::Unknown());
                        CLAY_TEXT(CLAY_STRING("Unknown"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));
                    }
                    else if(title->isOutOfSync()) {
                        BADGE(Theme::OutOfSync());
                        CLAY_TEXT(CLAY_STRING("Out of Sync"), CLAY_TEXT_CONFIG({ .textColor = Theme::Subtext0(), .fontSize = 12 }));
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

            CLAY(CLAY_ID("TitleButtons"), { .layout = { .childGap = 2, .childAlignment = { .x = CLAY_ALIGN_X_CENTER }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                CLAY_TEXT(CLAY_STRING("Title"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 13 }));

                BUTTON(CLAY_ID("TitleUpload"), CLAY_STRING("Upload \uE004+\uE000"), !title->getContainerFiles(SAVE).empty() || !title->getContainerFiles(EXTDATA).empty() ? Theme::OutOfSync() : Theme::ButtonDisabled());
                BUTTON(CLAY_ID("TitleDownload"), CLAY_STRING("Download \uE004+\uE001"), Theme::OutOfSync());
            }

            HSPACER();

            CLAY(CLAY_ID("BulkButtons"), { .layout = { .childGap = 2, .childAlignment = { .x = CLAY_ALIGN_X_CENTER }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
                CLAY_TEXT(CLAY_STRING("Bulk"), CLAY_TEXT_CONFIG({ .textColor = Theme::Text(), .fontSize = 13 }));

                BUTTON(CLAY_ID("BulkUpload"), CLAY_STRING("Upload \uE005+\uE000"), Theme::Bulk());
                BUTTON(CLAY_ID("BulkDownload"), CLAY_STRING("Download \uE005+\uE001"), Theme::Bulk());
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