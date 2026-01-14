#define CLAY_IMPLEMENTATION
#include <clay.h>

#include <Application.hpp>
#include <Cache.hpp>
#include <Debug/LeakDetector.h>
#include <Debug/LeakViewerApplication.hpp>
#include <Debug/Logger.hpp>
#include <ext_ptmplays.h>
#include <format>
#include <string>
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