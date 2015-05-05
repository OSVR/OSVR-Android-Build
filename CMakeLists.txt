cmake_minimum_required(VERSION 3.0)
project(OSVR-Android LANGUAGES)

include(ExternalProject)

###
# Configuration
###
if(NOT ANDROID_ABI)
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "Android ABI to build all projects with - see android-cmake docs for details")
    set_property(CACHE ANDROID_ABI PROPERTY STRINGS "armeabi" "armeabi-v7a" "armeabi-v7a with NEON" "armeabi-v7a with VFPV3" "armeabi-v6 with VFP" "x86" "mips" "arm64-v8a" "x86_64" "mips64")
endif()

if(NOT ANDROID_STL)
    set(ANDROID_STL "gnustl_shared" CACHE STRING "Android STL")
    set_property(CACHE ANDROID_ABI PROPERTY STRINGS "gnustl_shared")
endif()

if(NOT ANDROID_TOOLCHAIN_NAME)
    set(ANDROID_TOOLCHAIN_NAME "arm-linux-androideabi-4.9" CACHE STRING "Android Toolchain name")
    set_property(CACHE ANDROID_TOOLCHAIN_NAME PROPERTY STRINGS "arm-linux-androideabi-4.9")
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




###
# NDK finding - using environment and CMake variables as a hint
###
if(NOT ANDROID_NDK)
    set(ANDROID_NDK $ENV{ANDROID_NDK} CACHE PATH "The root directory of an Android NDK")
endif()

if(ANDROID_NDK)
    set(ANDROID_NDK_HINT ${ANDROID_NDK})
endif()

find_program(ANDROID_NDK_BUILD
    NAMES
    ndk-build.cmd
    ndk-build
    HINTS
    ${ANDROID_NDK_HINT}
    DOC "Root directory of android NDK")

if(NOT ANDROID_NDK_BUILD)
    message(FATAL_ERROR "Need the Android NDK path set, by finding the ndk-build command!")
endif()

###
# Build config handling
###
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
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "" FORCE)
    set(ANDROID_BUILD_TYPE $<CONFIG>)
endif()

###
# Setting some variables and properties
###
set(ANDROID_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/android-cmake/android.toolchain.cmake")
file(TO_NATIVE_PATH "${ANDROID_TOOLCHAIN}" ANDROID_TOOLCHAIN_NATIVE_PATH)

set(ANDROID_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/install")
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${ANDROID_INSTALL_DIR}")

get_filename_component(ANDROID_NDK ${ANDROID_NDK_BUILD} DIRECTORY CACHE)

set(COMMON_CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${ANDROID_INSTALL_DIR}"
    "-DCMAKE_PREFIX_PATH=${ANDROID_INSTALL_DIR}"
    "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${ANDROID_TOOLCHAIN}"
    "-DCMAKE_TOOLCHAIN_NAME:STRING=${ANDROID_TOOLCHAIN_NAME}"
    "-DANDROID_STL:STRING=${ANDROID_STL}"
    "-DANDROID_ABI:STRING=${ANDROID_ABI}"
    "-DCMAKE_BUILD_TYPE=${ANDROID_BUILD_TYPE}")

###
# Find make
###
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
        list(APPEND COMMON_CMAKE_ARGS "-DCMAKE_MAKE_PROGRAM=${ANDROID_NDK_MAKE}")
    endif()
    # todo find ninja
endif()

###
# Find other host programs
###
find_package(PythonInterp REQUIRED)

# Go into the subdirectory with the projects in it
add_subdirectory(src)