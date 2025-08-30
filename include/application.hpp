#ifndef __APPLICATION_HPP__
#define __APPLICATION_HPP__

#include <3ds.h>

#include <client.hpp>
#include <directory.hpp>
#include <loader.hpp>
#include <memory>
#include <title.hpp>
#include <vector>

#define ICON_WIDTH 48
#define ICON_HEIGHT 48

#define TOP_SCREEN_WIDTH GSP_SCREEN_HEIGHT_TOP
#define TOP_SCREEN_HEIGHT GSP_SCREEN_WIDTH

#define BOTTOM_SCREEN_WIDTH GSP_SCREEN_HEIGHT_BOTTOM
#define BOTTOM_SCREEN_HEIGHT GSP_SCREEN_WIDTH

class Application {
private:
    u32 clrClear;
    u32 clrWhite;
    u32 clrBlack;

public:
    C2D_Text test;
    C2D_TextBuf testBuf;

    C2D_Text pathsText;
    C2D_TextBuf pathsBuf;

    Application()
        : clrClear(C2D_Color32(0xFF, 0xD8, 0xB0, 0x68))
        , clrWhite(C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF))
        , clrBlack(C2D_Color32(0x00, 0x00, 0x00, 0xFF)) {
        gfxInitDefault();

        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        C2D_Prepare();

        amInit();

        Archive::init();
        TitleLoader::instance();

        m_top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        m_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

        testBuf  = C2D_TextBufNew(128);
        pathsBuf = C2D_TextBufNew(256);
    }

    virtual ~Application() {
        C2D_TextBufDelete(testBuf);

        TitleLoader::closeInstance();
        Archive::exit();

        amExit();

        C2D_Fini();
        C3D_Fini();

        gfxExit();
    }

    virtual bool loop() {
        if(!aptMainLoop()) {
            setShouldExit(true);
        }
        else {
            update();
            render();
        }

        return !m_shouldExit;
    }

    bool shouldExit() { return m_shouldExit; }
    void setShouldExit(bool shouldExit) { m_shouldExit = shouldExit; }

