add_library(tcc_server_lib STATIC
    src/config.cpp
    src/server.cpp
)

add_library(tcc::server ALIAS tcc_server_lib)

target_include_directories(tcc_server_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tcc_server_lib PUBLIC tcc::native)
target_compile_features(tcc_server_lib PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tcc_server_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(tcc_server main.cpp)
target_link_libraries(tcc_server PRIVATE tcc::server)
target_compile_features(tcc_server PRIVATE cxx_std_17)
