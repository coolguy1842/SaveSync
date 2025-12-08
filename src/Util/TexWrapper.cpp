#include <Util/TexWrapper.hpp>

std::shared_ptr<TexWrapper> TexWrapper::create(u16 width, u16 height, GPU_TEXCOLOR format) {
    struct make_shared_enabler : public TexWrapper {
        make_shared_enabler(u16 width, u16 height, GPU_TEXCOLOR format)
            : TexWrapper(width, height, format) {}
    };

    return std::make_shared<make_shared_enabler>(width, height, format);
}

TexWrapper::TexWrapper(u16 width, u16 height, GPU_TEXCOLOR format) {
    C3D_TexInit(&m_tex, width, height, format);
}

TexWrapper::~TexWrapper() {
    C3D_TexDelete(&m_tex);
}

C3D_Tex* TexWrapper::handle() { return &m_tex; }