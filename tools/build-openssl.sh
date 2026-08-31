#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"
openssl_source="${project_root}/third_party/openssl"
target="${1:-}"
abi="${2:-arm64-v8a}"

if [[ ! -f "${openssl_source}/Configure" ]]; then
    echo "OpenSSL is unavailable. Run: git submodule update --init --recursive" >&2
    exit 1
fi

build_openssl() {
    local configure_target="$1"
    local build_dir="$2"
    local install_dir="$3"
    shift 3

    mkdir -p "${build_dir}" "${install_dir}"

    (
        cd "${build_dir}"
        "${openssl_source}/Configure" \
            "${configure_target}" \
            no-shared \
            no-tests \
            no-apps \
            no-docs \
            "--prefix=${install_dir}" \
            "--openssldir=${install_dir}/ssl" \
            "$@"
        make -j"$(getconf _NPROCESSORS_ONLN)"
        make install_sw
    )
}

case "${target}" in
    linux)
        export CC=clang
        install_result="${project_root}/third_party/install/openssl-linux"
        build_openssl \
            linux-x86_64 \
            "${project_root}/third_party/build/openssl-linux" \
            "${install_result}"
        ;;
    android)
        android_sdk="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
        android_ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"

        if [[ ! -f "${android_ndk}/build/cmake/android.toolchain.cmake" && -n "${android_sdk}" ]]; then
            latest_ndk="$(find "${android_sdk}/ndk" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | sort -V | tail -n 1)"
            if [[ -n "${latest_ndk}" ]]; then
                android_ndk="${android_sdk}/ndk/${latest_ndk}"
            fi
        fi

        if [[ ! -f "${android_ndk}/build/cmake/android.toolchain.cmake" ]]; then
            echo "Android NDK not found. Set ANDROID_NDK_HOME or ANDROID_NDK_ROOT." >&2
            exit 1
        fi

        case "${abi}" in
            arm64-v8a)
                configure_target="android-arm64"
                ;;
            *)
                echo "Unsupported Android ABI: ${abi}. Supported ABI: arm64-v8a." >&2
                exit 2
                ;;
        esac

        host_tag="linux-x86_64"
        toolchain_bin="${android_ndk}/toolchains/llvm/prebuilt/${host_tag}/bin"

        if [[ ! -d "${toolchain_bin}" ]]; then
            echo "Android NDK toolchain not found at ${toolchain_bin}." >&2
            exit 1
        fi

        export ANDROID_NDK_ROOT="${android_ndk}"
        export PATH="${toolchain_bin}:${PATH}"

        install_result="${project_root}/third_party/install/openssl-android/${abi}"
        build_openssl \
            "${configure_target}" \
            "${project_root}/third_party/build/openssl-android-${abi}" \
            "${install_result}" \
            -D__ANDROID_API__=26
        ;;
    *)
        echo "Usage: $0 <linux|android> [android-abi]" >&2
        exit 2
        ;;
esac

echo "OpenSSL installed in ${install_result}"
