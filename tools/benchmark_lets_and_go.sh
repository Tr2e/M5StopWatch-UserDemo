#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${1:-/tmp/lets-go-benchmark}"
base="$repo_dir/main/apps/app_lets_and_go_racer"
mkdir -p "$out_dir"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$repo_dir/tools/lets_and_go_host" \
    "$repo_dir/tools/lets_and_go_render_benchmark.cpp" \
    "$base/view/garage_renderer.cpp" "$base/view/race_renderer.cpp" \
    "$base/controller/game_flow.cpp" "$base/controller/garage_selection.cpp" \
    "$base/controller/results_selection.cpp" "$base/controller/race_controller.cpp" \
    "$base/model/car_catalog.cpp" "$base/model/car_display_mesh.cpp" "$base/model/overpass_track.cpp" \
    "$base/model/track_types.cpp" "$base/model/racer_model.cpp" "$base/model/rival_ai.cpp" \
    -o "$out_dir/benchmark"
"$out_dir/benchmark" | tee "$out_dir/results.txt"
