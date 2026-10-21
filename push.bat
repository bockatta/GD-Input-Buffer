@echo off
rem Input Buffer - double-click to upload your changes to GitHub.
rem GitHub then builds Windows + Android + iOS (Actions tab -> Build Output).
cd /d "%~dp0"
title Input Buffer - push to GitHub

set "msg="
set /p "msg=What changed? (Enter = "update"): "
if "%msg%"=="" set "msg=update"

git add .
git commit -m "%msg%"
git push
if errorlevel 1 (
    echo.
    echo PUSH FAILED - see the message above.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  Uploaded. The build is running on GitHub:
echo  https://github.com/bockatta/GD-Input-Buffer/actions
echo ============================================================
pause
