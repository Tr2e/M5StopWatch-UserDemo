#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${1:-/tmp/gundam-reference}"
mkdir -p "$out_dir"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -pedantic \
  -I"$repo_dir/tools/lets_and_go_host" \
  -I"${M5GFX_PARENT_INCLUDE:-$repo_dir/M5StopWatch-UserDemo-ruview/components/M5GFX/src}" \
  "$repo_dir/tools/gundam_reference_render.cpp" "$repo_dir/main/apps/app_gundam_museum/model/rx78.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/nu_gundam.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/strike_gundam.cpp" \
  -o "$out_dir/render"
"$out_dir/render" "$out_dir" "${2:-rx78}" "${3:-final}" | tee "$out_dir/cameras.txt"
