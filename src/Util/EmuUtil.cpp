#include <Util/EmuUtil.hpp>

// https://github.com/azahar-emu/azahar/blob/43e044ad9ad9dd4f124dc0acf8abd7f69b59778e/src/core/hle/kernel/svc.cpp#L107
#define SYSTEM_INFO_TYPE_CITRA_INFORMATION 0x20000

bool EmulatorUtil::isEmulated() {
    static bool m_isEmulated    = false;
    static bool m_emulatedKnown = false;

    if(!m_emulatedKnown) {
        s64 check = 0;
        // possible later emulators dont use this, doesnt matter as this is just to suppress an emulator log
        m_isEmulated    = !(R_SUCCEEDED(svcGetSystemInfo(&check, SYSTEM_INFO_TYPE_CITRA_INFORMATION, 0)) && check == 0);
        m_emulatedKnown = true;
    }

    return m_isEmulated;
}