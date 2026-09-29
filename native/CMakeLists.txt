set(
    TCC_DEPENDENCIES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/dependencies"
    CACHE PATH
    "Directory containing the OpenSSL and liboqs installations"
)

if(ANDROID)
    set(_tcc_dependency_platform "android/${ANDROID_ABI}")
else()
    set(_tcc_dependency_platform "linux")
endif()

set(
    TCC_OPENSSL_ROOT
    "${TCC_DEPENDENCIES_DIR}/${_tcc_dependency_platform}/openssl"
    CACHE PATH
    "OpenSSL installation prefix for the current target"
)
set(
    TCC_LIBOQS_ROOT
    "${TCC_DEPENDENCIES_DIR}/${_tcc_dependency_platform}/liboqs"
    CACHE PATH
    "liboqs installation prefix for the current target"
)

if(NOT EXISTS "${TCC_OPENSSL_ROOT}/lib/libcrypto.a")
    message(FATAL_ERROR
        "OpenSSL was not found at '${TCC_OPENSSL_ROOT}'. "
        "Run tools/build-openssl.sh for the current target."
    )
endif()

if(NOT EXISTS "${TCC_LIBOQS_ROOT}/lib/liboqs.a")
    message(FATAL_ERROR
        "liboqs was not found at '${TCC_LIBOQS_ROOT}'. "
        "Run tools/build-liboqs.sh for the current target."
    )
endif()

file(
    STRINGS
    "${TCC_OPENSSL_ROOT}/include/openssl/opensslv.h"
    _tcc_openssl_version_line
    REGEX "^#[ \t]*define[ \t]+OPENSSL_VERSION_STR[ \t]+\"3\\.5\\.7\""
)
if(NOT _tcc_openssl_version_line)
    message(FATAL_ERROR
        "The OpenSSL installation at '${TCC_OPENSSL_ROOT}' is not version 3.5.7."
    )
endif()

add_library(tcc_openssl_crypto STATIC IMPORTED GLOBAL)
set_target_properties(tcc_openssl_crypto PROPERTIES
    IMPORTED_LOCATION "${TCC_OPENSSL_ROOT}/lib/libcrypto.a"
    INTERFACE_INCLUDE_DIRECTORIES "${TCC_OPENSSL_ROOT}/include"
)
add_library(OpenSSL::Crypto ALIAS tcc_openssl_crypto)

set(_tcc_openssl_link_libraries "${CMAKE_DL_LIBS}")
if(NOT ANDROID)
    find_package(Threads REQUIRED)
    list(APPEND _tcc_openssl_link_libraries Threads::Threads)
endif()
set_property(
    TARGET tcc_openssl_crypto
    PROPERTY INTERFACE_LINK_LIBRARIES "${_tcc_openssl_link_libraries}"
)

find_package(
    liboqs 0.16
    CONFIG
    REQUIRED
    PATHS "${TCC_LIBOQS_ROOT}/lib/cmake/liboqs"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

add_library(tcc_native STATIC
    src/client.cpp
    src/csv.cpp
    src/crypto.cpp
    src/crypto/ed25519.cpp
    src/crypto/factory.cpp
    src/crypto/falcon.cpp
    src/crypto/ml_dsa.cpp
    src/crypto/openssl_helpers.cpp
    src/crypto/rsa_pss.cpp
    src/measurement.cpp
    src/payload.cpp
    src/transport.cpp
)

add_library(tcc::native ALIAS tcc_native)

target_include_directories(tcc_native
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tcc_native PUBLIC cxx_std_17)

target_link_libraries(tcc_native
    PRIVATE
        OpenSSL::Crypto
        OQS::oqs
)

set_target_properties(tcc_native PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tcc_native PRIVATE -Wall -Wextra -Wpedantic)
endif()
