cmake_minimum_required(VERSION 3.22)

project(
    tcc_signature_benchmark
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TCC_BUILD_SERVER "Build the Linux server" OFF)
option(TCC_BUILD_TESTS "Build the native tests" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)

add_subdirectory(core)

if(TCC_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(TCC_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
