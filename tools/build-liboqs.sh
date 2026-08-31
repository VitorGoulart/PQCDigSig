#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"
liboqs_source="${project_root}/third_party/liboqs"
target="${1:-}"
abi="${2:-arm64-v8a}"
algorithms="SIG_falcon_512;SIG_falcon_1024"

if [[ ! -f "${liboqs_source}/CMakeLists.txt" ]]; then
    echo "liboqs is unavailable. Run: git submodule update --init --recursive" >&2
    exit 1
fi

common_args=(
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF
    -DOQS_BUILD_ONLY_LIB=ON
    -DOQS_USE_OPENSSL=OFF
    "-DOQS_MINIMAL_BUILD=${algorithms}"
)

case "${target}" in
    linux)
        build_dir="${project_root}/third_party/build/liboqs-linux"
        install_dir="${project_root}/third_party/install/liboqs-linux"

        cmake -S "${liboqs_source}" -B "${build_dir}" \
            "${common_args[@]}" \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_INSTALL_PREFIX="${install_dir}"
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

        build_dir="${project_root}/third_party/build/liboqs-android-${abi}"
        install_dir="${project_root}/third_party/install/liboqs-android/${abi}"

        cmake -S "${liboqs_source}" -B "${build_dir}" \
            "${common_args[@]}" \
            -DCMAKE_TOOLCHAIN_FILE="${android_ndk}/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI="${abi}" \
            -DANDROID_PLATFORM=android-26 \
            -DCMAKE_INSTALL_PREFIX="${install_dir}"
        ;;
    *)
        echo "Usage: $0 <linux|android> [android-abi]" >&2
        exit 2
        ;;
esac

cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}"

echo "liboqs installed in ${install_dir}"
