
@mkdir "%~dp0build"
pushd "%~dp0build"
@rem "-DCMAKE_MAKE_PROGRAM=%ANDROID_NDK%prebuilt\windows-x86_64\bin\make.exe"
cmake .. -G "MinGW Makefiles" "-DCMAKE_MAKE_PROGRAM=%ANDROID_NDK%\prebuilt\windows-x86_64\bin\make.exe" -DCMAKE_BUILD_TYPE=Release %*
popd
