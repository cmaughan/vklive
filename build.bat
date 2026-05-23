@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"

python do.py build %CONFIG%
exit /b %errorlevel%
