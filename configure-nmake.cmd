
@mkdir "%~dp0build"
pushd "%~dp0build"
cmake .. -G "NMake Makefiles" %*
popd
