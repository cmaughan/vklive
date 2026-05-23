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

if [ ! -d "vcpkg/imgui" ]; then
  cd vcpkg
  ./vcpkg install cppcodec freetype stb sdl3 fmt foonathan-memory glm concurrentqueue tinydir catch2 nlohmann-json --triplet ${triplet[0]} --recurse
  if [ "$(uname)" != "Darwin" ]; then
  ./vcpkg install glib --triplet ${triplet[0]} --recurse
  fi
  cd ..
fi

