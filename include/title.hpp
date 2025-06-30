#ifndef __TITLE_HPP__
#define __TITLE_HPP__

#include <3ds.h>

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

std::u16string removeForbiddenCharacters(std::u16string src) {
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

C2D_Image loadTextureFromBytes(u16* bigIconData) {
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

class Title {
public:
    Title(u64 id, FS_MediaType mediaType)
        : m_titleID(id)
        , m_mediaType(mediaType) {
        m_smdh = loadSMDH((u32)m_titleID, (u32)(m_titleID >> 32), m_mediaType);
    }

    ~Title() {
        delete m_smdh;
    }

    u64 id() const {
        return m_titleID;
    }

    std::string shortDescription() const {
        return toUTF8(removeForbiddenCharacters((char16_t*)m_smdh->applicationTitles[1].shortDescription));
    }

    std::string longDescription() const {
        return toUTF8(std::u16string((char16_t*)m_smdh->applicationTitles[1].longDescription));
    }

    C2D_Image icon() {
        if(!m_icon.has_value()) {
            m_icon = loadTextureFromBytes(m_smdh->bigIconData);
        }

        return m_icon.value();
    }

private:
    u64 m_titleID;
    FS_MediaType m_mediaType;

    smdh_s* m_smdh;
    std::optional<C2D_Image> m_icon;
};

#endif