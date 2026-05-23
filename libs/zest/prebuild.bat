@echo off

echo %Time%

if not exist "vcpkg\vcpkg.exe" (
cd vcpkg
call bootstrap-vcpkg.bat -disableMetrics
cd %~dp0
)

cd vcpkg
echo Installing Libraries
vcpkg install cppcodec freetype tinydir tinyfiledialogs stb sdl3 fmt cppcodec glm concurrentqueue catch2 nlohmann-json --triplet x64-windows-static-md --recurse
cd %~dp0

echo %Time%

