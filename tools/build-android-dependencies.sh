#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"

abi="${1:-arm64-v8a}"

"${script_dir}/build-liboqs.sh" android "${abi}"
"${script_dir}/build-openssl.sh" android "${abi}"
