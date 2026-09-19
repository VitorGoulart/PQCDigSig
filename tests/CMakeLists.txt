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
