add_library(lynne_logger STATIC
    logger_models.cpp
    logger_factory.cpp
    imp/spdlog_logger.cpp
)

target_include_directories(lynne_logger PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)

target_link_libraries(lynne_logger PUBLIC lynne_common)
