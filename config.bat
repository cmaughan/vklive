@echo off
setlocal

set "ROOT_DIR=%~dp0."
set "BUILD_DIR=%ROOT_DIR%\build"

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    if errorlevel 1 exit /b %errorlevel%
)

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022"
exit /b %errorlevel%

