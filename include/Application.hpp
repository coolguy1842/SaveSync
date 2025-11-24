#ifndef __APPLICATION_HPP__
#define __APPLICATION_HPP__

#include <Client.hpp>
#include <Config.hpp>
#include <TitleLoader.hpp>
#include <clay_renderer_C2D.hpp>
#include <memory>

class Application {
public:
    Application();
    ~Application();

    void update();
    void render();

    bool loop();

    bool shouldExit() const;
    void setShouldExit(bool shouldExit = true);

private:
    void initClay();

    bool m_shouldExit;

    Config m_config;
    TitleLoader m_loader;
    std::shared_ptr<Client> m_client;

    Clay_Context* m_topContext    = nullptr;
    Clay_Context* m_bottomContext = nullptr;

    void* m_clayTopMemory    = nullptr;
    void* m_clayBottomMemory = nullptr;
    Clay_C2DRendererData m_rendererData;

    C3D_RenderTarget* m_top    = nullptr;
    C3D_RenderTarget* m_bottom = nullptr;

    touchPosition m_prevTouch;
    u64 m_prevTime;
};

#endif