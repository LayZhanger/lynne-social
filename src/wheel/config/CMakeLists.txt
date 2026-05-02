add_library(lynne_config STATIC
    config_models.cpp
    config_factory.cpp
    imp/json_config_loader.cpp
)

target_include_directories(lynne_config PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)
