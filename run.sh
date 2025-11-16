#!/usr/bin/env bash

./build.sh
if [[ $? == 0 ]]; then
    $DEVKITPRO/tools/bin/3dslink -a 192.168.1.24 ./build/SaveSync.3dsx $@
fi