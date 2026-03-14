cmake_minimum_required(VERSION 3.15)
project(reAAF VERSION 0.1.0 LANGUAGES C CXX)


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
# LibAAF sources
# -------------------------------------------------------------------------
set(LIBAAF_SOURCES
        ${LIBAAF_DIR}/src/LibCFB/LibCFB.c
        ${LIBAAF_DIR}/src/LibCFB/CFBDump.c
        ${LIBAAF_DIR}/src/AAFCore/AAFCore.c
        ${LIBAAF_DIR}/src/AAFCore/AAFClass.c
        ${LIBAAF_DIR}/src/AAFCore/AAFToText.c
        ${LIBAAF_DIR}/src/AAFCore/AAFDump.c
        ${LIBAAF_DIR}/src/AAFIface/AAFIface.c
        ${LIBAAF_DIR}/src/AAFIface/AAFIParser.c
        ${LIBAAF_DIR}/src/AAFIface/AAFIEssenceFile.c
        ${LIBAAF_DIR}/src/AAFIface/RIFFParser.c
        ${LIBAAF_DIR}/src/AAFIface/URIParser.c
        ${LIBAAF_DIR}/src/AAFIface/ProTools.c
        ${LIBAAF_DIR}/src/AAFIface/MediaComposer.c
        ${LIBAAF_DIR}/src/AAFIface/Resolve.c
        ${LIBAAF_DIR}/src/common/utils.c
        ${LIBAAF_DIR}/src/common/log.c
)

# -------------------------------------------------------------------------
# Plugin sources
# -------------------------------------------------------------------------
add_library(reaper_aaf MODULE
        src/main.cpp
        src/aaf_import.cpp
        src/helpers.cpp
        src/RppWriter.cpp
        src/AafImporter.cpp
        src/FadeResolver.cpp
        ${LIBAAF_SOURCES}
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
        ${REAPER_SDK_DIR}/sdk
        ${REAPER_SDK_DIR}/WDL
        ${LIBAAF_DIR}/include
)

# -------------------------------------------------------------------------
# REAPER API — only what we actually call
# -------------------------------------------------------------------------
target_compile_definitions(reaper_aaf PRIVATE
        REAPERAPI_MINIMAL
        REAPERAPI_WANT_ShowConsoleMsg
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
    set(REAPER_PLUGINS_DIR "$ENV{HOME}/Library/Application Support/REAPER/UserPlugins")
    add_custom_command(TARGET reaper_aaf POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:reaper_aaf>
            "${REAPER_PLUGINS_DIR}/reaper_reAAF.dylib"
            COMMENT "Copying to REAPER UserPlugins..."
    )

else()  # Linux
    set_target_properties(reaper_aaf PROPERTIES SUFFIX ".so")
    target_compile_definitions(reaper_aaf PRIVATE SWELL_PROVIDED_BY_APP)
    target_link_libraries(reaper_aaf PRIVATE dl)
endif()