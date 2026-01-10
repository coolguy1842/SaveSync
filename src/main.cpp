#define CLAY_IMPLEMENTATION
#include <clay.h>

#include <Application.hpp>
#include <Cache.hpp>
#include <Client.hpp>
#include <Debug/ExceptionHandler.hpp>
#include <Debug/LeakDetector.h>
#include <Debug/LeakViewerApplication.hpp>
#include <Debug/Logger.hpp>
#include <Debug/SymbolUtils.h>
#include <ext_ptmplays.h>
#include <malloc.h>
#include <memory>
#include <timestamp.h>

int main() {
    // prevent from showing in leak list
    {
        std::string dirtyStr = strlen(git_dirty_str) != 0 ? std::format("| {} ", git_dirty_str) : "";
        Logger::log("{} v{}.{}.{} | {}({}) {}| Built {}", EXE_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, git_branch_str, git_short_hash_str, dirtyStr, build_time_str);

        Application app;
        while(app.loop()) {
            // printf("\x1b[2;1HCPU:     %6.2f%%\x1b[K", C3D_GetProcessingTime() * 6.0f);
            // printf("\x1b[3;1HGPU:     %6.2f%%\x1b[K", C3D_GetDrawingTime() * 6.0f);
            // printf("\x1b[4;1HCmdBuf:  %6.2f%%\x1b[K", C3D_GetCmdBufUsage() * 100.0f);
        }

        // Services::AM p_AM;

        // gfxInitDefault();
        // consoleInit(GFX_TOP, NULL);

        // printf("t\n");
        // std::shared_ptr<Title> title = std::make_shared<Title>(0x000400000016A600, MEDIATYPE_SD, CARD_CTR);
        // title->hashContainer(EXTDATA);

        // while(true) {
        //     gspWaitForVBlank();
        //     gfxFlushBuffers();
        //     gfxSwapBuffers();

        //     hidScanInput();
        //     u32 kDown = hidKeysDown();
        //     if(kDown & KEY_START) {
        //         break;
        //     }
        // }

        // title.reset();
        // gfxExit();
    }

    if(!aptShouldClose() && isDetectingLeaks()) {
        LeakViewerApplication app;
        while(app.loop()) {}
    }
    else {
        Logger::closeLogFile();
    }

    return 0;
}