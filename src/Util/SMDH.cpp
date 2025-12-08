#include <Debug/Profiler.hpp>
#include <FS/Archive.hpp>
#include <FS/File.hpp>
#include <Util/SMDH.hpp>

SMDH::SMDH(u64 id, FS_MediaType media)
    : SMDH(static_cast<u32>(id), static_cast<u32>(id >> 32), media) {}

SMDH::SMDH(u32 lowID, u32 highID, FS_MediaType media)
    : m_valid(false) {
    PROFILE_SCOPE("Load SMDH");

    u32 archPath[]              = { lowID, highID, media, 0x0 };
    static const u32 filePath[] = { 0x0, 0x0, 0x2, 0x6E6F6369, 0x0 };

    FS_Path binArchPath = { PATH_BINARY, 0x10, archPath };
    FS_Path binFilePath = { PATH_BINARY, 0x14, filePath };

    std::shared_ptr<File> file = File::openDirect(ARCHIVE_SAVEDATA_AND_CONTENT, binArchPath, binFilePath, FS_OPEN_READ, 0);
    if(file == nullptr || !file->valid()) {
        return;
    }

    if(file->read(&m_data, sizeof(m_data), 0) == U64_MAX) {
        return;
    }

    // from checkpoint: https://github.com/BernardoGiordano/Checkpoint/blob/3f94b5c8816a323348c8f84eeaf5a53d2c3bebe6/3ds/source/title.cpp#L35
    m_bigTex = TexWrapper::create(ICON_DATA_WIDTH, ICON_DATA_HEIGHT, GPU_RGB565);
    copyImageData(m_data.bigIconData, ICON_WIDTH, ICON_HEIGHT, reinterpret_cast<u16*>(m_bigTex->handle()->data), ICON_DATA_WIDTH, ICON_DATA_HEIGHT);

    m_valid = true;
}

void SMDH::copyImageData(const u16* src, u16 srcWidth, u16 srcHeight, u16* dst, u16 dstWidth, u16 dstHeight) {
    if(srcWidth != dstWidth || srcHeight != dstHeight) {
        if(srcWidth > dstWidth || srcHeight > dstHeight) {
            src += (srcWidth - dstWidth) * srcHeight;
        }
        else {
            dst += (dstWidth - srcWidth) * dstHeight;
        }
    }

    for(u16 j = 0; j < std::min(srcHeight, dstHeight); j += 8) {
        memcpy(dst, src, std::min(srcWidth, dstWidth) * 8 * sizeof(u16));

        src += srcWidth * 8;
        dst += dstWidth * 8;
    }
}

bool SMDH::valid() const { return m_valid; }

const SMDH::Header& SMDH::header() const { return m_data.header; }
const SMDH::Settings& SMDH::settings() const { return m_data.settings; }
const SMDH::ApplicationTitle& SMDH::applicationTitle(u8 index) const {
    if(index >= 16) {
        return m_data.applicationTitles[0];
    }

    return m_data.applicationTitles[index];
}

std::shared_ptr<TexWrapper> SMDH::bigTex() { return m_bigTex; }