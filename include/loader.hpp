#ifndef __LOADER_HPP__
#define __LOADER_HPP__

#include <algorithm>
#include <complex>
#include <memory>
#include <title.hpp>
#include <vector>

class TitleLoader {
public:
    ~TitleLoader() {
        m_titles.clear();
    }

    static TitleLoader* instance();
    static void closeInstance();

    const std::vector<std::shared_ptr<Title>>& titles() { return m_titles; }
    void reloadTitles() {
        m_titles.clear();

        loadSDTitles();
        loadCartTitle();
    }

private:
    bool titleIDValid(u64 id) {
        switch((u32)id) {
        // Instruction Manual
        case 0x00008602:
        case 0x00009202:
        case 0x00009B02:
        case 0x0000A402:
        case 0x0000AC02:
        case 0x0000B402:
        // Internet Browser
        case 0x00008802:
        case 0x00009402:
        case 0x00009D02:
        case 0x0000A602:
        case 0x0000AE02:
        case 0x0000B602:
        case 0x20008802:
        case 0x20009402:
        case 0x20009D02:
        case 0x2000AE02:
        // Garbage
        case 0x00021A00:
            return false;
        }

        // check for updates
        u32 high = id >> 32;
        if(high == 0x0004000E) {
            return false;
        }

        return true;
    }

    void loadSDTitles() {
        u32 count = 0;
        AM_GetTitleCount(MEDIATYPE_SD, &count);

        std::vector<u64> ids(count);
        AM_GetTitleList(NULL, MEDIATYPE_SD, count, ids.data());

        for(auto& id : ids) {
            if(!titleIDValid(id)) {
                continue;
            }

            std::shared_ptr<Title> title = std::make_shared<Title>(id, MEDIATYPE_SD);
            if(title->accessibleSave() || title->accessibleExtData()) {
                m_titles.push_back(title);
            }
        }
    }

    void loadCartTitle() {
    }

    TitleLoader() {
        reloadTitles();
    }

    std::vector<std::shared_ptr<Title>> m_titles;
};

#endif