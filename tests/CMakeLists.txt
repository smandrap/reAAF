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
