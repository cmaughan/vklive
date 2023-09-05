#!/bin/bash -x
# Remember to preinstall linux libraries:
# (ibus,  tar, zip, unzip, buid-prerequisits, xorg-dev)
if [ ! -f "vcpkg/vcpkg" ]; then
    cd vcpkg
    ./bootstrap-vcpkg.sh -disableMetrics
    cd ..
fi

triplet=(x64-linux)
if [ "$(uname)" == "Darwin" ]; then
   triplet=(x64-osx)
fi

cd vcpkg
./vcpkg install tsl-ordered-map ableton-link cppcodec range-v3 portaudio stb gli reproc fmt nativefiledialog tinyfiledialogs clipp tomlplusplus concurrentqueue assimp glm tinydir vulkan-memory-allocator spirv-reflect sdl2[vulkan] --triplet ${triplet[0]} --recurse
if [ "$(uname)" != "Darwin" ]; then
./vcpkg install glib --triplet ${triplet[0]} --recurse
fi
cd ..


