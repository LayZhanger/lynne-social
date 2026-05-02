set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

add_library(lynne_llm STATIC
    llm_models.cpp
    llm_factory.cpp
    imp/deepseek_engine.cpp
)

target_include_directories(lynne_llm PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)

target_link_libraries(lynne_llm PUBLIC
    lynne_common
    lynne_scheduler
    ${DIST_DIR}/lib/libssl.a
    ${DIST_DIR}/lib/libcrypto.a
    pthread
    dl
    rt
)

target_compile_definitions(lynne_llm PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
