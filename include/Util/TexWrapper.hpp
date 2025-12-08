#ifndef __TEX_WRAPPER_HPP__
#define __TEX_WRAPPER_HPP__

#include <3ds.h>
#include <citro3d.h>
#include <memory>

class TexWrapper {
public:
    static std::shared_ptr<TexWrapper> create(u16 width, u16 height, GPU_TEXCOLOR format);
    ~TexWrapper();

    C3D_Tex* handle();

private:
    TexWrapper(u16 width, u16 height, GPU_TEXCOLOR format);
    C3D_Tex m_tex;
};

#endif