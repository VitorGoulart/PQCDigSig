add_executable(payload_test payload_test.cpp)
target_link_libraries(payload_test PRIVATE tcc::native)
add_test(NAME payload_test COMMAND payload_test)

add_executable(crypto_structures_test crypto_structures_test.cpp)
target_link_libraries(crypto_structures_test PRIVATE tcc::native)
add_test(NAME crypto_structures_test COMMAND crypto_structures_test)

add_executable(crypto_adapters_test crypto_adapters_test.cpp)
target_link_libraries(crypto_adapters_test PRIVATE tcc::native)
add_test(NAME crypto_adapters_test COMMAND crypto_adapters_test)

add_executable(measurement_test measurement_test.cpp)
target_link_libraries(measurement_test PRIVATE tcc::native)
add_test(NAME measurement_test COMMAND measurement_test)

add_executable(transport_test transport_test.cpp)
target_link_libraries(transport_test PRIVATE tcc::native)
add_test(NAME transport_test COMMAND transport_test)

add_executable(csv_test csv_test.cpp)
target_link_libraries(csv_test PRIVATE tcc::native)
add_test(NAME csv_test COMMAND csv_test)

if(TARGET tcc_server_lib)
    add_executable(client_test client_test.cpp)
    target_link_libraries(client_test PRIVATE tcc::native tcc::server)
    add_test(NAME client_test COMMAND client_test)
    set_tests_properties(client_test PROPERTIES TIMEOUT 120 RESOURCE_LOCK tcc_tcp_port)

    add_executable(server_config_test server_config_test.cpp)
    target_link_libraries(server_config_test PRIVATE tcc::server)
    add_test(NAME server_config_test COMMAND server_config_test)
endif()
