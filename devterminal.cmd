@echo off
if exist %1 set ANDROID_NDK=%1

echo Hoping your ANDROID_NDK is %ANDROID_NDK%
set PATH=%ANDROID_NDK%\prebuilt\windows-x86_64\bin;%ANDROID_NDK%;%PATH%

cd /d "%~dp0"

echo.

echo You're in the source tree right now, with ANDROID_NDK set
echo and on your path, as well as the prebuilt windows tools (for make, etc)
echo.

echo Run configure to generate a build tree in the build directory.
echo You can edit the configuration with configure-gui
echo You can build by running build, or by changing into
echo the build directory and running make.

cmd /k
