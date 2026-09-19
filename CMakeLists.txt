cmake_minimum_required(VERSION 3.25)

project(tcc_signatures LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TCC_BUILD_SERVER "Build the Linux server" ON)
option(TCC_BUILD_TESTS "Build native tests" ON)

add_subdirectory(native)

if(TCC_BUILD_SERVER AND NOT ANDROID)
    add_subdirectory(server)
endif()

if(TCC_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()
