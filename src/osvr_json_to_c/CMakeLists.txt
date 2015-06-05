# A silly build file to build just the osvr_json_to_c tool out of an OSVR-Core source directory.
cmake_minimum_required(VERSION 3.0)
project(osvr_json_to_c)

###
# Make sure we have OSVR-Core source
###
find_path(OSVR_CORE_SOURCE_DIR devtools/osvr_json_to_c.cpp)
if(NOT OSVR_CORE_SOURCE_DIR OR NOT EXISTS "${OSVR_CORE_SOURCE_DIR}/devtools/osvr_json_to_c.cpp")
    message(FATAL_ERROR "Need OSVR_CORE_SOURCE_DIR set to the root of an OSVR-Core source tree!")
endif()

###
# Some setup from the main OSVR-Core build system
###
list(APPEND CMAKE_MODULE_PATH "${OSVR_CORE_SOURCE_DIR}/cmake" "${OSVR_CORE_SOURCE_DIR}/cmake-local" "${OSVR_CORE_SOURCE_DIR}")
include(MapImportedReleaseVariants)
include(MSVCMultipleProcessCompile)

include(SetDefaultBuildType)
set_default_build_type(RelWithDebInfo)

include(CMakeBoostHelper)
if(WIN32)
    option(Boost_USE_STATIC_LIBS "Build with Boost's static libraries?" ON)
endif()

###
# Deps
###
find_package(jsoncpp REQUIRED)
if(TARGET jsoncpp_lib_static AND NOT TARGET jsoncpp_lib)
    add_library(jsoncpp_lib INTERFACE)
    target_link_libraries(jsoncpp_lib INTERFACE jsoncpp_lib_static)
endif()
find_package(Boost REQUIRED COMPONENTS program_options)

###
# Build and install
###
add_executable(osvr_json_to_c "${OSVR_CORE_SOURCE_DIR}/devtools/osvr_json_to_c.cpp")

target_include_directories(osvr_json_to_c PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(osvr_json_to_c jsoncpp_lib ${Boost_PROGRAM_OPTIONS_LIBRARIES})
install(TARGETS osvr_json_to_c
    RUNTIME DESTINATION bin)
