set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

add_library(lynne_ws_client STATIC
    ws_client_models.cpp
    ws_client_factory.cpp
    imp/ix_ws_client.cpp
)

target_include_directories(lynne_ws_client PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)

target_link_libraries(lynne_ws_client PUBLIC
    lynne_common
    lynne_scheduler
    ${DIST_DIR}/lib/libixwebsocket.a
    ${DIST_DIR}/lib/libssl.a
    ${DIST_DIR}/lib/libcrypto.a
    z
    pthread
    dl
    rt
)
