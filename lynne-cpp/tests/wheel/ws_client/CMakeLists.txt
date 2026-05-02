add_lynne_test(test_ws_client_ut test_ws_client_ut.cpp)
target_link_libraries(test_ws_client_ut lynne_common lynne_ws_client)

add_lynne_ta(test_ws_client_ta test_ws_client_ta.cpp ws_echo_server.cpp)
target_link_libraries(test_ws_client_ta lynne_common lynne_ws_client ${DIST_DIR}/lib/libixwebsocket.a z pthread dl rt)
