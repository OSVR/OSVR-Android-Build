# See URLs below for more documentation on ExternalProject
# http://www.cmake.org/cmake/help/v3.0/module/ExternalProject.html
# http://www.kitware.com/media/html/BuildingExternalProjectsWithCMake2.8.html

# Since what OSVR-Core depends on varies based on build configurations, we
# accumulate the dependencies in this variable.
set(osvr_dep_targets)

# Stores the paths to the build stamp files we want to delete if we build the "force_rebuild" target
set(force_build_stamps)
set(force_build_targets)

# pass the external project target name, then any steps you want to
# force in addition to just "build"
function(add_ep_to_force _target)
    ExternalProject_Get_Property(${_target} STAMP_DIR)
    foreach(step build ${ARGN})
        list(APPEND force_build_stamps "${STAMP_DIR}/${_target}-${step}")
    endforeach()
    set(force_build_targets ${force_build_targets} ${_target} PARENT_SCOPE)
    set(force_build_stamps "${force_build_stamps}" PARENT_SCOPE)
endfunction()

###
# Conditionally build Boost
###
if(NOT ANDROID_CRYSTAX_NDK)
    # In this case, we need boost.
    if(WIN32)
        # and msys bash
        find_program(SH_COMMAND NAMES bash.exe dash.exe ash.sh sh.exe)
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

if(BUILD_SERVER)
    ###
    # Host (native, not Android) projects from source
    ###
    set(HOST_PREFIX_PATH ${CMAKE_PREFIX_PATH})

    if(TARGET osvr::osvr_json_to_c)
        message(STATUS "Using host osvr_json_to_c from build at ${osvr_DIR}")
        set(OSVR_JSON_TO_C_COMMAND $<TARGET_FILE:osvr::osvr_json_to_c>)
        # dummy target
        add_custom_target(osvr_json_to_c_host)
    elseif(OSVR_JSON_TO_C_COMMAND AND EXISTS "${OSVR_JSON_TO_C_COMMAND}")
        message(STATUS "Using specified host osvr_json_to_c ${OSVR_JSON_TO_C_COMMAND}")
        # dummy target
        add_custom_target(osvr_json_to_c_host)
    else()
        # need to build it ourselves
        ExternalProject_Add(jsoncpp_host
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp"
            CMAKE_ARGS
            "-DCMAKE_INSTALL_PREFIX=${HOST_INSTALL_DIR}"
            -DJSONCPP_WITH_CMAKE_PACKAGE=ON
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_STATIC_LIBS=ON
            -DJSONCPP_WITH_TESTS=OFF
            -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF
            "-DCMAKE_PREFIX_PATH=${HOST_PREFIX_PATH}"
            BUILD_IN_SOURCE 0)
        add_ep_to_force(jsoncpp_host)
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
        add_ep_to_force(osvr_json_to_c_host)
    endif()
endif()

###
# Android Binary SDKs
###

# OpenCV has a binary SDK for Android - just download and unpack.
set(OPENCV_VERSION 2.4.11)
set(SHA1 ACFB4789B78752AE5C52CC5C151E2AE3DD006CEF)
set(FN OpenCV-${OPENCV_VERSION}-android-sdk.zip)
# Use our own script to download, since ExternalProject re-downloads too often.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FN}"
    # Downlaod the file if we need it.
    COMMAND "${CMAKE_COMMAND}"
    "-DEXTERNAL_LOCATION=${CMAKE_CURRENT_SOURCE_DIR}/${FN}"
    "-DLOCAL_LOCATION=${CMAKE_CURRENT_BINARY_DIR}/${FN}"
    "-DURL=http://downloads.sourceforge.net/project/opencvlibrary/opencv-android/${OPENCV_VERSION}/${FN}"
    "-DSHA1=${SHA1}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/DownloadScript.cmake"
    # Once we have it, copy it to the source tree to save download time.
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/${FN}" "${CMAKE_CURRENT_SOURCE_DIR}/${FN}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DownloadScript.cmake"
    VERBATIM)
