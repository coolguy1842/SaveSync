#ifndef SMDH_HPP
#define SMDH_HPP

#include <3ds.h>
#include <array>
#include <memory>
#include <string>

#include <Util/TexWrapper.hpp>
#include <citro2d.h>

class SMDH {
public:
    static constexpr u16 ICON_DATA_WIDTH  = 64;
    static constexpr u16 ICON_DATA_HEIGHT = 64;

    static constexpr u16 ICON_WIDTH  = 48;
    static constexpr u16 ICON_HEIGHT = 48;

    static constexpr Tex3DS_SubTexture ICON_SUBTEX = { ICON_WIDTH, ICON_HEIGHT, 0.0f, ICON_WIDTH / static_cast<float>(ICON_DATA_WIDTH), ICON_HEIGHT / static_cast<float>(ICON_DATA_HEIGHT), 0.0f };

    struct Header {
        u32 magic;
        u16 version;
        u16 reserved;
    };

    struct ApplicationTitle {
        u16 shortDescription[0x40];
        u16 longDescription[0x80];
        u16 publisher[0x40];
    };

    struct Settings {
        u8 gameRatings[0x10];
        u32 regionLock;
        u32 matchmaker_id;
        u64 matchmaker_id_bit;
        u32 flags;
        u16 eulaVersion;
        u16 reserved;
        u32 defaultFrame;
        u32 cecId;
    };

    struct Data {
        Header header;
        ApplicationTitle applicationTitles[16];
        Settings settings;
        u64 reserved;
        u8 smallIconData[0x480];
        u16 bigIconData[0x900];
    };

    SMDH(u64 id, FS_MediaType media);
    SMDH(u32 low, u32 high, FS_MediaType media);

    bool valid() const;

    const Header& header() const;
    const ApplicationTitle& applicationTitle(u8 index) const;
    const Settings& settings() const;

    std::shared_ptr<TexWrapper> bigTex();

    static void copyImageData(const u16* src, u16 srcWidth, u16 srcHeight, u16* dst, u16 dstWidth, u16 dstHeight);

private:
    bool m_valid;

    Data m_data;
    std::shared_ptr<TexWrapper> m_bigTex;
};

#endif