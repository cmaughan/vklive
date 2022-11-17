@echo off

echo %Time%

if not exist "vcpkg\vcpkg.exe" (
    cd vcpkg 
    call bootstrap-vcpkg.bat -disableMetrics
    cd %~dp0
)

cd vcpkg
echo Installing Libraries 
vcpkg install kissfft portaudio range-v3 freetype stb gli reproc fmt nativefiledialog tinyfiledialogs clipp tomlplusplus freetype concurrentqueue assimp glm tinydir vulkan-memory-allocator spirv-reflect sdl2[vulkan] --triplet x64-windows-static-md --recurse
cd %~dp0

echo %Time%

