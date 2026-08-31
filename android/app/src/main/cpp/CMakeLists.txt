cmake_minimum_required(VERSION 3.22)

project(tcc_android_native LANGUAGES C CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "This CMake project must be configured with the Android NDK")
endif()

get_filename_component(TCC_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)

if(NOT DEFINED TCC_LIBOQS_ANDROID_ROOT)
    set(
        TCC_LIBOQS_ANDROID_ROOT
        "${TCC_PROJECT_ROOT}/third_party/install/liboqs-android"
        CACHE PATH
        "Directory containing one liboqs installation per Android ABI"
    )
endif()

set(
    TCC_LIBOQS_ROOT
    "${TCC_LIBOQS_ANDROID_ROOT}/${ANDROID_ABI}"
    CACHE PATH
    "liboqs installation for the selected Android ABI"
    FORCE
)

if(NOT DEFINED TCC_OPENSSL_ANDROID_ROOT)
    set(
        TCC_OPENSSL_ANDROID_ROOT
        "${TCC_PROJECT_ROOT}/third_party/install/openssl-android"
        CACHE PATH
        "Directory containing one OpenSSL installation per Android ABI"
    )
endif()

set(
    TCC_OPENSSL_ROOT
    "${TCC_OPENSSL_ANDROID_ROOT}/${ANDROID_ABI}"
    CACHE PATH
    "OpenSSL installation for the selected Android ABI"
    FORCE
)

list(APPEND CMAKE_MODULE_PATH "${TCC_PROJECT_ROOT}/cmake")
include(Dependencies)

add_subdirectory("${TCC_PROJECT_ROOT}/core" "${CMAKE_CURRENT_BINARY_DIR}/tcc-core")

add_library(
    tcc_android
    SHARED
    android_client.cpp
    android_log.cpp
    native_bridge.cpp
)

target_compile_features(tcc_android PRIVATE cxx_std_17)
target_include_directories(tcc_android PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(tcc_android PRIVATE tcc::core android log)
