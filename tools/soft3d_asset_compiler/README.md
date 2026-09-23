# Soft3D GLB asset compiler

`compile_model.py` converts a deterministic glTF 2.0 binary subset into the
immutable `soft3d::ModelAsset` ABI and writes an audit-friendly JSON report.

```bash
python3 tools/soft3d_asset_compiler/compile_model.py model.glb \
  --manifest model.toml --out generated/my_model
```

The supported MVP is indexed TRIANGLES, embedded GLB buffers, node TRS/matrix
transforms, solid opaque materials, multiple LOD ranges, and optional rigid
node bindings declared with `rigid_nodes = ["arm", "tool"]`. Coordinates are
converted to right-handed `+Y` up / `+Z` front, and `pivot = "ground_center"`
is the default. Consecutive `ABC, ACD` triangles are packed only when material,
binding, sidedness, and coplanarity match.

The compiler deliberately rejects transparency, textures/PBR programs,
weighted skins, morph targets, sparse accessors, unsupported vertex attributes,
non-triangle modes, non-finite data, degenerate triangles, invalid indices, and
16-bit capacity overflow. Unsupported content must not silently degrade on the
ESP32.
