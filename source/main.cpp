#include <3ds.h>
#include <citro2d.h>
#include <malloc.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <application.hpp>
#include <array>
#include <codecvt>
#include <httpHandler.hpp>
#include <locale>
#include <memory>
#include <optional>
#include <settings.hpp>
#include <smdh.hpp>
#include <title.hpp>
#include <vector>

int main(int argc, char* argv[]) {
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);

    amInit();

    Client client;

    Archive::init();
    TitleLoader::instance();

    bool serverOnline = client.isServerOnline();
    printf("server online? %s\n", serverOnline ? "yes" : "no");

    if(serverOnline) {
        std::shared_ptr<Title> title;
        for(auto t : TitleLoader::instance()->titles()) {
            if(t->id() == 0x4000000053F00) {
                title = t;
            }
        }

        if(title != nullptr) {
            TitleInfo info = client.getSaveInfo(title->id());

            printf("title: %llX\n", info.titleID);
            printf("save exists? %s\n", info.save.exists ? "yes" : "no");

            if(info.save.exists) {
                printf("last changed: %llu\nhash: %s\n", info.save.date, info.save.hash.c_str());
            }

            printf("extdata exists? %s\n", info.extdata.exists ? "yes" : "no");
            if(info.extdata.exists) {
                printf("last changed: %llu\nhash: %s\n", info.extdata.date, info.extdata.hash.c_str());
            }

            client.uploadSave(title);
        }
        else {
            printf("couldnt find mario 3d land\n");
        }
    }

    while(aptMainLoop()) {
        gspWaitForVBlank();
        hidScanInput();

        u32 kDown = hidKeysDown();
        if(kDown & KEY_START) {
            break;
        }
    }

    // Application* app = new Application();

    // while(app->loop());
    // delete app;

    HttpHandler::close();
    Settings::close();
    Archive::exit();

    amExit();
    gfxExit();
    return 0;
}