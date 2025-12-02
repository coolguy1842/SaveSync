#ifndef __LEAK_VIEWER_APPLICATION_HPP__
#define __LEAK_VIEWER_APPLICATION_HPP__

#include <3ds.h>
#include <Debug/LeakDetector.h>
#include <clay_renderer_C2D.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

class LeakViewerApplication {
public:
    LeakViewerApplication();
    ~LeakViewerApplication();

    void update();
    void render();

    bool loop();

    bool shouldExit() const;
    void setShouldExit(bool shouldExit = true);

private:
    void renderTop();
    void renderBottom();

    void initClay();
    void initLeakList();

    void updateScroll();

    bool m_shouldExit;

    Clay_Context* m_topContext    = nullptr;
    Clay_Context* m_bottomContext = nullptr;

    void* m_clayTopMemory    = nullptr;
    void* m_clayBottomMemory = nullptr;
    Clay_C2DRendererData m_rendererData;

    C3D_RenderTarget* m_top    = nullptr;
    C3D_RenderTarget* m_bottom = nullptr;

    leak_list_node* m_leakBegin = nullptr;

    u16 m_rows        = 0;
    u16 m_visibleRows = 0;
    u16 m_scroll      = 0;

    size_t m_selectedLeak;

    touchPosition m_prevTouch;
    u64 m_prevTime;

    struct TraceInfo {
        const char* symbolString;

        std::string labelString;
        std::string addressString;
        std::string offsetString;

        std::string addressWithOffsetString;
    };

    struct LeakInfo {
        leak_list_node* node;

        // strings here to keep the data loaded for clay
        std::string ptrString;
        std::string sizeString;
        std::string numTracesString;

        const char* allocatorString;

        int numTraces;
        TraceInfo traces[LEAK_LIST_NODE_MAX_TRACES];
    };

    std::vector<std::shared_ptr<LeakInfo>> m_leakInfo;
    std::string m_totalLeakedString;
};

#endif