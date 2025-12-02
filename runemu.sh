#!/usr/bin/env sh

./tools/build.sh
if [[ $? == 0 ]]; then
    azahar ./build/SaveSync.3dsx
fi