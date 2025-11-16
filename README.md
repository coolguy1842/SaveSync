# SaveSync
~~Cloud~~ Locally hosted save/extdata backup, and synchronisation tool for the 3DS.

## Usage
This program supports touchscreen, and buttons, use whatever you like!

You must be running [SaveSyncd](https://github.com/coolguy1842/SaveSyncd) on another device. To connect, set the IP and port in settings.

### Note
Tested with Rosalina and Azahar, https not tested as this is intended for local use only.

This application is fairly new, so it is recommended to backup your saves/extdata before using the download feature, uploading never changes save/extdata files on the 3DS, so this function should be safe.

Expect the program to take longer to start the first time, or after new titles are added.
While uploading/downloading, the program will disable home, sleep, and closing the application.

## Previews
![Preview Grid]
![Preview List]

## Issues
The most recent 5 logs are stored in /3ds/SaveSync/logs. If the program was compiled in debug mode, the console can be toggled with L+X, and the screen swapped with R+X. Debug mode also shows a list of potential memory leaks when closing, shown after pressing the start button.
You can print the profiler with L+Y, or R+Y, L will toggle the console on if it isn't active.

Additionally if you build with -DREDIRECT_CONSOLE it will redirect stdout to 3dslink.

## Building
```sh
git clone https://github.com/coolguy1842/SaveSync --recursive
cd SaveSync
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(nproc)
```

devkitARM is required, Install or update dependencies with: `dkp-pacman -S libctru citro3d citro2d makerom 3ds-bzip2`

For CIA format, drag makerom(.exe) into the base folder, and rebuild, for the banner, do the same steps but with bannertool(.exe).

## TODO
- [ ] Upgrade Server API
- [ ] Load Game Catridges
- [ ] Add Button to Cancel Upload/Download

## Thanks To
- [Checkpoint](https://github.com/BernardoGiordano/Checkpoint) Design inspiration, and code reference.
- [devkitPro](https://devkitpro.org/) Making this possible.
- All involved with [3ds-examples](https://github.com/devkitPro/3ds-examples).
- [Clay](https://github.com/nicbarker/clay) Great UI layout library.
- [3DSky](https://github.com/pvini07BR/3DSky) & [clay3ds](https://github.com/sonodima/clay3ds) & [Clay Renderer SDL3](https://github.com/nicbarker/clay/blob/main/renderers/SDL3/clay_renderer_SDL3.c) References for the Clay renderer.
- [curl](https://github.com/devkitPro/curl) Great HTTP library.
- [sigs](https://github.com/netromdk/sigs) Easy to use signal library.
- [rapidjson](https://github.com/Tencent/rapidjson) Fast JSON Parser & Generator.
- [md5-c](https://github.com/Zunawe/md5-c) Simple and easy to use MD5 hasher.
- [devkitNix](https://github.com/bandithedoge/devkitNix) Nix flake for devkitPro.
- [cmake_timestamp](https://github.com/kraiskil/cmake_timestamp) Simple cmake include for time & git info
- Everyone in the Nintendo Homebrew discord who helped.

<!-- resources -->
[Preview Grid]: images/gridPreview.png
[Preview List]: images/listPreview.png