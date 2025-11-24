#define CLAY_IMPLEMENTATION
#include <clay.h>

#include <Application.hpp>
#include <Debug/LeakDetector.h>
#include <Debug/LeakViewerApplication.hpp>
#include <Util/Logger.hpp>
#include <Util/Profiler.hpp>
#include <fstream>
#include <memory>
#include <timestamp.h>

void HandleClayErrorsA(Clay_ErrorData errorData) { Logger::error("Clay", "{}", errorData.errorText.chars); }
int main() {
    // prevent from showing in leak list
    {
        std::string dirtyStr;
        if(strlen(git_dirty_str) != 0) {
            dirtyStr = std::format("| {} ", git_dirty_str);
        }

        Logger::log("SaveSync v{}.{}.{} | {}({}) {}| Built {}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, git_branch_str, git_short_hash_str, dirtyStr, build_time_str);
    }

    // Clay_SetMaxElementCount(1024);
    while(true) {
        Logger::info("main", "before: leaked: {}\n", leakListCurrentLeaked());
        Application app;
        while(app.loop()) {
            // printf("\x1b[2;1HCPU:     %6.2f%%\x1b[K", C3D_GetProcessingTime() * 6.0f);
            // printf("\x1b[3;1HGPU:     %6.2f%%\x1b[K", C3D_GetDrawingTime() * 6.0f);
            // printf("\x1b[4;1HCmdBuf:  %6.2f%%\x1b[K", C3D_GetCmdBufUsage() * 100.0f);
        }

        Logger::info("main", "after: leaked: {}\n", leakListCurrentLeaked());
        if(aptShouldClose()) {
            return 0;
        }
    }

    // {
    //     LeakViewerApplication app;
    //     while(app.loop()) {}
    // }

    return 0;
}