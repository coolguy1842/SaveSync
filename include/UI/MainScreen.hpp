#ifndef __MAIN_SCREEN_HPP__
#define __MAIN_SCREEN_HPP__

#include <Client.hpp>
#include <TitleLoader.hpp>
#include <UI/Screen.hpp>
#include <UI/SettingsScreen.hpp>
#include <clay.h>
#include <memory>
#include <rocket.hpp>

class MainScreen : public Screen, rocket::trackable {
public:
    MainScreen(std::shared_ptr<Config> config, std::shared_ptr<TitleLoader> loader, std::shared_ptr<Client> client);
    ~MainScreen() = default;

    void update();

    void renderTop();
    void renderBottom();

private:
    void updateQueuedText(size_t queueSize, bool processing);
    void updateRequestStatusText(std::string status);
    void onClientRequestFailed(std::string status);

private:
    void TitleIcon(std::shared_ptr<Title> title, float width, float height, Clay_BorderElementConfig border);

    void GridLayout();
    void ListLayout();

private:
    void handleButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData);
    static void onButtonHover(Clay_ElementId elementId, Clay_PointerData pointerData, intptr_t userData);

    void scrollToCurrent();
    void showYesNo(std::string text, QueuedRequest::RequestType action, std::shared_ptr<Title> title);

    // tries to upload/download the selected title
    void tryDownload(Container container);
    void tryUpload(Container container);

    std::shared_ptr<Config> m_config;
    std::shared_ptr<TitleLoader> m_loader;
    std::shared_ptr<Client> m_client;

    std::unique_ptr<SettingsScreen> m_settingsScreen;
    size_t m_selectedTitle = 0;

    u16 m_rows        = 0;
    u16 m_visibleRows = 0;

    u16 m_cols   = 0;
    u16 m_scroll = 0;

    bool m_yesNoActive = false, m_yesSelected = false;
    QueuedRequest m_yesNoAction;

    bool m_okActive = false;

    size_t m_prevLoadedTitles = std::numeric_limits<size_t>::max();

    std::string m_loadedText, m_yesNoText, m_okText;
    Clay_String m_loadedString = CLAY_STRING(""), m_yesNoString = CLAY_STRING(""), m_okString = CLAY_STRING("");

    std::string m_titleText, m_idText, m_mediaTypeText;
    Clay_String m_titleString = CLAY_STRING(""), m_idString = CLAY_STRING(""), m_mediaTypeString = CLAY_STRING("");

    std::string m_networkQueueText, m_networkRequestText;
    Clay_String m_networkQueueString = CLAY_STRING(""), m_networkRequestString = CLAY_STRING("");

    std::string m_titleTexts;

    bool m_serverOnline    = false;
    bool m_updateTitleInfo = true;
};

#endif