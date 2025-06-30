#ifndef __APPLICATION_HPP__
#define __APPLICATION_HPP__

#define ENABLE_CONSOLE

#include <3ds.h>

#include <string>

class Application {
private:
    u32 clrClear;
    u32 clrWhite;
    u32 clrBlack;

public:
    C2D_Text test;
    C2D_TextBuf testBuf;

    Application()
        : clrClear(C2D_Color32(0xFF, 0xD8, 0xB0, 0x68))
        , clrWhite(C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF))
        , clrBlack(C2D_Color32(0x00, 0x00, 0x00, 0xFF)) {
        gfxInitDefault();

        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        C2D_Prepare();

        m_top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        m_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

        testBuf = C2D_TextBufNew(5);
        C2D_TextParse(&test, testBuf, "test");

#ifdef ENABLE_CONSOLE
        m_consoleTextBuf = C2D_TextBufNew(m_consoleTextBufSize);
        C2D_TextParse(&m_consoleText, m_consoleTextBuf, "Hello world!");
#endif
    }

    virtual ~Application() {
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
    virtual void update() {
        hidScanInput();

        u32 kDown = hidKeysDown();
        if(kDown & KEY_START) {
            setShouldExit(true);
        }

        if(kDown & KEY_Y) {
            m_showConsole = !m_showConsole;
        }
    }

    virtual void render() {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

#ifdef ENABLE_CONSOLE
        if(m_showConsole)
            renderConsole();
        else
#endif
            renderTop();
        renderBottom();

        C3D_FrameEnd(0);
    }

#ifdef ENABLE_CONSOLE
    virtual void renderConsole() {
        C2D_TargetClear(m_top, clrBlack);
        C2D_SceneBegin(m_top);

        C2D_DrawText(&m_consoleText, C2D_AlignLeft | C2D_WithColor | C2D_WordWrap, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, clrWhite, (float)GSP_SCREEN_WIDTH);
    }
#endif

    // does nothing if ENABLE_CONSOLE is not defined
    void consoleLog(const char* format, ...) {
#ifdef ENABLE_CONSOLE

#endif
    }

    virtual void renderTop() {
        C2D_TargetClear(m_top, clrClear);
        C2D_SceneBegin(m_top);

        C2D_DrawText(&m_consoleText, C2D_AlignLeft, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    virtual void renderBottom() {
        C2D_TargetClear(m_bottom, clrClear);
        C2D_SceneBegin(m_bottom);

        C2D_DrawText(&test, C2D_AlignLeft, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

private:
    bool m_showConsole = true;
    bool m_shouldExit  = false;

    C3D_RenderTarget* m_top    = nullptr;
    C3D_RenderTarget* m_bottom = nullptr;

#ifdef ENABLE_CONSOLE
    C2D_Text m_consoleText;
    C2D_TextBuf m_consoleTextBuf;

    u32 m_consoleTextBufSize          = 64;
    const u32 m_consoleTextBufMaxSize = 512;
#endif
};

#endif