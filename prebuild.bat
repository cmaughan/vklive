@echo off
setlocal

python do.py setup
exit /b %errorlevel%
