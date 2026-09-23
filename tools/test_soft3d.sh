#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${1:-/tmp/soft3d-host}"
mkdir -p "$out_dir"
flags=(-std=c++17 -O2 -g -Wall -Wextra -Werror -pedantic)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
"${CXX:-c++}" "${flags[@]}" -I"$repo_dir/tools/lets_and_go_host" \
  "$repo_dir/tools/soft3d_model_asset_test.cpp" -o "$out_dir/model_asset_test"
"$out_dir/model_asset_test"