add_custom_target(OpenCV_Download DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${FN}")

ExternalProject_Add(OpenCV
    DEPENDS OpenCV_Download
#    URL http://downloads.sourceforge.net/project/opencvlibrary/opencv-android/${OPENCV_VERSION}/OpenCV-${OPENCV_VERSION}-android-sdk.zip
    URL "${CMAKE_CURRENT_BINARY_DIR}/${FN}"
    URL_HASH SHA1=${SHA1}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

# Find out where OpenCV was extracted so we can extend the prefix path.
ExternalProject_Get_Property(OpenCV SOURCE_DIR)
set(OpenCV_DIR "${SOURCE_DIR}/sdk/native/jni")

# SDL 2.0 has a source zip file for Android.
#set(SHA1 00b2abe02de0ab1c9565955d56e6ac66bd342c07)
#set(FN SDL2-2.0.4.zip)
# Use our own script to download, since ExternalProject re-downloads too often.
#add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FN}"
#    # Downlaod the file if we need it.
#    COMMAND "${CMAKE_COMMAND}"
#    "-DEXTERNAL_LOCATION=${CMAKE_CURRENT_SOURCE_DIR}/${FN}"
#    "-DLOCAL_LOCATION=${CMAKE_CURRENT_BINARY_DIR}/${FN}"
#    "-DSHA1=${SHA1}"
#    "-DURL=http://www.libsdl.org/release/${FN}"
#    "-DANDROID_FORCE_ARM_BUILD=ON"
#    -P "${CMAKE_CURRENT_SOURCE_DIR}/DownloadScript.cmake"
#    # Once we have it, copy it to the source tree to save download time.
#    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DownloadScript.cmake"
#    VERBATIM)
# add_custom_target(SDL2_Download DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${FN}")

# Instructions at end of https://wiki.libsdl.org/Android indicate that this
# is experimental but should work (toolchain is defined in the COMMON_CMAKE_ARGS).
# @todo Why is it having a problem finding wchar_t?
#message(STATUS "Making SDL2 with SDL_SHARED=FALSE")
#ExternalProject_Add(SDL2
#    URL "${CMAKE_CURRENT_BINARY_DIR}/${FN}"
#    URL_HASH SHA1=${SHA1}
#    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
#    DEPENDS SDL2_Download
#    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
#    CMAKE_ARGS
#    ${COMMON_CMAKE_ARGS}
#    -DSDL_SHARED=FALSE
#    -C ${CMAKE_CURRENT_SOURCE_DIR}/SDL-add-ons.cmake
#)

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
    -DBUILD_SHARED_LIBS=ON
    -DBUILD_STATIC_LIBS=OFF
    "-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}" # Have to pass in "host" python
    -DJSONCPP_WITH_TESTS=OFF
    -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF
    # put any others here
    BUILD_IN_SOURCE 0)
add_ep_to_force(jsoncpp)

if(BUILD_SERVER)
    ExternalProject_Add(libfunctionality
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfunctionality"
        INSTALL_DIR "${ANDROID_INSTALL_DIR}"
        CMAKE_GENERATOR "${ANDROID_GENERATOR}"
        CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DBUILD_TESTING=OFF # put any others here
        BUILD_IN_SOURCE 0)
    list(APPEND osvr_dep_targets libfunctionality)
    add_ep_to_force(libfunctionality)
endif()

if(BUILD_SERVER_PLUGINS)
    ExternalProject_Add(libusb
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb"
        INSTALL_DIR "${ANDROID_INSTALL_DIR}"
        CONFIGURE_COMMAND ${ANDROID_NDK_BUILD_WITH_ARGS} clean -C "<SOURCE_DIR>/android/jni"
        BUILD_COMMAND ${ANDROID_NDK_BUILD_WITH_ARGS} -C "<SOURCE_DIR>/android/jni"
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E make_directory "<INSTALL_DIR>/include/libusb-1.0"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "<SOURCE_DIR>/libusb/libusb.h" "<INSTALL_DIR>/include/libusb-1.0/libusb.h"
        BUILD_IN_SOURCE 0)
    list(APPEND osvr_dep_targets libusb)
endif()

###
# Flags that will be used not only by OSVR-Core but by any plugins:

# Must set this, to avoid finding MinGW or other local, native OpenCV's
osvr_android_add_common_cmake_arg("-DOpenCV_DIR=${OpenCV_DIR}")

# Indicate where osvr_json_to_c is for ease of use
osvr_android_add_common_cmake_arg("-DOSVR_JSON_TO_C_COMMAND=${OSVR_JSON_TO_C_COMMAND}")

ExternalProject_Add(OSVR-Core
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    DEPENDS jsoncpp OpenCV ${osvr_dep_targets}
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    "-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}" # Have to pass in "host" python TODO is this actually needed?
    "-DLIBUSB1_LIBRARY=${ANDROID_INSTALL_DIR}/lib/libusb1.0.so" # TODO cmake should be able to find this for us.
    "-DVRPN_HIDAPI_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/hidapi" # Let VRPN build HIDAPI for us.
    ${OSVR_CORE_PASSTHROUGH_OPTIONS}
    BUILD_IN_SOURCE 0)
add_ep_to_force(OSVR-Core)

ExternalProject_Add(OSVR-RenderManager
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OSVR-RenderManager"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    DEPENDS OSVR-Core #SDL2
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS
    ${COMMON_CMAKE_ARGS}
    "-DEIGEN3_INCLUDE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core/vendor/eigen"
    "-DVRPN_INCLUDE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core/vendor/vrpn"
    "-DVRPN_LIBRARY:PATH=${CMAKE_CURRENT_BINARY_DIR}/OSVR-Core-prefix/src/OSVR-Core-build/bin/libvrpnserver.a"
    "-DQUATLIB_INCLUDE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/OSVR-Core/vendor/vrpn/quat"
    "-DQUATLIB_LIBRARY:PATH=${CMAKE_CURRENT_BINARY_DIR}/OSVR-Core-prefix/src/OSVR-Core-build/bin/libquat.a"
    "-DCMAKE_CXX_FLAGS:STRING=-std=c++11"
    BUILD_IN_SOURCE 0)
add_ep_to_force(OSVR-RenderManager)

if(BUILD_SERVER_PLUGINS)
    ExternalProject_Add(android_sensor_tracker
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android_sensor_tracker/com_osvr_android_sensorTracker"
        INSTALL_DIR "${ANDROID_INSTALL_DIR}"
        DEPENDS OSVR-Core
        CMAKE_GENERATOR "${ANDROID_GENERATOR}"
        CMAKE_ARGS
        ${COMMON_CMAKE_ARGS}
        BUILD_IN_SOURCE 0)
	add_ep_to_force(android_sensor_tracker)

    ExternalProject_Add(jniImaging
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android_sensor_tracker/com_osvr_android_jniImaging"
        INSTALL_DIR "${ANDROID_INSTALL_DIR}"
        DEPENDS OSVR-Core
        CMAKE_GENERATOR "${ANDROID_GENERATOR}"
        CMAKE_ARGS
        ${COMMON_CMAKE_ARGS}
        BUILD_IN_SOURCE 0)
    add_ep_to_force(jniImaging)
endif()

# Add the custom target to delete the build stamps
add_custom_target(force_rebuild
    COMMAND "${CMAKE_COMMAND}" -E remove -f ${force_build_stamps}
    COMMENT "Clearing build stamps to force rebuild of:\n${force_build_targets}"
    VERBATIM)

set_target_properties(force_rebuild
    PROPERTIES
    EXCLUDE_FROM_ALL
    TRUE)
message(STATUS "To tell the build system to proceed to build a subproject that you might have changed directly, build the force_rebuild target then the target you want.")
message(STATUS "Projects: ${force_build_targets}")

configure_file(packaging/setup.sh ${ANDROID_INSTALL_DIR}/setup.sh COPYONLY)
configure_file(packaging/launcher.sh ${ANDROID_INSTALL_DIR}/bin/launcher.sh COPYONLY)
configure_file(InstallTreeSuperBuildPrefix.cmake ${ANDROID_INSTALL_DIR}/lib/cmake/osvr/osvrConfigSuperBuildPrefix.cmake COPYONLY)
install(DIRECTORY ${ANDROID_INSTALL_DIR}/bin/ DESTINATION bin)
install(DIRECTORY ${ANDROID_INSTALL_DIR}/lib/ DESTINATION lib
    FILES_MATCHING
    PATTERN *.so)
install(DIRECTORY ${ANDROID_INSTALL_DIR}/share/osvrcore/ DESTINATION share/osvrcore
    FILES_MATCHING
    PATTERN *.json)
install(PROGRAMS ${ANDROID_INSTALL_DIR}/setup.sh DESTINATION .)
