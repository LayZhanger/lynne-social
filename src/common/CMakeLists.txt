add_library(lynne_common STATIC models.cpp)

target_include_directories(lynne_common PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dist/include
)
