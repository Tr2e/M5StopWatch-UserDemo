# Soft3D F1–F4 validation — 2026-09-23

## Candidate identity

- Commit: `6a57107c73ede5a878f69f1030d0db607b7fbcc6`
- Firmware: `V0.5-304-g6a57107`
- App binary SHA-256: `37eb2b91af6f3a56a38b5fb1fd3d556dc6f64aa17aa37ccd636b8dbaf5014a18`
- App binary: `0x20acb0`; app partition remains 59% free.
- Device: ESP32-S3 revision 0.2, 8 MiB octal PSRAM at 80 MHz, USB serial
  `/dev/cu.usbmodem83301`, MAC `44:1b:f6:c1:8a:00`.

## Delivered framework slices

- Neutral immutable `ModelAsset`, `ModelInstance`, `SceneView`, bounds, LOD,
  material, primitive and rigid-skeleton contracts.
- Shared deterministic vertex indexing, bounded scratch/fallback policy,
  workload-driven core/material/composite selection and frame-rate grading.
- Standard `renderModelAsset` / `renderScene` entry points consume world and
  rigid-pose transforms. Crate and eight-bone rigid chain use this same path.
- RX-78 is registered through the asset contract and hot-path capabilities;
  fast-path selection no longer depends on a Gundam model-name check.
- GLB + TOML compiler emits a linkable header and JSON budget report. It
  performs coordinate/unit/pivot conversion, node transforms, deterministic
  deduplication, safe `ABC + ACD` quad packing, bounds/normals/anchors, LOD and
  capacity accounting. Bad indices, NaN/Inf, degenerate triangles,
  transparency, textures/PBR programs, sparse accessors and weighted skins are
  rejected explicitly.

## Host gates

The final candidate passed:

```text
SANITIZE=1 bash tools/test_soft3d.sh /tmp/soft3d-final-sanitize
bash tools/test_gundam_arena.sh
bash tools/test_gundam_museum_perf.sh /tmp/soft3d-final-museum
bash tools/benchmark_lets_and_go.sh
idf.py build
```

- Soft3D ABI/compiler positive and rejection tests passed under ASan/UBSan.
- Museum: 6,720 pixel-identical cases; RX-78 retains 2,670 unique vertices.
- Arena motion/model regression passed.
- Racer garage, inspection, race, close-camera, near-clip, solid and wire
  benchmarks passed; no common-core correctness regression was observed.

## Same-device benchmark

Each scene ran 96 continuously rotating frames. Timing is CPU draw plus the
asynchronous present call; `panel_tx` is separately measured physical transfer
time and overlaps CPU work. Grades use P95 complete cycle, not the mean.

| Scene | Unique vertices | Primitives | Tested / written / depth-rejected / final pixels | Cycle mean | P95 | Max | Route | Grade |
| --- | ---: | ---: | --- | ---: | ---: | ---: | --- | --- |
| crate | 8 | 6 | 4,486 / 3,134 / 1,352 / 2,243 | 32.285 ms | 32.831 ms | 33.219 ms | single | 30 FPS |
| rigid chain | 64 | 48 | 5,859 / 3,579 / 2,279 / 1,995 | 36.085 ms | 37.074 ms | 37.327 ms | single | 24 FPS |
| RX-78 stress | 2,670 | 2,736 total; 1,965 submitted | established 41,468 / ~30,595 / ~10,900 / 13,566 | 66.800 ms | 69.860 ms | 70.246 ms | dual bands | below 15 FPS at strict P95 |

The RX mean is effectively the frozen 15 FPS pressure point (14.97 FPS), while
strict P95 does not meet the 66.667 ms 15 FPS deadline. This is reported as
`below15`, rather than rounding the mean into a stronger promise. The canonical
96-frame RX hash remained `1794963570:3828763871`; crate and rigid-chain hashes
were respectively `2887265117:4233499028` and `4251764397:2656961028`.

RX stage averages were `background=13.866 ms`, `cull=1.638 ms`,
`panel=8.050 ms`, `raster=59.387 ms`, `main=31.873 ms`, `worker=34.113 ms`,
`blit=5.196 ms`; physical panel transfer averaged 12.103 ms.

## Memory, fallback and lifecycle

- During sample residency: internal free/minimum/largest
  `24,923 / 23,959 / 7,680 B`; PSRAM free/minimum/largest
  `4,927,644 / 4,927,644 / 4,849,664 B`.
- Releasing the sample workspace returned PSRAM free to `5,083,296 B`.
- RX main-task and worker-task stack high-water reserves were 1,052 B and
  1,940 B. No overflow, watchdog reset or heap failure occurred; the main-task
  value is a benchmark/logging-path floor and must not be treated as spare
  budget for new stack arrays.
- Closing the RX renderer changed PSRAM free from `5,083,296` to `6,918,308 B`,
  releasing `1,835,012 B`. Reopen succeeded and returned to `5,083,296 B`:
  retained delta `0 B`.
- Host gates force insufficient internal-memory budgets for fixed and dynamic
  scratch, verify the fallback decision, and then render successfully using the
  bounded resident storage.

## Decision

Accept F1–F4 for the solid static/rigid scope. Automatic selection is based on
measured primitive or pixel work (`256` visible primitives or `18,000` tested
pixels for the Game30 profile), never asset names. Keep the previously closed
microtile and PSRAM-direct-DMA experiments closed. A physical-panel human check
of the two new sample silhouettes remains a release checklist item; hashes,
framebuffer regressions and device stability are automated gates, not a claim
that a human inspected the AMOLED in this run.
