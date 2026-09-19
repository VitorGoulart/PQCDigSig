#!/usr/bin/env bash

set -euo pipefail

readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly project_root="$(cd -- "${script_dir}/.." && pwd)"
readonly source_dir="${project_root}/third_party/liboqs"
readonly dependencies_dir="${TCC_DEPENDENCIES_DIR:-${project_root}/build/dependencies}"
readonly minimum_android_api=29
readonly target="${1:-}"
readonly android_abi="${2:-arm64-v8a}"
readonly algorithms="SIG_falcon_512;SIG_falcon_1024"

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

    if [[ ! -f "${candidate}/build/cmake/android.toolchain.cmake" ]]; then
        echo "Android NDK not found. Set ANDROID_NDK_HOME or ANDROID_NDK_ROOT." >&2
        exit 1
    fi

    echo "${candidate}"
}

if [[ ! -f "${source_dir}/CMakeLists.txt" ]]; then
    echo "liboqs sources not found. Initialize the third_party/liboqs submodule." >&2
    exit 1
fi

require_command cmake
require_command ninja

build_dir=""
install_dir=""
platform_args=()

case "${target}" in
    linux)
        require_command clang
        build_dir="${dependencies_dir}/work/liboqs-linux"
        install_dir="${dependencies_dir}/linux/liboqs"
        platform_args+=("-DCMAKE_C_COMPILER=${TCC_LINUX_CC:-clang}")
        ;;
    android)
        readonly android_ndk="$(find_android_ndk)"

        case "${android_abi}" in
            arm64-v8a|x86_64)
                ;;
            *)
                echo "Unsupported Android ABI: ${android_abi}" >&2
                echo "Supported ABIs: arm64-v8a, x86_64" >&2
                exit 2
                ;;
        esac

        build_dir="${dependencies_dir}/work/liboqs-android-${android_abi}"
        install_dir="${dependencies_dir}/android/${android_abi}/liboqs"
        platform_args+=(
            "-DCMAKE_TOOLCHAIN_FILE=${android_ndk}/build/cmake/android.toolchain.cmake"
            "-DANDROID_ABI=${android_abi}"
            "-DANDROID_PLATFORM=android-${minimum_android_api}"
        )
        ;;
    *)
        usage
        exit 2
        ;;
esac

cmake -S "${source_dir}" -B "${build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${install_dir}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_SHARED_LIBS=OFF \
    -DOQS_BUILD_ONLY_LIB=ON \
    -DOQS_USE_OPENSSL=OFF \
    "-DOQS_MINIMAL_BUILD=${algorithms}" \
    "${platform_args[@]}"

cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}"

if [[ ! -f "${install_dir}/lib/liboqs.a" ]]; then
    echo "liboqs build finished without the expected lib/liboqs.a artifact." >&2
    exit 1
fi

echo "liboqs installed in ${install_dir}"
