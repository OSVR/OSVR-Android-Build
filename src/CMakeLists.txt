# See URLs below for more documentation on ExternalProject
# http://www.cmake.org/cmake/help/v3.0/module/ExternalProject.html
# http://www.kitware.com/media/html/BuildingExternalProjectsWithCMake2.8.html

ExternalProject_Add(libfunctionality
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfunctionality"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS ${COMMON_CMAKE_ARGS} # put any others here
    BUILD_IN_SOURCE 0)

ExternalProject_Add(jsoncpp
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    -DJSONCPP_WITH_CMAKE_PACKAGE=ON
    -DJSONCPP_LIB_BUILD_SHARED=ON
    -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} # Have to pass in "host" python
    -DJSONCPP_WITH_TESTS=OFF
    -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF
    # put any others here
    BUILD_IN_SOURCE 0)

ExternalProject_Add(OSVR-Core
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    -DCMAKE_TOOLCHAIN_FILE=/Users/appleuser/Documents/OsvrAndroid/android-cmake-master/android.toolchain.cmake
    -DANDROID_STL=gnustl_shared
    -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-4.9
    -DANDROID_NATIVE_API_LEVEL=android-19
    -DBoost_INCLUDE_DIR=/Users/appleuser/Documents/OsvrAndroid/boost/Boost-for-Android-master/boost_1_53_0/
    -DBOOST_ROOT=/Users/appleuser/Documents/OsvrAndroid/boost/Boost-for-Android-master/boost_1_53_0/
    -DBOOST_LIBRARYDIR=/Users/appleuser/Documents/OsvrAndroid/boost/Boost-for-Android-master/build/lib/
    -DCMAKE_PREFIX_PATH="/Users/appleuser/Documents/OsvrAndroid/boost/Boost-for-Android-master/boost_1_53_0/;/Users/appleuser/Documents/OsvrAndroid/vrpn/submodules/jsoncpp/build;/Users/appleuser/Documents/OsvrAndroid/OSVR-Core/vendor/vrpn/build/"
    -DLIBUSB1_INCLUDE_DIR=/Users/appleuser/Documents/OsvrAndroid/libusb-master/libusb/
    -DLIBUSB1_LIBRARY=/Users/appleuser/Documents/OsvrAndroid/libusb-master/android/obj/local/armeabi/libusb1.0.so
    -DHIDAPI_INCLUDE_DIR=/Users/appleuser/Documents/OsvrAndroid/hidapi/hidapi/
    -DHIDAPI_LIBRARY=/Users/appleuser/Downloads/bins/libhidapi.so
    -DOpenCV_DIR=/Users/appleuser/Documents/OsvrAndroid/OpenCV-2.4.10-android-sdk/sdk/native/jni
    -DPYTHON_EXECUTABLE=/usr/bin/python
    -DJSONCPP_LIBRARY=/Users/appleuser/Documents/OsvrAndroid/vrpn/submodules/jsoncpp/build/lib/
    -DJSONCPP_INCLUDE_DIR=/Users/appleuser/Documents/OsvrAndroid/vrpn/submodules/jsoncpp/include/
    -DJAVA_JVM_LIBRARY=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/jre/lib/server/libjvm.dylib
    -DJAVA_AWT_LIBRARY=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/jre/lib/libawt.dylib
    -DJAVA_INCLUDE_PATH=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    -DJAVA_INCLUDE_PATH2=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    -DJAVA_AWT_INCLUDE_PATH=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    BUILD_IN_SOURCE 0)
