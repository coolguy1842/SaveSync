#define CLAY_IMPLEMENTATION
#include <clay.h>

#include <Application.hpp>
#include <Util/LeakDetector.h>
#include <Util/Logger.hpp>
#include <Util/Profiler.hpp>
#include <memory>
#include <timestamp.h>

int main() {
    Logger::log("SaveSync v{}.{}.{} | {}({}) | {} | Built {}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, git_branch_str, git_short_hash_str, git_dirty_str, build_time_str);

    {
        std::unique_ptr<Application> application = std::make_unique<Application>();
        while(application->loop()) {
            // printf("\x1b[2;1HCPU:     %6.2f%%\x1b[K", C3D_GetProcessingTime() * 6.0f);
            // printf("\x1b[3;1HGPU:     %6.2f%%\x1b[K", C3D_GetDrawingTime() * 6.0f);
            // printf("\x1b[4;1HCmdBuf:  %6.2f%%\x1b[K", C3D_GetCmdBufUsage() * 100.0f);
        }
    }

    if(aptShouldClose() || !isDetectingLeaks()) {
        clearLeaks();
        return 0;
    }

    // prevent profiler from showing up on leak list
    Profiler::reset();
    leak_list_node* begin = cloneCurrentList();

    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_BOTTOM, false);

    gfxSwapBuffers();
    gspWaitForVBlank();

    u16 width, height;
    u8* framebuffer = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &width, &height);
    // clear bottom screen
    memset(framebuffer, 0, width * height * gspGetBytesPerPixel(gfxGetScreenFormat(GFX_BOTTOM)));

    PrintConsole console;
    consoleInit(GFX_TOP, &console);

    Logger::logLeaks(begin);
    freeClonedList(begin);

    while(aptMainLoop()) {
        gfxSwapBuffers();
        gspWaitForVBlank();

        hidScanInput();
        u32 kDown = hidKeysDown();

        if(kDown & KEY_START) {
            break;
        }
    }

    gfxExit();

    return 0;
}