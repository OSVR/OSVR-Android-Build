cmake_minimum_required(VERSION 3.1)
project(OSVR-Android LANGUAGES)

if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Don't specify a toolchain for this super-build: use your native build tool, the nested builds automatically will have the toolchain specified!")
endif()

include(ExternalProject)

###
# Configuration
###
if(NOT ANDROID_ABI)
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "Android ABI to build all projects with - see android-cmake docs for details")
    set_property(CACHE ANDROID_ABI PROPERTY STRINGS "armeabi" "armeabi-v7a" "armeabi-v7a with NEON" "armeabi-v7a with VFPV3" "armeabi-v6 with VFP" "x86" "mips" "arm64-v8a" "x86_64" "mips64")
endif()

# ndk-build doesn't like decorated ABI strings.
if("${ANDROID_ABI}" MATCHES "armeabi-v7a" AND NOT "${ANDROID_ABI}" STREQUAL "armeabi-v7a")
    set(ANDROID_BARE_NDK_ABI "armeabi-v7a")
else()
    set(ANDROID_BARE_NDK_ABI "${ANDROID_ABI}")
endif()

# This is the only one that supports rtti and exceptions,
# except for libc++ which isn't supported by android-cmake yet.
#set(ANDROID_STL gnustl_static)

# well, except this one seems to work fine
if(NOT ANDROID_STL)
    set(ANDROID_STL gnustl_shared)
endif()

if(NOT ANDROID_TOOLCHAIN_NAME AND ANDROID_ABI MATCHES "armeabi")
    set(ANDROID_TOOLCHAIN_NAME "arm-linux-androideabi-4.9" CACHE STRING "Android Toolchain name")
    set_property(CACHE ANDROID_TOOLCHAIN_NAME PROPERTY STRINGS "arm-linux-androideabi-4.9" "arm-linux-androideabi-clang3.7" "arm-linux-androideabi-clang3.6")
endif()

if(NOT ANDROID_NATIVE_API_LEVEL)
    set(ANDROID_NATIVE_API_LEVEL "16" CACHE STRING "Android Native API Level")
endif()

# Optional, externally-built osvr_json_to_c instance

set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE RELWITHDEBINFO)
find_package(osvr)
unset(CMAKE_MAP_IMPORTED_CONFIG_RELEASE)
if(NOT TARGET osvr::osvr_json_to_c)
    find_program(OSVR_JSON_TO_C_COMMAND NAMES osvr_json_to_c)
endif()

if(WIN32)
    set(MAKEFILE_GENERATOR "MinGW Makefiles")
else()
    set(MAKEFILE_GENERATOR "Unix Makefiles")
endif()
if(NOT ANDROID_GENERATOR)
    find_program(NINJA_COMMAND ninja)
    if(NINJA_COMMAND)
        set(ANDROID_GENERATOR "Ninja" CACHE STRING "The CMake generator to use when building projects.")
    else()
        set(ANDROID_GENERATOR "${MAKEFILE_GENERATOR}" CACHE STRING "The CMake generator to use when building projects.")
    endif()
endif()
set_property(CACHE ANDROID_GENERATOR PROPERTY STRINGS "${MAKEFILE_GENERATOR}" "Ninja")

###
# Options that we pass through to OSVR-Core
###
include("${CMAKE_CURRENT_SOURCE_DIR}/src/OSVR-Core/cmake-local/osvrBuildOptions.cmake")
set(OSVR_CORE_PASSTHROUGH_OPTIONS)
foreach(opt ${OSVR_BUILD_OPTIONS})
    list(APPEND OSVR_CORE_PASSTHROUGH_OPTIONS "-DBUILD_${opt}=${BUILD_${opt}}")
endforeach()

list(APPEND OSVR_CORE_PASSTHROUGH_OPTIONS "-DOSVR_COMMON_IN_PROCESS_IMAGING=1")

###
# NDK finding - using environment and CMake variables as a hint
###
if(NOT ANDROID_NDK)
    get_filename_component(ANDROID_NDK "$ENV{ANDROID_NDK}" ABSOLUTE)
    set(ANDROID_NDK "${ANDROID_NDK}" CACHE PATH "The root directory of an Android NDK" FORCE)
endif()

if(ANDROID_NDK)
    set(ANDROID_NDK_HINT ${ANDROID_NDK})
endif()

if(WIN32)
    set(NDK_BUILD_SUFFIX .cmd)
