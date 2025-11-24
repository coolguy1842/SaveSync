#ifndef __LEAK_VIEWER_APPLICATION_HPP__
#define __LEAK_VIEWER_APPLICATION_HPP__

#include <3ds.h>
#include <Debug/LeakDetector.h>
#include <clay_renderer_C2D.hpp>

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

    bool m_shouldExit;

    Clay_Context* m_topContext    = nullptr;
    Clay_Context* m_bottomContext = nullptr;

    void* m_clayTopMemory    = nullptr;
    void* m_clayBottomMemory = nullptr;
    Clay_C2DRendererData m_rendererData;

    C3D_RenderTarget* m_top    = nullptr;
    C3D_RenderTarget* m_bottom = nullptr;

    leak_list_node* m_leakBegin = nullptr;
};

#endif