protected:
    std::vector<std::u16string> foundPaths;
    Result walkDir(FS_Archive& archive, std::u16string path) {
        Directory items(archive, path);

        if(!items.good()) {
            return items.error();
        }

        for(size_t i = 0; i < items.size(); i++) {
            std::u16string newPath = path + items.entry(i);

            if(items.folder(i)) {
                walkDir(archive, newPath + u"/");
            }
            else {
                foundPaths.push_back(newPath);
            }
        }

        return RL_SUCCESS;
    }

    virtual void update() {
        hidScanInput();

        u32 kDown = hidKeysDown();
        if(kDown & KEY_START) {
            setShouldExit(true);
        }

        auto& titles = TitleLoader::instance()->titles();

        bool updateSelected = false;
        if(kDown & KEY_LEFT) {
            u32 prevRow = m_selectedTitle / COLUMNS;
            u32 newRow  = (m_selectedTitle - 1) / COLUMNS;

            if(newRow == prevRow) {
                m_selectedTitle = (m_selectedTitle - 1) % titles.size();
                updateSelected  = true;
            }
        }

        if(kDown & KEY_RIGHT) {
            u32 prevRow = m_selectedTitle / COLUMNS;
            u32 newRow  = (m_selectedTitle + 1) / COLUMNS;

            if(newRow == prevRow) {
                m_selectedTitle = std::min(m_selectedTitle + 1, (u32)titles.size() - 1);
                updateSelected  = true;
            }
        }

        if(kDown & KEY_UP) {
            u32 prevRow = m_selectedTitle / COLUMNS;

            if(m_selectedTitle >= COLUMNS) {
                m_selectedTitle -= COLUMNS;
            }
            else if(prevRow != 0) {
                m_selectedTitle = 0;
            }

            updateSelected = true;
        }

        if(kDown & KEY_DOWN) {
            u32 prevRow = m_selectedTitle / COLUMNS;
            u32 curRows = titles.size() / COLUMNS;

            if(m_selectedTitle + COLUMNS < titles.size()) {
                m_selectedTitle += COLUMNS;
            }
            else if(prevRow != curRows) {
                m_selectedTitle = titles.size() - 1;
            }

            updateSelected = true;
        }

        if(updateSelected) {
            char buf[128];
            C2D_TextBufClear(testBuf);
            sprintf(buf, "%lu %s", m_selectedTitle, titles[m_selectedTitle]->longDescription().c_str());
            C2D_TextParse(&test, testBuf, buf);
            C2D_TextOptimize(&test);

            FS_Archive archive;
            std::shared_ptr<Title> title = titles[m_selectedTitle];
            Result res                   = Archive::save(&archive, title->mediaType(), title->lowID(), title->highID());

            foundPaths.clear();

            if(R_SUCCEEDED(res)) {
                walkDir(archive, u"/");
                FSUSER_CloseArchive(archive);
            }

            std::string test = "paths: " + std::to_string(foundPaths.size()) + "\n";
            C2D_TextBufClear(pathsBuf);

            for(auto& path : foundPaths) {
                test += toUTF8(path) + "\n";
            }

            C2D_TextParse(&pathsText, pathsBuf, test.c_str());
            C2D_TextOptimize(&pathsText);
        }
    }

    virtual void render() {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        renderTop();
        renderBottom();

        C3D_FrameEnd(0);
    }

    virtual void renderTop() {
        C2D_TargetClear(m_top, clrClear);
        C2D_SceneBegin(m_top);

        auto& titles = TitleLoader::instance()->titles();

        float offset = 0.0f;
        if(titles.size() > m_selectedTitle) {
            C2D_DrawImageAt(titles[m_selectedTitle]->icon(), 0.0f, 0.0f, 0.0f);
            offset = ICON_WIDTH;
        }

        C2D_DrawText(&test, C2D_AlignLeft | C2D_WordWrap, offset, 0.0f, 0.0f, 1.0f, 1.0f, (float)(TOP_SCREEN_WIDTH - offset));
        C2D_DrawText(&pathsText, C2D_AlignLeft | C2D_WordWrap, 5, ICON_HEIGHT + 1, 0.0f, 1.0f, 1.0f, (float)(TOP_SCREEN_WIDTH - 5));
    }

    virtual void renderBottom() {
        C2D_TargetClear(m_bottom, clrClear);
        C2D_SceneBegin(m_bottom);

        auto& titles = TitleLoader::instance()->titles();

        C2D_ImageTint selectedTint = {
            .corners = {
                        { .color = clrWhite, .blend = 0.5f },
                        { .color = clrWhite, .blend = 0.5f },
                        { .color = clrWhite, .blend = 0.5f },
                        { .color = clrWhite, .blend = 0.5f } }
        };

        const u32 ICON_PADDING = 2;
        for(size_t i = 0; i < titles.size(); i++) {
            C2D_Image& img = titles[i]->icon();

            float x = (i % COLUMNS) * (ICON_WIDTH + ICON_PADDING);
            float y = (i / COLUMNS) * (ICON_HEIGHT + ICON_PADDING);

            C2D_ImageTint* imgTint = nullptr;
            if(i == m_selectedTitle) {
                imgTint = &selectedTint;
            }

            C2D_DrawImageAt(img, x, y, 0.0f, imgTint);
            if(i == m_selectedTitle) {
                drawPulsingOutline(x, y, ICON_WIDTH, ICON_HEIGHT, 1, clrBlack);
            }
        }
    }

    void drawPulsingOutline(u32 x, u32 y, u16 w, u16 h, u8 size, u32 color) {
        static float timer = 0.0f;

        u8 r = color & 0xFF;
        u8 g = (color >> 8) & 0xFF;
        u8 b = (color >> 16) & 0xFF;

        float highlight_multiplier = fmax(0.0, fabs(fmod(timer, 1.0) - 0.5) / 0.5);
        color                      = C2D_Color32(r + (255 - r) * highlight_multiplier, g + (255 - g) * highlight_multiplier, b + (255 - b) * highlight_multiplier, 255);
        drawOutline(x, y, w, h, size, color);

        timer += 0.1f;
    }

    void drawOutline(u32 x, u32 y, u16 w, u16 h, u8 size, u32 color) {
        if(x == 0) {
            x++;
            w--;
        }

        if(y == 0) {
            y++;
            h--;
        }

        C2D_DrawRectSolid(x - size, y - size, 0.5f, w + 2 * size, size, color);  // top
        C2D_DrawRectSolid(x - size, y, 0.5f, size, h, color);                    // left
        C2D_DrawRectSolid(x + w, y, 0.5f, size, h, color);                       // right
        C2D_DrawRectSolid(x - size, y + h, 0.5f, w + 2 * size, size, color);     // bottom
    }

private:
    bool m_shouldExit = false;

    u32 m_selectedTitle = 0;
    Client m_client;

    C3D_RenderTarget* m_top    = nullptr;
    C3D_RenderTarget* m_bottom = nullptr;

    const u64 ROWS    = (BOTTOM_SCREEN_HEIGHT / ICON_HEIGHT);
    const u64 COLUMNS = (BOTTOM_SCREEN_WIDTH / ICON_WIDTH);
};

#endif