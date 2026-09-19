add_executable(tcc_server main.cpp)
target_link_libraries(tcc_server PRIVATE tcc::native)
target_compile_features(tcc_server PRIVATE cxx_std_17)
