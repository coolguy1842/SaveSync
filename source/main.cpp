#include <3ds.h>
#include <citro2d.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <application.hpp>
#include <array>
#include <codecvt>
#include <locale>
#include <memory>
#include <optional>
#include <smdh.hpp>
#include <title.hpp>
#include <vector>

int main(int argc, char* argv[]) {
    Application* app = new Application();
    while(app->loop());
    delete app;

    return 0;
}