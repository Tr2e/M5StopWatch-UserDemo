# Bandai Namco motion → Gundam Arena

源数据： [Bandai-Namco-Research-Motiondataset-1](https://github.com/BandaiNamcoResearchInc/Bandai-Namco-Research-Motiondataset)
许可： **CC BY-NC 4.0**（署名、非商用）。

`raw/` 里的 BVH 不入库。运行 `python3 tools/arena_motion/fetch_bandai.py` 拉取。
`retarget_bandai.py` 烤出：
- `main/apps/app_gundam_arena/model/arena_kick_clip.h`：运行时踢球
- `tools/arena_motion/arena_walk_clip.h`：对照用，**不**编进固件，走路仍用程序 IK

