# See URLs below for more documentation on ExternalProject
# http://www.cmake.org/cmake/help/v3.0/module/ExternalProject.html
# http://www.kitware.com/media/html/BuildingExternalProjectsWithCMake2.8.html


###
# Host (native, not Android) projects from source
###
set(HOST_PREFIX_PATH ${CMAKE_PREFIX_PATH})

ExternalProject_Add(jsoncpp_host
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp"
    CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${HOST_INSTALL_DIR}"
    -DJSONCPP_WITH_CMAKE_PACKAGE=ON
    -DJSONCPP_LIB_BUILD_SHARED=OFF
    -DJSONCPP_WITH_TESTS=OFF
    -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF
    BUILD_IN_SOURCE 0)

list(APPEND HOST_PREFIX_PATH "${HOST_INSTALL_DIR}")

ExternalProject_Add(osvr_json_to_c_host
    DEPENDS jsoncpp_host
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/osvr_json_to_c"
    CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${HOST_INSTALL_DIR}"
    "-DCMAKE_PREFIX_PATH=${HOST_PREFIX_PATH}"
    "-DOSVR_CORE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core")

set(OSVR_JSON_TO_C_COMMAND "${HOST_INSTALL_DIR}/bin/osvr_json_to_c")

###
# Android Binary SDKs
###

# OpenCV has a binary SDK for Android - just download and unpack.
set(OPENCV_VERSION 2.4.11)
ExternalProject_Add(OpenCV
    URL http://downloads.sourceforge.net/project/opencvlibrary/opencv-android/${OPENCV_VERSION}/OpenCV-${OPENCV_VERSION}-android-sdk.zip)

# Find out where OpenCV was extracted so we can extend the prefix path.
ExternalProject_Get_Property(OpenCV SOURCE_DIR)
osvr_android_add_to_common_prefix_path("${SOURCE_DIR}/sdk/native/jni")

###
# Conditionally build Boost
###
if(NOT ANDROID_CRYSTAX_NDK)
    # In this case, we need boost.
    if(WIN32)
        # and msys bash
        find_program(SH_COMMAND NAMES bash.exe dash.exe sh.exe)
        if(NOT SH_COMMAND)
            message(FATAL_ERROR "Missing msys sh - needed to build Boost. Either set SH_COMMAND or use the CrystaX NDK (which includes boost)")
        endif()
        set(SH_COMMAND_ARGS --login -i)
    else()
        set(SH_COMMAND sh)
        set(SH_COMMAND_ARGS)
    endif()
    ExternalProject_Add(Boost
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/boost"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND "${SH_COMMAND}" ${SH_COMMAND_ARGS} -c "./build-android.sh ${ANDROID_NDK}"
        BUILD_IN_SOURCE 1)
    # TODO call osvr_android_add_to_common_prefix_path - might be something like this
    #ExternalProject_Get_Property(Boost SOURCE_DIR)
    #osvr_android_add_to_common_prefix_path("${SOURCE_DIR}")
endif()

###
# Android builds from source
###

ExternalProject_Add(jsoncpp
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    -DJSONCPP_WITH_CMAKE_PACKAGE=ON
    -DJSONCPP_LIB_BUILD_SHARED=ON
    "-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}" # Have to pass in "host" python
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

# TODO build in <BUILD_DIR>, install in <INSTALL_DIR>
ExternalProject_Add(libusb
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb/android/jni"
    CONFIGURE_COMMAND ${ANDROID_NDK_BUILD_WITH_ARGS} clean -C <SOURCE_DIR>
    BUILD_COMMAND ${ANDROID_NDK_BUILD_WITH_ARGS} -C <SOURCE_DIR>
    INSTALL_COMMAND "")

set(LIBUSB1_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb/libusb")
set(LIBUSB1_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/android/jni/${ANDROID_ABI}/libusb1.0.so")
# TODO build hidapi - depends on libusb


# TODO add required projects to DEPENDS
# TODO remove hard-coded paths
ExternalProject_Add(OSVR-Core
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    DEPENDS jsoncpp libfunctionality OpenCV osvr_json_to_c_host
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    "-DOSVR_JSON_TO_C_COMMAND=${OSVR_JSON_TO_C_COMMAND}"
    "-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}" # Have to pass in "host" python TODO is this actually needed?
    -DLIBUSB1_INCLUDE_DIR=/Users/appleuser/Documents/OsvrAndroid/libusb-master/libusb/
    -DLIBUSB1_LIBRARY=/Users/appleuser/Documents/OsvrAndroid/libusb-master/android/obj/local/armeabi/libusb1.0.so
    -DHIDAPI_INCLUDE_DIR=/Users/appleuser/Documents/OsvrAndroid/hidapi/hidapi/
    -DHIDAPI_LIBRARY=/Users/appleuser/Downloads/bins/libhidapi.so
    -DJAVA_JVM_LIBRARY=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/jre/lib/server/libjvm.dylib
    -DJAVA_AWT_LIBRARY=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/jre/lib/libawt.dylib
    -DJAVA_INCLUDE_PATH=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    -DJAVA_INCLUDE_PATH2=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    -DJAVA_AWT_INCLUDE_PATH=/Library/Java/JavaVirtualMachines/jdk1.8.0_40.jdk/Contents/Home/include/
    BUILD_IN_SOURCE 0)
