#!/bin/bash

set -e -x

if ! [[ -f CMakeCache.txt ]]; then
    cmake -G "Visual Studio 14 2015 Win64" . # 如果没有 Win64 编译出来的就是 32 位的 DLL
fi

# /nr:false is to prevent msbuild not exiting
# See https://stackoverflow.com/a/3919906/1467959
export MSBUILDDISABLENODEREUSE=1
msbuild -nr:false seadrive_thumbnail_ext.vcxproj
