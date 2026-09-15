#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${1:-/tmp/gundam-museum}"
mkdir -p "$out_dir"
flags=(-std=c++17 -O2 -g -Wall -Wextra -Werror -pedantic)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
font_inc="${M5GFX_PARENT_INCLUDE:-$repo_dir/M5StopWatch-UserDemo-ruview/components/M5GFX/src}"
"${CXX:-c++}" "${flags[@]}" -I"$repo_dir/tools/lets_and_go_host" -I"$font_inc" \
  "$repo_dir/tools/gundam_museum_test.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/rx78.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/char_zaku.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/nu_gundam.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/sazabi.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/strike_gundam.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/model/destiny_gundam.cpp" \
  "$repo_dir/main/apps/app_gundam_museum/view/museum_renderer.cpp" \
  -o "$out_dir/test"
"$out_dir/test" "$out_dir" "${2:-rx78}" | tee "$out_dir/results.txt"
