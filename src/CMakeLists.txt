# See URL below for more documentation on ExternalProject
# http://www.cmake.org/cmake/help/v3.0/module/ExternalProject.html

ExternalProject_Add(libfunctionality
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfunctionality"
    INSTALL_DIR "${ANDROID_INSTALL_DIR}"
    CMAKE_GENERATOR "${ANDROID_GENERATOR}"
    CMAKE_ARGS ${COMMON_CMAKE_ARGS} # put any others here
    BUILD_IN_SOURCE 0)