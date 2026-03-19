cmake_minimum_required(VERSION 3.15)
project(reAAF VERSION 0.1.2 LANGUAGES C CXX OBJC OBJCXX)

# Pre-release suffix (e.g. "b3"). Leave empty for a stable release.
set(REAAF_VERSION_SUFFIX "")
set(REAAF_VERSION "${PROJECT_VERSION}${REAAF_VERSION_SUFFIX}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------------
# Helper: standalone unit-test binary (no REAPER SDK required)
# -------------------------------------------------------------------------
function(add_unit_test TARGET)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(${TARGET} ${ARG_SOURCES})
    target_include_directories(${TARGET} PRIVATE
            src
            ${CMAKE_SOURCE_DIR}/extern/reaper-sdk/WDL
    )
    set_target_properties(${TARGET} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
    )
    # pthread is part of libSystem on macOS — no explicit link needed there
    if(UNIX AND NOT APPLE)
        target_link_libraries(${TARGET} PRIVATE pthread)
    endif()
endfunction()

# -------------------------------------------------------------------------
# Unit tests
# -------------------------------------------------------------------------
add_unit_test(test_logbuffer SOURCES
        tests/test_logbuffer.cpp
        src/LogBuffer.cpp
)
add_unit_test(test_progressdialog_format SOURCES
        tests/test_progressdialog_format.cpp
        src/LogBuffer.cpp
)

# -------------------------------------------------------------------------
# Paths
# -------------------------------------------------------------------------
set(REAPER_SDK_DIR "${CMAKE_SOURCE_DIR}/extern/reaper-sdk" CACHE PATH "Path to justinfrankel/reaper-sdk checkout")
set(LIBAAF_DIR     "${CMAKE_SOURCE_DIR}/extern/LibAAF"     CACHE PATH "Path to agfline/LibAAF checkout")

# -------------------------------------------------------------------------
# Version header
# -------------------------------------------------------------------------
configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h)
configure_file(src/resource.rc.in ${CMAKE_BINARY_DIR}/generated/resource.rc)

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
        src/LogDialog.cpp
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
        REAPERAPI_WANT_plugin_register
        REAPERAPI_WANT_ShowConsoleMsg
        REAPERAPI_WANT_SetExtState
        REAPERAPI_WANT_GetExtState
        REAPERAPI_WANT_HasExtState
        REAPERAPI_WANT_GetMainHwnd
)

# -------------------------------------------------------------------------
# SWELL (macOS + Linux only)
# - Adds the swell/ directory to include paths so SWELL files can resolve
#   their own sibling headers with bare #include "swell.h" style includes.
# - Transpiles resource.rc → resource.rc_mac_dlg via swell_resgen.php so
#   resource_swell.cpp can #include the auto-generated file. resource.rc
#   remains the single source of truth for dialog layout on all platforms.
# -------------------------------------------------------------------------
if(NOT WIN32)
    target_include_directories(reaper_aaf PRIVATE
            ${REAPER_SDK_DIR}/WDL/swell
    )

    find_program(PHP_EXECUTABLE php REQUIRED)
    set(SWELL_RESGEN "${REAPER_SDK_DIR}/WDL/swell/swell_resgen.php")
    set(SWELL_RESGEN_OUT_DIR "${CMAKE_BINARY_DIR}/generated")
    add_custom_command(
            OUTPUT  "${SWELL_RESGEN_OUT_DIR}/resource.rc_mac_dlg"
                    "${SWELL_RESGEN_OUT_DIR}/resource.rc_mac_menu"
            COMMAND ${PHP_EXECUTABLE} "${SWELL_RESGEN}" --quiet resource.rc
            WORKING_DIRECTORY "${SWELL_RESGEN_OUT_DIR}"
            MAIN_DEPENDENCY "${CMAKE_BINARY_DIR}/generated/resource.rc"
            DEPENDS         "${SWELL_RESGEN}"
            COMMENT "Transpiling resource.rc → resource.rc_mac_dlg (SWELL)"
    )
    add_custom_target(swell_resources
            DEPENDS "${SWELL_RESGEN_OUT_DIR}/resource.rc_mac_dlg"
    )
    add_dependencies(reaper_aaf swell_resources)
endif()

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
    target_sources(reaper_aaf PRIVATE ${CMAKE_BINARY_DIR}/generated/resource.rc)
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
    target_link_libraries(reaper_aaf PRIVATE "-framework CoreFoundation" "-framework Cocoa")
    target_sources(reaper_aaf PRIVATE
            ${REAPER_SDK_DIR}/WDL/swell/swell-modstub.mm
            src/resource_swell.cpp
    )
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
    target_sources(reaper_aaf PRIVATE
            ${REAPER_SDK_DIR}/WDL/swell/swell-modstub-generic.cpp
            src/resource_swell.cpp
    )
endif()
