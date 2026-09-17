# Bandai Namco motion → Gundam Arena

源数据： [Bandai-Namco-Research-Motiondataset-1](https://github.com/BandaiNamcoResearchInc/Bandai-Namco-Research-Motiondataset)
许可： **CC BY-NC 4.0**（署名、非商用）。

`raw/` 里的 BVH 和 cfg 不入库。运行 `python3 tools/arena_motion/fetch_bandai.py` 拉集 1；`python3 tools/arena_motion/fetch_bandai2.py` 拉集 2 的 10+10 条 BVH + LICENSE（`fetch_bandai2_index.py` 只拉标签）。
`retarget_bandai.py` 烤出：
- `main/apps/app_gundam_arena/model/arena_kick_clip.h`：运行时踢球
- `main/apps/app_gundam_arena/model/arena_gesture_clips.h`：六段空手手势（P5：WAVE L/R 用 `active`，其余 `normal`）
- `tools/arena_motion/arena_walk_clip.h`：对照用，**不**编进固件，走路仍用程序 IK
- `tools/arena_motion/dataset2_preview.txt`：集 2 walk/run/turn 及风格对照，不进固件
- `tools/arena_motion/dataset2_style_report.txt`：P5 风格取舍与每段 flash 字节 / 主机渲染微秒

人体 mocap 不能把关节角 1:1 接到 SD。下蹲必须沉 Root.y 再 IK；跳高按身长比例；臂和空中膝要按短肢可读幅度放大，不要停在走路档。踢球不要播源数据的膝鞭（折→弹→再折），前摆小腿只能伸一次。详见 `docs/Gundam-Arena-技术文档.md` §6.5。

