# Soft3D F0 baseline — 2026-09-23

Branch: `feat/gundam-arena`  
Commit: `b0230941f47c9a5387bb64c38bfe11ea29fa43b2`  
Device: ESP32-S3 rev0.2, USB serial/MAC `44:1B:F6:C1:8A:00`, `/dev/cu.usbmodem83301`

## Host gates

- `bash tools/test_gundam_museum.sh /tmp/soft3d-baseline-museum`
  - 864 Museum geometry/composition cases passed.
  - RX-78: 2,736 panels, 1,965 submitted, 771 culled.
- `bash tools/test_gundam_museum_perf.sh /tmp/soft3d-baseline-perf`
  - 6,720 framebuffer comparisons were pixel-identical.
  - RX-78 unique vertices: 2,670.
  - Host-only 65% optimized mean: 1,058.52 us, P95: 1,439.58 us.
- `bash tools/test_gundam_arena.sh /tmp/soft3d-baseline-arena`
  - Arena passed; 2,871 panels and one mesh/index build.
- `bash tools/benchmark_lets_and_go.sh /tmp/soft3d-baseline-racer-bench`
  - Racer production renderer benchmark passed.
  - High-detail garage mean/P95: 1.377/1.469 ms.
  - High-detail race mean/P95: 1.075/1.200 ms.
  - Near-clip low-detail race mean/P95: 0.921/1.247 ms.

Host timings are retained only as same-host regression signals. They are not used to
claim ESP32-S3 frame-rate improvements.

## Device baseline

The already-flashed baseline firmware was sampled without reset or reflash. Two
consecutive 96-frame batches reproduced the accepted canonical framebuffer hash:

```text
batch=291 draw=66575 present=100 panel_tx=12109 cycle=66676 background=13880 cull=1657 panel=7899 raster=59285 main=31904 worker=34148 blit=5189 hash=1794963570:3828763871
batch=292 draw=66596 present=101 panel_tx=12111 cycle=66697 background=13920 cull=1659 panel=7872 raster=59301 main=31904 worker=34152 blit=5190 hash=1794963570:3828763871
```

Mean of these two independently emitted batches:

- draw: 66.586 ms
- present: 0.101 ms
- complete cycle: 66.687 ms / 14.995 FPS
- panel TX: 12.110 ms, overlapped with rendering
- background: 13.900 ms
- cull: 1.658 ms
- panel prepare: 7.886 ms
- prepare+raster: 59.293 ms
- CPU0/CPU1 raster: 31.904/34.150 ms
- composite: 5.190 ms

This is within the established 23-batch baseline envelope (66.572 ms draw,
66.673 ms cycle) and freezes the pre-extraction acceptance contract.

## Preserved workspace state

`tools/arena_motion/__pycache__/` was present before F0. It is unrelated, remains
untracked, and must not be deleted or committed by the framework work.
