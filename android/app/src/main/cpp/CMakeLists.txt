cmake_minimum_required(VERSION 3.25)

project(tcc_android LANGUAGES CXX)

get_filename_component(TCC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)

add_subdirectory(
    "${TCC_ROOT}/native"
    "${CMAKE_BINARY_DIR}/tcc-native"
)

add_library(tcc_android SHARED native_bridge.cpp)

find_library(android_log log REQUIRED)

target_link_libraries(tcc_android
    PRIVATE
        tcc::native
        ${android_log}
)

target_compile_features(tcc_android PRIVATE cxx_std_17)
