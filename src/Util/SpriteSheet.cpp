#include <Debug/Logger.hpp>
#include <Util/ScopedService.hpp>
#include <Util/SpriteSheet.hpp>

SpriteSheet::SpriteSheet(const std::string& path)
    : m_valid(false) {
    Services::RomFS romfs;

    m_sheet = C2D_SpriteSheetLoad(path.c_str());
    if(m_sheet == nullptr) {
        Logger::error("SpriteSheet Init", "C2D_SpriteSheetLoad returned null!");
        return;
    }

    size_t count = C2D_SpriteSheetCount(m_sheet);
    m_images     = std::vector<C2D_Image>(count);

    for(size_t i = 0; i < m_images.size(); i++) {
        m_images[i] = C2D_SpriteSheetGetImage(m_sheet, i);
    }

    m_valid = true;
}

SpriteSheet::~SpriteSheet() {
    C2D_SpriteSheetFree(m_sheet);
}

C2D_Image* SpriteSheet::image(size_t idx) {
    if(idx >= m_images.size()) {
        return nullptr;
    }

    return &m_images[idx];
}

bool SpriteSheet::valid() const { return m_valid; }
std::vector<C2D_Image> SpriteSheet::images() const { return m_images; }