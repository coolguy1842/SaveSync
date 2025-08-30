#ifndef __TITLE_HPP__
#define __TITLE_HPP__

#include <3ds.h>
#include <citro2d.h>

#include <archive.hpp>
#include <codecvt>
#include <locale>
#include <optional>
#include <smdh.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
template <typename T>
std::string toUTF8(const std::basic_string<T, std::char_traits<T>, std::allocator<T>>& source) {
    std::string result;

    std::wstring_convert<std::codecvt_utf8_utf16<T>, T> convertor;
    result = convertor.to_bytes(source);

    return result;
}
#pragma GCC diagnostic pop

class Title {
public:
    Title(u64 id, FS_MediaType mediaType)
        : m_titleID(id)
        , m_mediaType(mediaType)
        , m_smdh(loadSMDH(lowID(), highID(), m_mediaType)) {
        if(m_smdh == nullptr) {
            return;
        }

        bool loadImage = false;

        if(Archive::accessible(m_mediaType, lowID(), highID())) {
            m_accessibleSave = true;
            loadImage        = true;
        }

        if(Archive::accessible(extdataID())) {
            m_accessibleExtData = true;
            loadImage           = true;
        }

        if(loadImage) {
            m_icon = loadTextureFromBytes(m_smdh->bigIconData);
        }
    }

    ~Title() {
        if(m_smdh != nullptr) {
            delete m_smdh;
        }
    }

    std::string shortDescription() const {
        return toUTF8(removeForbiddenCharacters((char16_t*)m_smdh->applicationTitles[1].shortDescription));
    }

    std::string longDescription() const {
        return toUTF8(std::u16string((char16_t*)m_smdh->applicationTitles[1].longDescription));
    }

    C2D_Image& icon() { return m_icon; }

    u64 id() const { return m_titleID; }
    u32 lowID() const { return (u32)m_titleID; }
    u32 highID() const { return (u32)(m_titleID >> 32); }

    FS_MediaType mediaType() const { return m_mediaType; }

    bool accessibleSave() const { return m_accessibleSave; }
    bool accessibleExtData() const { return m_accessibleExtData; }

    u32 extdataID() const {
        u32 low = lowID();
        switch(low) {
        case 0x00055E00: return 0x055D;  // Pokémon Y
        case 0x0011C400: return 0x11C5;  // Pokémon Omega Ruby
        case 0x00175E00: return 0x1648;  // Pokémon Moon
        case 0x00179600:
        case 0x00179800: return 0x1794;  // Fire Emblem Conquest SE NA
        case 0x00179700:
        case 0x0017A800: return 0x1795;  // Fire Emblem Conquest SE EU
        case 0x0012DD00:
        case 0x0012DE00: return 0x12DC;  // Fire Emblem If JP
        case 0x001B5100: return 0x1B50;  // Pokémon Ultramoon
                                         // TODO: need confirmation for this
                                         // case 0x001C5100:
                                         // case 0x001C5300:
                                         //     return 0x0BD3; // Etrian Odyssey V: Beyond the Myth
        }

        return low >> 8;
    }

private:
    static std::u16string removeForbiddenCharacters(std::u16string src) {
        static const std::u16string illegalChars = u".,!\\/:?*\"<>|";
        for(size_t i = 0; i < src.length(); i++) {
            if(illegalChars.find(src[i]) != std::string::npos) {
                src[i] = ' ';
            }
        }

        size_t i;
        for(i = src.length() - 1; i > 0 && src[i] == L' '; i--);
        src.erase(i + 1, src.length() - i);

        return src;
    }

    static C2D_Image loadTextureFromBytes(u16* bigIconData) {
        C3D_Tex* tex                          = (C3D_Tex*)malloc(sizeof(C3D_Tex));
        static const Tex3DS_SubTexture subt3x = { 48, 48, 0.0f, 48 / 64.0f, 48 / 64.0f, 0.0f };
        C2D_Image image                       = (C2D_Image){ tex, &subt3x };
        C3D_TexInit(image.tex, 64, 64, GPU_RGB565);

        u16* dest = (u16*)image.tex->data + (64 - 48) * 64;
        u16* src  = bigIconData;
        for(int j = 0; j < 48; j += 8) {
            memcpy(dest, src, 48 * 8 * sizeof(u16));
            src += 48 * 8;
            dest += 64 * 8;
        }

        return image;
    }

private:
    u64 m_titleID;
    FS_MediaType m_mediaType;

    smdh_s* m_smdh;
    C2D_Image m_icon;

    bool m_accessibleSave;
    bool m_accessibleExtData;
};

#endif