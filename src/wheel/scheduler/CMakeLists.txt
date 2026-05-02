set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

add_library(lynne_scheduler STATIC
    scheduler_models.cpp
    scheduler_factory.cpp
    imp/uv_scheduler.cpp
)

target_include_directories(lynne_scheduler PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)

target_link_libraries(lynne_scheduler PUBLIC
    lynne_common
    ${DIST_DIR}/lib/libuv.a
    pthread
    dl
    rt
)
