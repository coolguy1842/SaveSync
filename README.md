![Banner]
<br>
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
- [ ] Second Confirm for Downloading, with Don't Show Again
- [ ] Use Compression with HTTP
- [ ] Better Caching System
- [ ] Better Caching System
- [ ] Handle Downloading for Games that use Files with Invalid Data

## Thanks To
- [Checkpoint](https://github.com/BernardoGiordano/Checkpoint) Design inspiration, and code reference.
- [devkitPro](https://devkitpro.org/) Making this possible.
- All involved with [3ds-examples](https://github.com/devkitPro/3ds-examples).
- [Clay](https://github.com/nicbarker/clay) Great UI layout library.
- [3DSky](https://github.com/pvini07BR/3DSky) & [clay3ds](https://github.com/sonodima/clay3ds) & [Clay Renderer SDL3](https://github.com/nicbarker/clay/blob/main/renderers/SDL3/clay_renderer_SDL3.c) References for the Clay renderer.
- [curl](https://github.com/devkitPro/curl) Great HTTP library.
- [rocket](https://github.com/tripleslash/rocket) Easy to use & safe signal library.
- [rapidjson](https://github.com/Tencent/rapidjson) Fast JSON Parser & Generator.
- [md5-c](https://github.com/Zunawe/md5-c) Simple and easy to use MD5 hasher.
- [devkitNix](https://github.com/bandithedoge/devkitNix) Nix flake for devkitPro.
- [cmake_timestamp](https://github.com/kraiskil/cmake_timestamp) Simple cmake include for time & git info
- [indigodigi](https://indigodigi.com/) Redesigned Icon & Banner
- [Material Symbols](fonts.google.com/icons)
- Everyone in the Nintendo Homebrew discord who helped.

<!-- resources -->
[Banner]: assets/banner.png
[Preview Grid]: images/gridPreview.png
[Preview List]: images/listPreview.png