add_library(lynne_storage STATIC
    storage_models.cpp
    storage_factory.cpp
    imp/jsonl_storage.cpp
)

target_include_directories(lynne_storage PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)

target_link_libraries(lynne_storage PUBLIC lynne_common)
