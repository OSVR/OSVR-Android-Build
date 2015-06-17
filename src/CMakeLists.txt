# See URLs below for more documentation on ExternalProject
# http://www.cmake.org/cmake/help/v3.0/module/ExternalProject.html
# http://www.kitware.com/media/html/BuildingExternalProjectsWithCMake2.8.html

set(osvr_dep_targets)

if(BUILD_SERVER)
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
        "-DCMAKE_PREFIX_PATH=${HOST_PREFIX_PATH}"
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
    list(APPEND osvr_dep_targets osvr_json_to_c_host)
endif()

###
# Android Binary SDKs
###

# OpenCV has a binary SDK for Android - just download and unpack.
set(OPENCV_VERSION 2.4.11)
ExternalProject_Add(OpenCV
    URL http://downloads.sourceforge.net/project/opencvlibrary/opencv-android/${OPENCV_VERSION}/OpenCV-${OPENCV_VERSION}-android-sdk.zip
    URL_HASH SHA1=ACFB4789B78752AE5C52CC5C151E2AE3DD006CEF
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

# Find out where OpenCV was extracted so we can extend the prefix path.
ExternalProject_Get_Property(OpenCV SOURCE_DIR)
set(OpenCV_DIR "${SOURCE_DIR}/sdk/native/jni")

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

if(BUILD_SERVER)
    ExternalProject_Add(libfunctionality
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfunctionality"
        INSTALL_DIR "${ANDROID_INSTALL_DIR}"
        CMAKE_GENERATOR "${ANDROID_GENERATOR}"
        CMAKE_ARGS ${COMMON_CMAKE_ARGS} # put any others here
        BUILD_IN_SOURCE 0)
    list(APPEND osvr_dep_targets libfunctionality)
endif()

if(BUILD_SERVER_PLUGINS)
    ExternalProject_Add(libusb
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb"
        INSTALL_DIR "${ANDROID_INSTALL_DIR}"
        CONFIGURE_COMMAND ${ANDROID_NDK_BUILD_WITH_ARGS} clean -C "<SOURCE_DIR>/android/jni"
        BUILD_COMMAND ${ANDROID_NDK_BUILD_WITH_ARGS} -C "<SOURCE_DIR>/android/jni"
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E make_directory "<INSTALL_DIR>/include"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "<SOURCE_DIR>/libusb/libusb.h" "<INSTALL_DIR>/include/libusb.h"
        BUILD_IN_SOURCE 0)
    list(APPEND osvr_dep_targets libusb)
endif()

ExternalProject_Add(OSVR-Core
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    DEPENDS jsoncpp OpenCV ${osvr_dep_targets}
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    "-DOSVR_JSON_TO_C_COMMAND=${OSVR_JSON_TO_C_COMMAND}"
    "-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}" # Have to pass in "host" python TODO is this actually needed?
    "-DOpenCV_DIR=${OpenCV_DIR}"
    "-DLIBUSB1_LIBRARY=${ANDROID_INSTALL_DIR}/lib/libusb1.0.so" # TODO cmake should be able to find this for us.
    "-DVRPN_HIDAPI_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/hidapi" # Let VRPN build HIDAPI for us.
    ${OSVR_CORE_PASSTHROUGH_OPTIONS}
    BUILD_IN_SOURCE 0)


configure_file(packaging/setup.sh ${ANDROID_INSTALL_DIR}/setup.sh COPYONLY)
configure_file(packaging/launcher.sh ${ANDROID_INSTALL_DIR}/bin/launcher.sh COPYONLY)
install(DIRECTORY ${ANDROID_INSTALL_DIR}/bin/ DESTINATION bin)
install(DIRECTORY ${ANDROID_INSTALL_DIR}/lib/ DESTINATION lib
    FILES_MATCHING
    PATTERN *.so)
install(PROGRAMS ${ANDROID_INSTALL_DIR}/setup.sh DESTINATION .)
