# -------------------------------------------------------------------------
# Individual test executables
# -------------------------------------------------------------------------
add_catch2_test(test_logbuffer SOURCES
        test_logbuffer.cpp
        ${CMAKE_SOURCE_DIR}/src/LogBuffer.cpp
)

add_catch2_test(test_helpers SOURCES
        test_helpers.cpp
)
target_link_libraries(test_helpers PRIVATE aaf-static)
target_include_directories(test_helpers PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/LibAAF/include
        ${CMAKE_BINARY_DIR}/include
)

add_catch2_test(test_faderesolver SOURCES
        test_faderesolver.cpp
        ${CMAKE_SOURCE_DIR}/src/FadeResolver.cpp
)
target_link_libraries(test_faderesolver PRIVATE aaf-static)
target_include_directories(test_faderesolver PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/LibAAF/include
        ${CMAKE_BINARY_DIR}/include
)

add_catch2_test(test_rppwriter SOURCES
        test_rppwriter.cpp
        ${CMAKE_SOURCE_DIR}/src/RppWriter.cpp
)
target_include_directories(test_rppwriter PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/LibAAF/include
        ${CMAKE_BINARY_DIR}/include
)

add_catch2_test(test_aafimporter SOURCES
        test_aafimporter.cpp
        helpers_stubs.cpp
        ${CMAKE_SOURCE_DIR}/src/AafImporter.cpp
        ${CMAKE_SOURCE_DIR}/src/RppWriter.cpp
        ${CMAKE_SOURCE_DIR}/src/FadeResolver.cpp
        ${CMAKE_SOURCE_DIR}/src/LogBuffer.cpp
)
target_link_libraries(test_aafimporter PRIVATE aaf-static)
target_include_directories(test_aafimporter PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/LibAAF/include
        ${CMAKE_BINARY_DIR}/include
)
target_compile_definitions(test_aafimporter PRIVATE
        AAF_TEST_DIR="${CMAKE_SOURCE_DIR}/extern/LibAAF/test/aaf"
        GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden"
)

add_catch2_test(test_aafimporter_mr SOURCES
        test_aafimporter_mr.cpp
        helpers_stubs.cpp
        ${CMAKE_SOURCE_DIR}/src/AafImporter.cpp
        ${CMAKE_SOURCE_DIR}/src/RppWriter.cpp
        ${CMAKE_SOURCE_DIR}/src/FadeResolver.cpp
        ${CMAKE_SOURCE_DIR}/src/LogBuffer.cpp
)
target_link_libraries(test_aafimporter_mr PRIVATE aaf-static)
target_include_directories(test_aafimporter_mr PRIVATE
        ${CMAKE_SOURCE_DIR}/extern/LibAAF/include
        ${CMAKE_BINARY_DIR}/include
)
target_compile_definitions(test_aafimporter_mr PRIVATE
        AAF_TEST_DIR="${CMAKE_SOURCE_DIR}/extern/LibAAF/test/aaf"
        GOLDEN_AAFTOOL_DIR="${CMAKE_SOURCE_DIR}/tests/golden-aaftool"
)

# -------------------------------------------------------------------------
# Meta-target to build all tests at once
# -------------------------------------------------------------------------
add_custom_target(all_tests
        DEPENDS
        test_logbuffer
        test_helpers
        test_faderesolver
        test_rppwriter
        test_aafimporter
        test_aafimporter_mr
)

# -------------------------------------------------------------------------
# Enable CTest integration for easier running
# -------------------------------------------------------------------------
enable_testing()
add_test(NAME test_logbuffer COMMAND test_logbuffer)
add_test(NAME test_helpers COMMAND test_helpers)
add_test(NAME test_faderesolver COMMAND test_faderesolver)
add_test(NAME test_rppwriter COMMAND test_rppwriter)
add_test(NAME test_aafimporter COMMAND test_aafimporter)
add_test(NAME test_aafimporter_mr COMMAND test_aafimporter_mr)