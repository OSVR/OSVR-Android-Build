# input:
# SHA1 - the SHA1 hash of the file
# EXTERNAL_LOCATION - an optional variable, for if the user wants to specify where they already have the file.
# LOCAL_LOCATION - where the file should be downloaded or copied to.
# URL - the download location.
string(TOLOWER "${SHA1}" SHA1)

message(STATUS "SHA1 ${SHA1}")
message(STATUS "LOCAL_LOCATION ${LOCAL_LOCATION}")

if(EXISTS "${LOCAL_LOCATION}")
    file(SHA1 "${LOCAL_LOCATION}" _actual_hash)
    string(TOLOWER "${_actual_hash}" _actual_hash)
    if("${_actual_hash}" STREQUAL "${SHA1}")
        message(STATUS "Already-downloaded file ${LOCAL_LOCATION} matches expected hash, using it.")
        #set(_FILE_LOCATION "${LOCAL_LOCATION}")
        return()
    else()
        message(STATUS "Already-downloaded file ${LOCAL_LOCATION} exists but does not match hash (actual ${_actual_hash}).")
    endif()
endif()


message(STATUS "EXTERNAL_LOCATION ${EXTERNAL_LOCATION}")
if(EXTERNAL_LOCATION AND EXISTS "${EXTERNAL_LOCATION}")
    file(SHA1 "${EXTERNAL_LOCATION}" _actual_hash)
    string(TOLOWER "${_actual_hash}" _actual_hash)
    if("${_actual_hash}" STREQUAL "${SHA1}")
        message(STATUS "File ${EXTERNAL_LOCATION} matches expected hash, using it.")
        #set(_FILE_LOCATION "${EXTERNAL_LOCATION}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${EXTERNAL_LOCATION}" "${LOCAL_LOCATION}")
        return()
    else()
        message(STATUS "Externally-specified file ${EXTERNAL_LOCATION} exists but does not match hash.")
    endif()
endif()


message(STATUS "Downloading from ${URL}")
set(_DL_DEST "${LOCAL_LOCATION}.tmp")
file(DOWNLOAD
    "${URL}"
    "${_DL_DEST}"
    STATUS _status
    LOG _log)
list(GET _status 0 _status_code)
list(GET _status 1 _status_string)
if(NOT _status_code EQUAL 0)
    message(FATAL_ERROR "error: downloading '${URL}' failed
    status_code: ${_status_code}
    status_string: ${_status_string}
    log: ${_log}")
endif()
message(STATUS "Download complete, checking hash.")
file(SHA1 "${_DL_DEST}" _actual_hash)
string(TOLOWER "${_actual_hash}" _actual_hash)
if(_actual_hash STREQUAL "${SHA1}")
    message(STATUS "Download complete, hash matches.")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${_DL_DEST}" "${LOCAL_LOCATION}")
    #set(_FILE_LOCATION "${LOCAL_LOCATION}")
else()
    message(FATAL_ERROR "Download completed successfully, but hash did not match!")
endif()
