#!/usr/bin/env bash

set -euo pipefail

readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly project_root="$(cd -- "${script_dir}/.." && pwd)"
readonly source_dir="${project_root}/third_party/openssl"
readonly dependencies_dir="${TCC_DEPENDENCIES_DIR:-${project_root}/build/dependencies}"
readonly minimum_android_api=29
readonly target="${1:-}"
readonly android_abi="${2:-arm64-v8a}"
readonly parallel_jobs="${TCC_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"

usage() {
    echo "Usage: $0 <linux|android> [android-abi]" >&2
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required command not found: $1" >&2
        exit 1
    fi
}

find_android_ndk() {
    local candidate="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"

    if [[ -z "${candidate}" && -n "${ANDROID_HOME:-}" ]]; then
        candidate="${ANDROID_HOME}/ndk/30.0.15729638"
    fi

    if [[ ! -d "${candidate}/toolchains/llvm/prebuilt/linux-x86_64/bin" ]]; then
        echo "Android NDK not found. Set ANDROID_NDK_HOME or ANDROID_NDK_ROOT." >&2
        exit 1
    fi

    echo "${candidate}"
}

if [[ ! -x "${source_dir}/Configure" ]]; then
    echo "OpenSSL sources not found. Initialize the third_party/openssl submodule." >&2
    exit 1
fi

require_command make
require_command perl

configure_target=""
build_dir=""
install_dir=""
extra_args=()

case "${target}" in
    linux)
        require_command clang
        configure_target="linux-x86_64"
        build_dir="${dependencies_dir}/work/openssl-linux"
        install_dir="${dependencies_dir}/linux/openssl"
        export CC="${TCC_LINUX_CC:-clang}"
        ;;
    android)
        readonly android_ndk="$(find_android_ndk)"

        case "${android_abi}" in
            arm64-v8a)
                configure_target="android-arm64"
                ;;
            x86_64)
                configure_target="android-x86_64"
                ;;
            *)
                echo "Unsupported Android ABI: ${android_abi}" >&2
                echo "Supported ABIs: arm64-v8a, x86_64" >&2
                exit 2
                ;;
        esac

        build_dir="${dependencies_dir}/work/openssl-android-${android_abi}"
        install_dir="${dependencies_dir}/android/${android_abi}/openssl"
        export ANDROID_NDK_ROOT="${android_ndk}"
        export PATH="${android_ndk}/toolchains/llvm/prebuilt/linux-x86_64/bin:${PATH}"
        extra_args+=("-D__ANDROID_API__=${minimum_android_api}")
        ;;
    *)
        usage
        exit 2
        ;;
esac

mkdir -p "${build_dir}" "${install_dir}"

(
    cd "${build_dir}"
    "${source_dir}/Configure" \
        "${configure_target}" \
        no-shared \
        no-module \
        no-tests \
        no-apps \
        no-docs \
        --libdir=lib \
        "--prefix=${install_dir}" \
        "--openssldir=${install_dir}/ssl" \
        "${extra_args[@]}"
    make -s -j"${parallel_jobs}"
    make -s install_sw
)

if [[ ! -f "${install_dir}/lib/libcrypto.a" ]]; then
    echo "OpenSSL build finished without the expected lib/libcrypto.a artifact." >&2
    exit 1
fi

echo "OpenSSL installed in ${install_dir}"
