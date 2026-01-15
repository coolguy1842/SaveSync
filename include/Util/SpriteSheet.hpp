#ifndef __SPRITE_SHEET_HPP__
#define __SPRITE_SHEET_HPP__

#include <3ds.h>
#include <citro2d.h>
#include <string>

class SpriteSheet {
public:
    SpriteSheet(const std::string& path);
    ~SpriteSheet();

    C2D_Image* image(size_t idx);
    std::vector<C2D_Image> images() const;

    bool valid() const;

private:
    C2D_SpriteSheet m_sheet;
    std::vector<C2D_Image> m_images;

    bool m_valid;
};

#endif