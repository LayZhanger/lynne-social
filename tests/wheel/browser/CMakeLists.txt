set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)

add_lynne_test(test_browser_models_ut test_browser_models_ut.cpp)
target_link_libraries(test_browser_models_ut lynne_browser)

add_lynne_ta(test_browser_ta test_browser_ta.cpp)
target_link_libraries(test_browser_ta lynne_browser lynne_logger ${DIST_DIR}/lib/libixwebsocket.a)
