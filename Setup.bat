@echo off

set RootDirectory=%~dp0
set RootDirectory=%RootDirectory:~0,-1%

"%RootDirectory%\vendor\Premake\Windows\premake5.exe" --file=premake.lua vs2022
if %ERRORLEVEL% neq 0 (
    echo Premake failed with error %ERRORLEVEL%.
    pause
)
