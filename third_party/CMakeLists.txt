cmake_minimum_required(VERSION 3.16)
project(lynne-deps VERSION 0.1.0)

# Output directory
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/../dist)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_INSTALL_PREFIX}/bin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Third party source dir (this directory)
set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# === libuv ===
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS_DIR}/libuv)
# Install libuv headers
install(TARGETS uv_a ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(FILES ${DEPS_DIR}/libuv/include/uv.h DESTINATION include)
install(DIRECTORY ${DEPS_DIR}/libuv/include/uv DESTINATION include/uv)

# === spdlog (header-only, just copy headers) ===
install(DIRECTORY ${DEPS_DIR}/spdlog/include/spdlog DESTINATION include)

# === nlohmann/json (header-only) ===
install(FILES ${DEPS_DIR}/nlohmann/json.hpp DESTINATION include)

# === cpp-httplib (header-only) ===
install(FILES ${DEPS_DIR}/cpp-httplib/httplib.h DESTINATION include)

# === IXWebSocket ===
# Optional build - can be disabled if not needed
option(BUILD_IXWEBSOCKET "Build IXWebSocket" ON)
if(BUILD_IXWEBSOCKET)
    add_subdirectory(${DEPS_DIR}/IXWebSocket)
    # IXWebSocket installs itself
endif()

# === Google Test (optional, for testing) ===
option(BUILD_GTEST "Build Google Test" OFF)
if(BUILD_GTEST)
    add_subdirectory(${DEPS_DIR}/googletest)
endif()

message(STATUS "Lynne dependencies will be installed to: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  include -> ${CMAKE_INSTALL_PREFIX}/include")
message(STATUS "  lib    -> ${CMAKE_INSTALL_PREFIX}/lib")
message(STATUS "  bin    -> ${CMAKE_INSTALL_PREFIX}/bin")