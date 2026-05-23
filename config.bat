@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"

python do.py config %CONFIG%
exit /b %errorlevel%
