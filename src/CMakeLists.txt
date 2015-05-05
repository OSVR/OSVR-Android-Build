# See URLs below for more documentation on ExternalProject
# http://www.cmake.org/cmake/help/v3.0/module/ExternalProject.html
# http://www.kitware.com/media/html/BuildingExternalProjectsWithCMake2.8.html
    
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
    
ExternalProject_Add(libfunctionality
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfunctionality"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS ${COMMON_CMAKE_ARGS} # put any others here
    BUILD_IN_SOURCE 0)

ExternalProject_Add(OSVR-Core
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    DEPENDS "jsoncpp"
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    -DOpenCV_DIR=/Users/appleuser/Documents/OsvrAndroid/OpenCV-2.4.10-android-sdk/sdk/native/jni
    -DPYTHON_EXECUTABLE=/usr/bin/python
    -DJAVA_JVM_LIBRARY=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/jre/lib/server/libjvm.dylib
    -DJAVA_AWT_LIBRARY=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/jre/lib/libawt.dylib
    -DJAVA_INCLUDE_PATH=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    -DJAVA_INCLUDE_PATH2=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    -DJAVA_AWT_INCLUDE_PATH=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    BUILD_IN_SOURCE 0)

