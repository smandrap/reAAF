cmake_minimum_required(VERSION 3.15)
project(reaper_aaf VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------------
# Paths — adjust REAPER_SDK_DIR if you placed the SDK elsewhere
# -------------------------------------------------------------------------
set(REAPER_SDK_DIR "${CMAKE_SOURCE_DIR}/extern/reaper-sdk" CACHE PATH "Path to justinfrankel/reaper-sdk checkout")
set(LIBAAF_DIR     "${CMAKE_SOURCE_DIR}/extern/LibAAF"     CACHE PATH "Path to agfline/LibAAF checkout")

# -------------------------------------------------------------------------
# LibAAF — compile its C sources directly into our plugin so we ship one DLL
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
        ${LIBAAF_SOURCES}
        src/helpers.cpp
        src/RppWriter.cpp
        src/AafImporter.cpp
        src/AafImporter.h
        src/FadeResolver.cpp
        src/FadeResolver.h
        src/defines.h
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
        ${REAPER_SDK_DIR}/sdk
        ${REAPER_SDK_DIR}/WDL
        ${LIBAAF_DIR}/include
)

# -------------------------------------------------------------------------
# Only expose the API functions we actually need (keeps the DLL lean)
# -------------------------------------------------------------------------
# We write RPP text via ProjectStateContext::AddLine — we never call the
# track/item/take construction APIs. The only REAPER API function we call
# directly is ShowConsoleMsg for diagnostics.
target_compile_definitions(reaper_aaf PRIVATE
        REAPERAPI_MINIMAL
        REAPERAPI_WANT_ShowConsoleMsg
)

# -------------------------------------------------------------------------
# Platform specifics
# -------------------------------------------------------------------------
if(WIN32)
    # REAPER requires MSVC-compatible ABI
    set_target_properties(reaper_aaf PROPERTIES
            PREFIX ""
            SUFFIX ".dll"
    )
    target_compile_definitions(reaper_aaf PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
    # Auto-copy to REAPER UserPlugins after build (optional)
    set(REAPER_PLUGINS_DIR "$ENV{APPDATA}/REAPER/UserPlugins")
    add_custom_command(TARGET reaper_aaf POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:reaper_aaf>
            "${REAPER_PLUGINS_DIR}/reaper_reAAF.dll"
            COMMENT "Copying to REAPER UserPlugins..."
    )

elseif(APPLE)
    # SWELL is bundled with REAPER on macOS — tell WDL not to build it
    target_compile_definitions(reaper_aaf PRIVATE SWELL_PROVIDED_BY_APP)
    set_target_properties(reaper_aaf PROPERTIES
            PREFIX ""
            SUFFIX ".dylib"
            OSX_ARCHITECTURES "arm64;x86_64"   # universal binary
    )
    target_link_libraries(reaper_aaf PRIVATE "-framework CoreFoundation")
    # Auto-copy to REAPER UserPlugins after build (optional)
    set(REAPER_PLUGINS_DIR "$ENV{HOME}/Library/Application Support/REAPER/UserPlugins")
    add_custom_command(TARGET reaper_aaf POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:reaper_aaf>
            "${REAPER_PLUGINS_DIR}/reaper_reAAF.dylib"
            COMMENT "Copying to REAPER UserPlugins..."
    )

else()  # Linux
    target_compile_definitions(reaper_aaf PRIVATE SWELL_PROVIDED_BY_APP)
    set_target_properties(reaper_aaf PROPERTIES
            PREFIX ""
            SUFFIX ".so"
    )
    target_link_libraries(reaper_aaf PRIVATE dl)
endif()