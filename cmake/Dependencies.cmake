include_guard(GLOBAL)

set(
    TCC_LIBOQS_ROOT
    ""
    CACHE PATH
    "Installation prefix containing liboqs for the current target"
)

if(NOT TCC_LIBOQS_ROOT)
    if(ANDROID)
        message(FATAL_ERROR
            "TCC_LIBOQS_ROOT is required for Android and must point to "
            "third_party/install/liboqs-android/<ABI>"
        )
    endif()

    set(
        TCC_LIBOQS_ROOT
        "${PROJECT_SOURCE_DIR}/third_party/install/liboqs-linux"
        CACHE PATH
        "Installation prefix containing liboqs for the current target"
        FORCE
    )
endif()

if(NOT EXISTS "${TCC_LIBOQS_ROOT}")
    message(FATAL_ERROR
        "liboqs was not found at '${TCC_LIBOQS_ROOT}'. "
        "Run tools/build-liboqs.sh linux or provide -DTCC_LIBOQS_ROOT=<prefix>."
    )
endif()

find_package(
    liboqs 0.16.0
    CONFIG
    REQUIRED
    PATHS "${TCC_LIBOQS_ROOT}/lib/cmake/liboqs"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT TARGET OQS::oqs)
    message(FATAL_ERROR "The liboqs package did not export the OQS::oqs target")
endif()

set(
    TCC_OPENSSL_ROOT
    ""
    CACHE PATH
    "Installation prefix containing OpenSSL for the current target"
)

if(NOT TCC_OPENSSL_ROOT)
    if(ANDROID)
        message(FATAL_ERROR
            "TCC_OPENSSL_ROOT is required for Android and must point to "
            "third_party/install/openssl-android/<ABI>"
        )
    endif()

    set(
        TCC_OPENSSL_ROOT
        "${PROJECT_SOURCE_DIR}/third_party/install/openssl-linux"
        CACHE PATH
        "Installation prefix containing OpenSSL for the current target"
        FORCE
    )
endif()

if(NOT EXISTS "${TCC_OPENSSL_ROOT}")
    message(FATAL_ERROR
        "OpenSSL was not found at '${TCC_OPENSSL_ROOT}'. "
        "Run tools/build-openssl.sh linux or provide -DTCC_OPENSSL_ROOT=<prefix>."
    )
endif()

find_package(
    OpenSSL 3.5
    CONFIG
    REQUIRED
    COMPONENTS Crypto
    PATHS
        "${TCC_OPENSSL_ROOT}/lib/cmake/OpenSSL"
        "${TCC_OPENSSL_ROOT}/lib64/cmake/OpenSSL"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT TARGET OpenSSL::Crypto)
    message(FATAL_ERROR "OpenSSL did not provide the OpenSSL::Crypto target")
endif()
