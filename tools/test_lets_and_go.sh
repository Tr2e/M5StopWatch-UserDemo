#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
out_dir="${TMPDIR:-/tmp}/lets-and-go-host-tests"
cxx="${CXX:-c++}"
base="$repo_dir/main/apps/app_lets_and_go_racer"

mkdir -p "$out_dir"

flags=(-std=c++17 -Wall -Wextra -Werror -pedantic)
if [[ "${SANITIZE:-0}" == "1" ]]; then
    flags+=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
fi

compile_run() {
    local name="$1"
    shift
    echo "[host-test] $name"
    "$cxx" "${flags[@]}" "$@" -o "$out_dir/$name"
    "$out_dir/$name"
}

compile_run game_flow \
    "$repo_dir/tools/lets_and_go_game_flow_test.cpp" \
    "$base/controller/game_flow.cpp"

compile_run chip_audio \
    "$repo_dir/tools/lets_and_go_audio_test.cpp" \
    "$base/audio/chip_synth.cpp"

compile_run audio_lifecycle -pthread -I"$repo_dir/tools/lets_and_go_audio_host" \
    "$repo_dir/tools/lets_and_go_audio_lifecycle_test.cpp" \
    "$base/audio/racer_audio.cpp" "$base/audio/chip_synth.cpp"

compile_run car_geometry \
    "$repo_dir/tools/lets_and_go_car_geometry_test.cpp" \
    "$base/model/car_catalog.cpp" "$base/model/car_display_mesh.cpp"

compile_run mesh_builder \
    "$repo_dir/tools/lets_and_go_mesh_builder_test.cpp"

compile_run garage_selection \
    "$repo_dir/tools/lets_and_go_garage_selection_test.cpp" \
    "$base/controller/garage_selection.cpp" \
    "$base/controller/game_flow.cpp" \
    "$base/model/car_catalog.cpp"

compile_run track \
    "$repo_dir/tools/lets_and_go_track_test.cpp" \
    "$base/model/overpass_track.cpp" \
    "$base/model/track_types.cpp"

compile_run input \
    "$repo_dir/tools/lets_and_go_input_test.cpp"

compile_run device_control \
    "$repo_dir/tools/lets_and_go_device_control_test.cpp" \
    "$base/controller/game_flow.cpp"

compile_run garage_view \
    "$repo_dir/tools/lets_and_go_garage_view_test.cpp"

compile_run racer_model \
    "$repo_dir/tools/lets_and_go_racer_model_test.cpp" \
    "$base/model/racer_model.cpp" \
    "$base/model/car_catalog.cpp" \
    "$base/model/overpass_track.cpp" \
    "$base/model/track_types.cpp"

race_sources=(
    "$base/controller/race_controller.cpp"
    "$base/controller/game_flow.cpp"
    "$base/model/car_catalog.cpp"
    "$base/model/overpass_track.cpp"
    "$base/model/racer_model.cpp"
    "$base/model/rival_ai.cpp"
    "$base/model/track_types.cpp"
)

compile_run race \
    "$repo_dir/tools/lets_and_go_race_test.cpp" \
    "${race_sources[@]}"

compile_run balance \
    "$repo_dir/tools/lets_and_go_balance_test.cpp" \
    "${race_sources[@]}"

compile_run results \
    "$repo_dir/tools/lets_and_go_results_test.cpp" \
    "$base/controller/results_selection.cpp" \
    "$base/controller/game_flow.cpp"

compile_run render_budget \
    "$repo_dir/tools/lets_and_go_render_budget_test.cpp"

compile_run soak \
    "$repo_dir/tools/lets_and_go_soak_test.cpp" \
    "${race_sources[@]}"

compile_run vector_input_contract \
    "$repo_dir/tools/vector_canyon_input_contract_test.cpp" \
    "$repo_dir/main/apps/app_vector_canyon_fighter/model/flight_model.cpp"

echo "[host-test] vector_render_budget"
"$cxx" "${flags[@]}" -I"$repo_dir/main/apps/app_vector_canyon_fighter" \
    "$repo_dir/tools/vector_canyon_render_budget_test.cpp" \
    -o "$out_dir/vector_render_budget"
"$out_dir/vector_render_budget"

compile_run vector_external_input \
    "$repo_dir/tools/vector_canyon_external_input_test.cpp"

compile_run launcher_external_input \
    "$repo_dir/tools/launcher_external_input_test.cpp"

vector_base="$repo_dir/main/apps/app_vector_canyon_fighter"
vector_stream="$vector_base/model/explicit_canyon_stream.cpp"
vector_flight="$vector_base/model/flight_model.cpp"
vector_collision="$vector_base/model/explicit_canyon_collision.cpp"

compile_run vector_composite_input \
    "$repo_dir/tools/vector_canyon_composite_input_test.cpp" \
    "$vector_base/input/composite_input_provider.cpp"
compile_run vector_flight_attitude \
    "$repo_dir/tools/vector_canyon_flight_attitude_test.cpp" "$vector_flight"
compile_run vector_hud_layout \
    "$repo_dir/tools/vector_canyon_hud_layout_test.cpp"
compile_run vector_hud_semantics \
    "$repo_dir/tools/vector_canyon_hud_semantics_test.cpp" "$vector_flight"
compile_run vector_imu_attitude \
    "$repo_dir/tools/vector_canyon_imu_attitude_test.cpp"
compile_run vector_explicit_event_stream \
    "$repo_dir/tools/vector_canyon_explicit_event_stream_test.cpp" "$vector_stream"
compile_run vector_explicit_projection \
    "$repo_dir/tools/vector_canyon_explicit_projection_test.cpp" "$vector_stream"
compile_run vector_explicit_curved_baseline \
    "$repo_dir/tools/vector_canyon_explicit_curved_baseline_test.cpp" "$vector_stream"
compile_run vector_explicit_static_baseline \
    "$repo_dir/tools/vector_canyon_explicit_static_baseline_test.cpp" "$vector_stream"
compile_run vector_explicit_production \
    "$repo_dir/tools/vector_canyon_explicit_production_test.cpp" \
    "$vector_stream" "$vector_flight"
compile_run vector_collision_visual_alignment \
    "$repo_dir/tools/vector_canyon_collision_visual_alignment_test.cpp" \
    "$vector_stream" "$vector_collision"
compile_run vector_explicit_integration \
    "$repo_dir/tools/vector_canyon_explicit_integration_test.cpp" \
    "$vector_stream" "$vector_collision" "$vector_flight"

echo "[host-test] production_renderers"
bash "$repo_dir/tools/render_lets_and_go.sh" "$out_dir/frames"

echo "[host-test] all game, device control, Vector Run, Launcher and renderer suites passed"
