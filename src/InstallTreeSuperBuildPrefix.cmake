# Injected by OSVR-Android-Build.
# We might be in an install directory with a host-install peer directory,
# where a host-executable osvr_json_to_c might live.

# Compute the installation prefix relative to this file.
get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)

set(_OSVR_POSSIBLE_OSVR_JSON_TO_C_LOCATION "${_IMPORT_PREFIX}/../host-install/bin/osvr_json_to_c")
if(CMAKE_HOST_WIN32)
    set(_OSVR_POSSIBLE_OSVR_JSON_TO_C_LOCATION "${_OSVR_POSSIBLE_OSVR_JSON_TO_C_LOCATION}.exe")
endif()
if(EXISTS "${_OSVR_POSSIBLE_OSVR_JSON_TO_C_LOCATION}")
    # By just setting in cache, this allows an override, if desired.
    set(OSVR_JSON_TO_C_EXECUTABLE "${_OSVR_POSSIBLE_OSVR_JSON_TO_C_LOCATION}" CACHE FILEPATH "")
endif()
