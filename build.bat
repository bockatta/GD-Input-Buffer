@echo off
rem Input Buffer - double-click to build and install the mod into Geometry Dash.
cd /d "%~dp0"
title Input Buffer - build

rem First time (or the build folder was deleted): set it up.
if not exist "build\build.ninja" (
    echo Setting up the build folder...
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    if errorlevel 1 goto failed
)

echo Building...
cmake --build build
if errorlevel 1 goto failed

echo.
echo ============================================================
echo  Done - the mod is built and installed.
echo  Restart Geometry Dash to load the new version.
echo ============================================================
pause
exit /b 0

:failed
echo.
echo ============================================================
echo  BUILD FAILED - scroll up for the first "error:" line
echo  and send it over.
echo ============================================================
pause
exit /b 1
