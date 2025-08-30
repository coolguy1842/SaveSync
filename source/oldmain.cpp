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
    Application* app = new Application();

    while(app->loop());
    delete app;

    HttpHandler::close();
    Settings::close();

    gfxExit();
    return 0;
}