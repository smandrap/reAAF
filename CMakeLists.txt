cmake_minimum_required(VERSION 3.15)
project(reAAF VERSION 0.1.2 LANGUAGES C CXX)

# -------------------------------------------------------------------------
# LogBuffer standalone test binary (no REAPER SDK required)
# -------------------------------------------------------------------------
add_executable(test_logbuffer
        tests/test_logbuffer.cpp
        src/LogBuffer.cpp
)
target_include_directories(test_logbuffer PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/extern/reaper-sdk/WDL
)
set_target_properties(test_logbuffer PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_logbuffer PRIVATE pthread)
elseif(UNIX AND APPLE)
    # pthread is provided by libSystem on macOS — no explicit link needed
endif()

# -------------------------------------------------------------------------
# test_progressdialog_format: formatEntry() unit tests
# -------------------------------------------------------------------------
add_executable(test_progressdialog_format
        tests/test_progressdialog_format.cpp
        src/LogBuffer.cpp
)
target_include_directories(test_progressdialog_format PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/extern/reaper-sdk/WDL
)
set_target_properties(test_progressdialog_format PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_progressdialog_format PRIVATE pthread)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------------
# Paths
# -------------------------------------------------------------------------
set(REAPER_SDK_DIR "${CMAKE_SOURCE_DIR}/extern/reaper-sdk" CACHE PATH "Path to justinfrankel/reaper-sdk checkout")
set(LIBAAF_DIR     "${CMAKE_SOURCE_DIR}/extern/LibAAF"     CACHE PATH "Path to agfline/LibAAF checkout")

# -------------------------------------------------------------------------
# Version header
# -------------------------------------------------------------------------
configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h)

# -------------------------------------------------------------------------
# LibAAF — use its own build system, static only, no tools/docs/tests
# Save and restore output dirs so LibAAF doesn't redirect our plugin binary
# -------------------------------------------------------------------------
set(_SAVE_ARCHIVE_DIR ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set(_SAVE_LIBRARY_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(_SAVE_RUNTIME_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(BUILD_STATIC_LIB ON  CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS      OFF CACHE BOOL "" FORCE)
set(BUILD_DOC        OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TEST  OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
add_subdirectory(${LIBAAF_DIR} extern/LibAAF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${_SAVE_ARCHIVE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${_SAVE_LIBRARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${_SAVE_RUNTIME_DIR})

# -------------------------------------------------------------------------
# Plugin sources
# -------------------------------------------------------------------------
add_library(reaper_aaf MODULE
        src/main.cpp
        src/helpers.cpp
        src/RppWriter.cpp
        src/AafImporter.cpp
        src/FadeResolver.cpp
        src/LogBuffer.cpp
        src/PrefsPage.cpp
        src/ProgressDialog.cpp
        ${REAPER_SDK_DIR}/WDL/wingui/wndsize.cpp
)

set_target_properties(reaper_aaf PROPERTIES
        PREFIX ""
        OUTPUT_NAME "reaper_reAAF"
)

# -------------------------------------------------------------------------
# Include paths
# -------------------------------------------------------------------------
target_include_directories(reaper_aaf PRIVATE
        src
        include
        ${CMAKE_BINARY_DIR}/generated
        ${LIBAAF_DIR}/include
        ${REAPER_SDK_DIR}/sdk
        ${REAPER_SDK_DIR}/WDL
)

# -------------------------------------------------------------------------
# Link LibAAF static lib — its PUBLIC include dirs propagate automatically
# -------------------------------------------------------------------------
target_link_libraries(reaper_aaf PRIVATE aaf-static)

# -------------------------------------------------------------------------
# REAPER API — only what we actually call
# -------------------------------------------------------------------------
target_compile_definitions(reaper_aaf PRIVATE
        REAPERAPI_MINIMAL
        REAPERAPI_WANT_ShowConsoleMsg
        REAPERAPI_WANT_SetExtState
        REAPERAPI_WANT_GetExtState
        REAPERAPI_WANT_HasExtState
        REAPERAPI_WANT_GetMainHwnd
)

# -------------------------------------------------------------------------
# Platform specifics
# -------------------------------------------------------------------------
if(WIN32)
    set_target_properties(reaper_aaf PROPERTIES SUFFIX ".dll")
    target_compile_definitions(reaper_aaf PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
    target_sources(reaper_aaf PRIVATE src/resource.rc)
    set(REAPER_PLUGINS_DIR "$ENV{APPDATA}/REAPER/UserPlugins")
    add_custom_command(TARGET reaper_aaf POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:reaper_aaf>
            "${REAPER_PLUGINS_DIR}/reaper_reAAF.dll"
            COMMENT "Copying to REAPER UserPlugins..."
    )

elseif(APPLE)
    set_target_properties(reaper_aaf PROPERTIES
            SUFFIX ".dylib"
            OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}"
    )
    target_compile_definitions(reaper_aaf PRIVATE SWELL_PROVIDED_BY_APP)
    target_link_libraries(reaper_aaf PRIVATE "-framework CoreFoundation")
    # swell-modstub.mm defines SWELL_curmodule_dialogresource_head and populates
    # SWELL function pointers at startup (required for any SWELL UI call)
    target_sources(reaper_aaf PRIVATE
            ${REAPER_SDK_DIR}/WDL/swell/swell-modstub.mm
            src/resource_swell.cpp
    )
    target_link_libraries(reaper_aaf PRIVATE "-framework Cocoa")
    set(REAPER_PLUGINS_DIR "$ENV{HOME}/Library/Application Support/REAPER/UserPlugins")
    add_custom_command(TARGET reaper_aaf POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:reaper_aaf>
            "${REAPER_PLUGINS_DIR}/reaper_reAAF-${CMAKE_SYSTEM_PROCESSOR}.dylib"
            COMMENT "Copying to REAPER UserPlugins..."
    )

else()  # Linux
    set_target_properties(reaper_aaf PROPERTIES SUFFIX ".so")
    target_compile_definitions(reaper_aaf PRIVATE SWELL_PROVIDED_BY_APP)
    target_link_libraries(reaper_aaf PRIVATE dl)
    # swell-modstub-generic.cpp defines SWELL_curmodule_dialogresource_head on Linux
    target_sources(reaper_aaf PRIVATE
            ${REAPER_SDK_DIR}/WDL/swell/swell-modstub-generic.cpp
            src/resource_swell.cpp
    )
endif()
