cmake_minimum_required(VERSION 3.0)
project(OSVR-Android LANGUAGES)

include(ExternalProject)

set(ANDROID_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/android-cmake/android.toolchain.cmake")
file(TO_NATIVE_PATH "${ANDROID_TOOLCHAIN}" ANDROID_TOOLCHAIN_NATIVE_PATH)

if(NOT ANDROID_ABI)
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "Android ABI to build all projects with - see android-cmake docs for details")
    set_property(CACHE ANDROID_ABI PROPERTY STRINGS "armeabi" "armeabi-v7a" "armeabi-v7a with NEON" "armeabi-v7a with VFPV3" "armeabi-v6 with VFP" "x86" "mips" "arm64-v8a" "x86_64" "mips64")
endif()

if(NOT ANDROID_GENERATOR)
    if(WIN32)
        set(ANDROID_GENERATOR "MinGW Makefiles" CACHE STRING "The CMake generator to use when building projects.")
        set_property(CACHE ANDROID_GENERATOR PROPERTY STRINGS "MinGW Makefiles" "Ninja")
    else()
        set(ANDROID_GENERATOR "Unix Makefiles" CACHE STRING "The CMake generator to use when building projects.")
        set_property(CACHE ANDROID_GENERATOR PROPERTY STRINGS "Unix Makefiles" "Ninja")
    endif()
endif()

if(ENV{ANDROID_NDK})
    set(ANDROID_NDK_HINT $ENV{ANDROID_NDK})
elseif(ANDROID_NDK)
    set(ANDROID_NDK_HINT ${ANDROID_NDK})
endif()

find_program(ANDROID_NDK_BUILD
    NAMES
    ndk-build
    ndk-build.cmd
    HINTS
    ${ANDROID_NDK_HINT}
    DOC "Root directory of android NDK")

if(NOT ANDROID_NDK_BUILD)
    message(FATAL_ERROR "Need the Android NDK path set, by finding the ndk-build command!")
endif()

get_filename_component(ANDROID_NDK ${ANDROID_NDK_BUILD} DIRECTORY)

set(ANDROID_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/install")

if(NOT CMAKE_CONFIGURATION_TYPES)
    # Single config generator
    if(NOT CMAKE_BUILD_TYPE)
        # no config set.
        set(ANDROID_BUILD_TYPE Release)
    elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Release" OR "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
        # OK config set
        set(ANDROID_BUILD_TYPE ${CMAKE_BUILD_TYPE})
    else()# bad config set
        message(STATUS "${CMAKE_BUILD_TYPE} is not a valid build type for the Android toolchain - must be Release or Debug.")
        set(ANDROID_BUILD_TYPE Release)
    endif()

    message(STATUS "Android build config: ${ANDROID_BUILD_TYPE}")
else()
    # Multi-config generator - will need generator expression
    set(ANDROID_BUILD_TYPE $<CONFIG>)
endif()

set(COMMON_CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${ANDROID_INSTALL_DIR}"
    "-DCMAKE_PREFIX_PATH=${ANDROID_INSTALL_DIR}"
    "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${ANDROID_TOOLCHAIN}"
    "-DANDROID_ABI:STRING=${ANDROID_ABI}"
    "-DCMAKE_BUILD_TYPE=${ANDROID_BUILD_TYPE}")

if(WIN32)
    if("${ANDROID_GENERATOR}" STREQUAL "MinGW Makefiles")
        find_program(ANDROID_NDK_MAKE
            NAMES
            make.exe
            make
            HINTS
            ${ANDROID_NDK}/prebuilt/windows-x86_64/bin)
        if(NOT ANDROID_NDK_MAKE)
            message(FATAL_ERROR "Need the location of a make executable - usually bundled with the NDK!")
        endif()
        list(APPEND COMMON_CMAKE_ARGS -DCMAKE_MAKE_PROGRAM="${ANDROID_NDK_MAKE}")
    endif()
    # todo find ninja
endif()

add_subdirectory(src)