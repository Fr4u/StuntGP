@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0StuntGP_NEW.exe" exit /b 1
if not exist "%~dp0data\retail\wads" exit /b 2
if not exist "%~dp0data\retail\Game.cfg" exit /b 3
start "" "%~dp0StuntGP_NEW.exe" --root "%~dp0." --source-menu
