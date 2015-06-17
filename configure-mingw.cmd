
@mkdir "%~dp0build"
pushd "%~dp0build"
cmake .. -G "MinGW Makefiles" "-DCMAKE_MAKE_PROGRAM=%ANDROID_NDK%\prebuilt\windows-x86_64\bin\make.exe" %*
popd
