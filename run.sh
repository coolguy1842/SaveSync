#!/usr/bin/env sh

./tools/build.sh
if [[ $? == 0 ]]; then
    $DEVKITPRO/tools/bin/3dslink ./build/SaveSync.3dsx $@
fi