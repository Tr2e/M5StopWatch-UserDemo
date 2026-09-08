#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${1:-/tmp/lets-go-frames}"
base="$repo_dir/main/apps/app_lets_and_go_racer"
mkdir -p "$out_dir"
flags=(-std=c++17 -O1 -g -Wall -Wextra -Werror -pedantic)
if [[ "${SANITIZE:-0}" == "1" ]]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CXX:-c++}" "${flags[@]}" -I"$repo_dir/tools/lets_and_go_host" \
    "$repo_dir/tools/lets_and_go_renderer_test.cpp" \
    "$base/view/garage_renderer.cpp" "$base/view/race_renderer.cpp" \
    "$base/controller/game_flow.cpp" "$base/controller/garage_selection.cpp" \
    "$base/controller/results_selection.cpp" "$base/controller/race_controller.cpp" \
    "$base/model/car_catalog.cpp" "$base/model/car_display_mesh.cpp" "$base/model/overpass_track.cpp" \
    "$base/model/track_types.cpp" "$base/model/racer_model.cpp" "$base/model/rival_ai.cpp" \
    -o "$out_dir/render-test"
"$out_dir/render-test" "$out_dir"
