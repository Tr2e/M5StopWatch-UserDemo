# Bandai Namco motion → Gundam Arena

源数据： [Bandai-Namco-Research-Motiondataset-1](https://github.com/BandaiNamcoResearchInc/Bandai-Namco-Research-Motiondataset)
许可： **CC BY-NC 4.0**（署名、非商用）。

`raw/` 里的 BVH 不入库。运行 `python3 tools/arena_motion/fetch_bandai.py` 拉取。
`retarget_bandai.py` 烤出：
- `main/apps/app_gundam_arena/model/arena_kick_clip.h`：运行时踢球
- `tools/arena_motion/arena_walk_clip.h`：对照用，**不**编进固件，走路仍用程序 IK

人体 mocap 不能把关节角 1:1 接到 SD。下蹲必须沉 Root.y 再 IK；跳高按身长比例；臂和空中膝要按短肢可读幅度放大，不要停在走路档。详见 `docs/Gundam-Arena-技术文档.md` §6.5。

