@echo off
setlocal

set "ROOT_DIR=%~dp0."
set "BUILD_DIR=%ROOT_DIR%\build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Build directory is not configured. Run config.bat first.
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config "%CONFIG%"
exit /b %errorlevel%
