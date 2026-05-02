set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

add_library(lynne_browser STATIC
    browser_models.cpp
    browser_factory.cpp
    imp/cdp_browser_manager.cpp
)

target_include_directories(lynne_browser PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)

target_link_libraries(lynne_browser PUBLIC
    lynne_common
    lynne_scheduler
    lynne_ws_client
    ${DIST_DIR}/lib/libixwebsocket.a
    ${DIST_DIR}/lib/libssl.a
    ${DIST_DIR}/lib/libcrypto.a
    z
    pthread
    dl
    rt
)
