#!/bin/bash

triplet=(x64-linux)
if [ "$(uname)" == "Darwin" ]; then
   if [ "$(uname -m)" == "arm64" ]; then
       triplet=(arm64-osx)
   else
       triplet=(x64-osx)
   fi
fi

mkdir -p build
cd build
if [ "$1" != "" ] ; then
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$1 -DCMAKE_EXPORT_COMPILE_COMMANDS=YES -DVCPKG_TARGET_TRIPLET=${triplet[0]} ../
else
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=${triplet[0]} ../
fi
cd ../
