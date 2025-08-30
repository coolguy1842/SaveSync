#!/usr/bin/env bash

./build.sh
if [[ $? == 0 ]]; then
    azahar ./build/SaveSync.3dsx
fi