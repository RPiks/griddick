# Stamp firmware identity into griddick_build.h.
# Called from the griddick_tnc build, not from the host build.
# firmware_version is the independent key in manifest.yaml (repo root).

if(NOT GRIDDICK_MANIFEST OR NOT GRIDDICK_OUT)
    message(FATAL_ERROR "stamp_build: GRIDDICK_MANIFEST and GRIDDICK_OUT required")
endif()
if(NOT EXISTS "${GRIDDICK_MANIFEST}")
    message(FATAL_ERROR "stamp_build: manifest not found: ${GRIDDICK_MANIFEST}")
endif()

file(READ "${GRIDDICK_MANIFEST}" _man)
if(NOT _man MATCHES "firmware_version:[ \t]*\"([^\"]+)\"")
    message(FATAL_ERROR "stamp_build: firmware_version missing in ${GRIDDICK_MANIFEST}")
endif()
set(_ver "${CMAKE_MATCH_1}")
if(_ver MATCHES "[\"\\\\]")
    message(FATAL_ERROR "stamp_build: firmware_version is not a plain JSON string")
endif()

string(TIMESTAMP _utc "%Y-%m-%dT%H:%M:%SZ" UTC)

file(WRITE "${GRIDDICK_OUT}"
"#define GRIDDICK_FW_VERSION \"${_ver}\"\n#define GRIDDICK_BUILD_UTC \"${_utc}\"\n")
message(STATUS "firmware build ${_ver} ${_utc}")
