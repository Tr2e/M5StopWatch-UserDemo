# Race vehicle raster controlled optimization

This batch changes only the race-car triangle hot loop. For each scanline it
evaluates barycentric coordinates exactly at the first pixel, then advances
them with precomputed horizontal increments. The original evaluator remains
available behind the renderer switch for paired regression and device A/B
measurement. Garage rendering is unchanged.

## Controlled device result

- Device: StopWatch ESP32-S3, 240 MHz, 8 MB PSRAM.
- Workload: all 8 cars, all 3 tracks, 24 timed three-car poses per group.
- Pairing: original and incremental modes rendered the same snapshot in
  alternating order, for 576 pairs / 1,152 measured frames.
- Visual result: 0 CRC mismatches across all 576 frame pairs.
- Raster mean: 48,667.72 us -> 40,501.92 us, saving 8,165.80 us / 16.779%.
- Full draw mean: 132,352.47 us -> 124,463.49 us, saving 7,888.98 us / 5.961%.
- Every one of the 24 car/track groups improved. Per-group raster savings were
  16.124% to 17.415% (median 16.841%).
- Presentation was neutral: 31,483.76 us -> 31,479.08 us in the canvas-based
  test harness.
- End state: 0 mismatches, internal heap 84,139 bytes, PSRAM 5,493,908 bytes,
  task stack high-water mark 2,436 bytes.
- Normal firmware build: 0x397610 bytes; the 0x4f0000 application partition
  retains 0x1589f0 bytes (27%).

The occasional whole-frame spike includes existing periodic diagnostic log
work. The acceptance decision is therefore anchored on the nested `raster`
timer, which excludes those later logs; the paired full-draw mean is retained
as the end-to-end corroboration.

## Evidence

- `ab-device.log`: raw serial output, including the final complete run after
  the last `RasterAB BEGIN` marker.
- `frames.csv`: normalized timed samples.
- `summary.json`: aggregate and all 24 group summaries.
- `analyze.py`: standard-library-only reproducible parser.
