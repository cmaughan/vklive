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
common_packages=(lodepng minizip tsl-ordered-map ableton-link cppcodec range-v3 portaudio stb gli reproc fmt nativefiledialog tinyfiledialogs clipp concurrentqueue assimp glm tinydir spirv-reflect)
if [ "$(uname)" == "Darwin" ]; then
    metal_packages=(sdl2 spirv-cross)
    if [ "${VKLIVE_PREBUILD_VULKAN:-0}" == "1" ]; then
        metal_packages=(sdl2[vulkan] spirv-cross vulkan-memory-allocator)
    fi
    ./vcpkg install "${common_packages[@]}" "${metal_packages[@]}" --triplet ${triplet[0]} --recurse
else
    ./vcpkg install "${common_packages[@]}" vulkan-memory-allocator sdl2[vulkan] --triplet ${triplet[0]} --recurse
    ./vcpkg install glib --triplet ${triplet[0]} --recurse
fi
cd ..
