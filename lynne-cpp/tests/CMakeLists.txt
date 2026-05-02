set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)
set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/googletest gtest)

# googletest's internal_utils.cmake hardcodes ARCHIVE_OUTPUT → CMAKE_BINARY_DIR/lib
# Override so gtest/gmock libraries land in dist/lib/
set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${DIST_DIR}/lib"
)

set(GTEST_INCLUDE_DIR ${THIRD_PARTY_DIR}/googletest/googletest/include)

set(COMMON_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
    ${GTEST_INCLUDE_DIR}
)

function(add_lynne_test TARGET)
    add_executable(${TARGET} ${ARGN})
    target_include_directories(${TARGET} PRIVATE ${COMMON_TEST_INCLUDES})
    target_link_libraries(${TARGET} gtest gtest_main)
    # gtest_main provides main(); order: gtest then gtest_main
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endfunction()

function(add_lynne_ta TARGET)
    add_executable(${TARGET} ${ARGN})
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/dist/include
    )
    # standalone main() — no gtest dependency
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endfunction()

add_subdirectory(common)
add_subdirectory(wheel/logger)
add_subdirectory(wheel/config)
add_subdirectory(wheel/storage)
add_subdirectory(wheel/scheduler)
add_subdirectory(wheel/llm)
add_subdirectory(wheel/ws_client)