endif()
find_program(ANDROID_NDK_BUILD
    NAMES
    ndk-build${NDK_BUILD_SUFFIX}
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
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release" OR "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
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

# Excerpted/modified from the code we added to detect CrystaX in android-cmake
if(EXISTS "${ANDROID_NDK}/RELEASE.TXT")
    file(STRINGS "${ANDROID_NDK}/RELEASE.TXT" ANDROID_NDK_GENERIC_RELEASE_FULL LIMIT_COUNT 1 REGEX "[^ ]+")
    if(ANDROID_NDK_GENERIC_RELEASE_FULL AND ANDROID_NDK_GENERIC_RELEASE_FULL MATCHES "crystax-ndk")
        set(ANDROID_CRYSTAX_NDK ON)
    endif()
endif()

set(ANDROID_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/android-cmake/android.toolchain.cmake")
file(TO_NATIVE_PATH "${ANDROID_TOOLCHAIN}" ANDROID_TOOLCHAIN_NATIVE_PATH)

set(ANDROID_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/install")
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${ANDROID_INSTALL_DIR}")

set(HOST_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/host-install")
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${HOST_INSTALL_DIR}")

get_filename_component(ANDROID_NDK ${ANDROID_NDK_BUILD} DIRECTORY CACHE)

if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "${ANDROID_NDK}/prebuilt/windows-x86_64")
endif()

set(ANDROID_NDK_BUILD_WITH_ARGS
    "${ANDROID_NDK_BUILD}"
    NDK_TOOLCHAIN=${ANDROID_TOOLCHAIN_NAME}
    APP_ABI=${ANDROID_BARE_NDK_ABI}
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
        "-DCMAKE_VERBOSE_MAKEFILE:BOOL=${CMAKE_VERBOSE_MAKEFILE}"
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


if(ANDROID_TOOLCHAIN_NAME)
    message(STATUS "Using toolchain ${ANDROID_TOOLCHAIN_NAME}")
    list(APPEND ANDROID_NDK_BUILD_WITH_ARGS
        "NDK_TOOLCHAIN=${ANDROID_TOOLCHAIN_NAME}")
    osvr_android_add_common_cmake_arg("-DANDROID_TOOLCHAIN_NAME:STRING=${ANDROID_TOOLCHAIN_NAME}")
endif()

if("${ANDROID_STL}" MATCHES "c\\+\\+_")

    if(NOT ANDROID_LIBCXX_VERSION AND NOT ("${ANDROID_TOOLCHAIN_NAME}" MATCHES "clang") AND NOT ("${ANDROID_TOOLCHAIN_NAME}" MATCHES "clang"))
        message(STATUS "Specified a GCC toolchain with libc++ - setting a default ANDROID_LIBCXX_VERSION for you.")
        set(ANDROID_LIBCXX_VERSION 3.7)
    endif()
    if(ANDROID_LIBCXX_VERSION)
        message(STATUS "Passing ANDROID_LIBCXX_VERSION=${ANDROID_LIBCXX_VERSION}")
        osvr_android_add_common_cmake_arg("-DANDROID_LIBCXX_VERSION:STRING=${ANDROID_LIBCXX_VERSION}")
    endif()
endif()

if(ANDROID_EXTRA_CXX_FLAGS)
    message(STATUS "Passing along ANDROID_EXTRA_CXX_FLAGS=${ANDROID_EXTRA_CXX_FLAGS} as CMAKE_CXX_FLAGS")
    osvr_android_add_common_cmake_arg("-DCMAKE_CXX_FLAGS:STRING=${ANDROID_EXTRA_CXX_FLAGS}")
endif()

###
# Find/use make/ninja
###
if("${ANDROID_GENERATOR}" STREQUAL "MinGW Makefiles")
    find_program(ANDROID_NDK_MAKE
        NAMES
        make.exe
        make
        mingw32-make
        HINTS
        ${ANDROID_NDK}/prebuilt/windows-x86_64/bin)
    if(NOT ANDROID_NDK_MAKE)
        message(FATAL_ERROR "Need the location of a make executable - usually bundled with the NDK!")
    endif()
    osvr_android_add_common_cmake_arg("-DCMAKE_MAKE_PROGRAM=${ANDROID_NDK_MAKE}")
endif()

if("${ANDROID_GENERATOR}" STREQUAL "Ninja")
    if(NOT NINJA_COMMAND)
        message(FATAL_ERROR "Ninja generator was selected, so need the location of a ninja executable in NINJA_COMMAND! (or switch to the makefiles generator)")
    endif()
    osvr_android_add_common_cmake_arg("-DCMAKE_MAKE_PROGRAM=${NINJA_COMMAND}")
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
    # Use shared libcrystax consistently for all modules.
    osvr_android_add_common_cmake_arg("-DANDROID_CRYSTAX_NDK_SHARED_LIBCRYSTAX=ON")
else()
    message(STATUS "Using a standard (Google) Android NDK")
endif()
message(STATUS "ANDROID_NDK: ${ANDROID_NDK}")
message(STATUS "ANDROID_ABI: ${ANDROID_ABI}")

# Go into the subdirectory with the projects in it
add_subdirectory(src)

###
# Bundle stuff up
###
set(CPACK_GENERATOR TGZ)
include(CPack)
