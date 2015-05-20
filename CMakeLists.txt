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

# This is the only one that supports rtti and exceptions,
# except for libc++ which isn't supported by android-cmake yet.
set(ANDROID_STL gnustl_static)

# if(NOT ANDROID_TOOLCHAIN_NAME)
#     set(ANDROID_TOOLCHAIN_NAME "arm-linux-androideabi-4.9" CACHE STRING "Android Toolchain name")
#     set_property(CACHE ANDROID_TOOLCHAIN_NAME PROPERTY STRINGS "arm-linux-androideabi-4.9")
# endif()
#
if(NOT ANDROID_NATIVE_API_LEVEL)
    set(ANDROID_NATIVE_API_LEVEL "14" CACHE STRING "Android Native API Level")
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

if(WIN32)
find_program(ANDROID_NDK_BUILD
    NAMES
    ndk-build.cmd
    HINTS
    ${ANDROID_NDK_HINT}
    DOC "Root directory of android NDK")
else()
find_program(ANDROID_NDK_BUILD
    NAMES
    ndk-build
    HINTS
    ${ANDROID_NDK_HINT}
    DOC "Root directory of android NDK")
endif()


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
if(EXISTS "${ANDROID_NDK}/sources/crystax")
    # We have the CrystaX NDK. This is just a simple way to check
    set(ANDROID_CRYSTAX_NDK ON)
endif()

set(ANDROID_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/android-cmake/android.toolchain.cmake")
file(TO_NATIVE_PATH "${ANDROID_TOOLCHAIN}" ANDROID_TOOLCHAIN_NATIVE_PATH)

set(ANDROID_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/install")
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${ANDROID_INSTALL_DIR}")

set(HOST_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/host-install")
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${HOST_INSTALL_DIR}")

get_filename_component(ANDROID_NDK ${ANDROID_NDK_BUILD} DIRECTORY CACHE)

set(ANDROID_NDK_BUILD_WITH_ARGS
    "${ANDROID_NDK_BUILD}"
    #NDK_TOOLCHAIN=${ANDROID_TOOLCHAIN_NAME}
    APP_ABI=${ANDROID_ABI}
    "NDK_OUT=<BINARY_DIR>"
    "NDK_APP_DST_DIR=<INSTALL_DIR>/lib"
    )

set(ANDROID_CMAKE_PREFIX_PATH)
set(ANDROID_COMMON_CMAKE_ARGS)
# Internal utility macro for generating COMMON_CMAKE_ARGS
macro(_osvr_android_update_common_cmake_args)
    set(COMMON_CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${ANDROID_INSTALL_DIR}"
        "-DCMAKE_PREFIX_PATH=${ANDROID_CMAKE_PREFIX_PATH}"
        "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${ANDROID_TOOLCHAIN}"
        #"-DANDROID_TOOLCHAIN_NAME:STRING=${ANDROID_TOOLCHAIN_NAME}"
        "-DANDROID_NDK:PATH=${ANDROID_NDK}"
        "-DANDROID_STL:STRING=${ANDROID_STL}"
        "-DANDROID_ABI:STRING=${ANDROID_ABI}"
        "-DCMAKE_BUILD_TYPE=${ANDROID_BUILD_TYPE}"
        "-DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL}"
        ${ANDROID_COMMON_CMAKE_ARGS}
        )
endmacro()

# Creates/updates the COMMON_CMAKE_ARGS, after adding the given argument
macro(osvr_android_add_common_cmake_arg _arg)
    list(APPEND ANDROID_COMMON_CMAKE_ARGS ${_arg})
    _osvr_android_update_common_cmake_args()
endmacro()

# Creates/updates the COMMON_CMAKE_ARGS, after adding the given path to the
# ANDROID_CMAKE_PREFIX_PATH (the list that gets passed to Android CMake builds)
macro(osvr_android_add_to_common_prefix_path _newpath)
    list(APPEND ANDROID_CMAKE_PREFIX_PATH ${_newpath})
    _osvr_android_update_common_cmake_args()
endmacro()

# Initial run to generate COMMON_CMAKE_ARGS, only have one entry for the prefix path
osvr_android_add_to_common_prefix_path("${ANDROID_INSTALL_DIR}")

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
        osvr_android_add_common_cmake_arg("-DCMAKE_MAKE_PROGRAM=${ANDROID_NDK_MAKE}")
    endif()
    # todo find ninja
endif()

###
# Find other host programs
###
find_package(PythonInterp REQUIRED)

###
# Messages regarding config
###
if(ANDROID_CRYSTAX_NDK)
    message(STATUS "Using a CrystaX NDK")
    message(STATUS "toolchain is ${ANDROID_TOOLCHAIN}")
    # Use shared libcrystax consistently for all modules.
    osvr_android_add_common_cmake_arg("-DANDROID_CRYSTAX_NDK_SHARED_LIBCRYSTAX=ON")
else()
    message(STATUS "Using a standard (Google) Android NDK")
endif()

# Go into the subdirectory with the projects in it
add_subdirectory(src)

###
# Bundle stuff up
###
set(CPACK_GENERATOR TGZ)
include(CPack)
