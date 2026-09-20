# AgentGuard 官网提交材料目录

本目录按官方作品提交模板整理。源码和 AI Coding 日志不放入官网压缩包，评审直接从赛事仓库获取。

## 已生成

- `AgentGuard-技术报告.docx`：必交技术报告，可继续用 LibreOffice/Word 微调。
- `AgentGuard-技术报告.pdf`：必交技术报告 PDF 版。
- `AgentGuard-作品海报.pdf`：可选的一页作品海报，入围后可继续完善。
- `AgentGuard-答辩PPT.pptx`：可选的答辩演示文稿。
- `参赛信息填写稿.md`：官网表单和报告中需要人工填写的信息汇总。
- `材料校验值.sha256`：当前已生成文档的 SHA-256，修改或重新导出后需更新。
- `视频拍摄清单.md`：≤5 分钟必交视频的逐镜头清单和 monitor 启动命令。
- `../docs/submission/演示视频脚本.md`：视频旁白与镜头安排的原始脚本。
- `作品照片拍摄清单.md`：硬件作品必交照片的文件名和画面要求。
- `官网提交清单.md`：上传前最后核对。

## 当前材料状态

1. 演示视频已经完成，本地交付副本为 `AgentGuard-演示视频.mp4`，实测时长 3 分 08 秒，满足不超过 5 分钟的要求；上传前仍需人工完整播放确认声音和画面。
2. 本地交付目录 `photos/` 已有 6 张硬件、功能或事件展示图片（含事件面板截图），满足打包校验要求。
3. 公开材料已填写队名、姓名和团队分工；手机、邮箱、学校和参赛者类别仅保存在 `submission/private/`，在比赛平台填写，不提交 GitHub。
4. 如补测两小时稳定性或功耗，将结果同步到 Markdown、DOCX 和 PDF。
5. 核心功能 PR #2 和摄像头修复 PR #3 已合并；还需将本次 Codex 日志与材料统计更新 PR 合入 open-vela 官方专属仓的 `dev-ai-contest-2026`。

## 最终压缩包

建议名称：

```text
魔魔胡胡胡萝卜队-AgentGuard-contest2026_123_momohuhuhuluobudui.zip
```

将视频命名为 `AgentGuard-演示视频.mp4`（或 `.mov`），并把至少 6 张照片放入 `photos/`，然后执行：

```bash
python3 tools/build_submission_package.py \
  --source /path/to/提交材料 \
  --team-name '魔魔胡胡胡萝卜队'
```

工具只会打包技术报告、演示视频、照片、海报和答辩 PPT；材料不全时拒绝生成 ZIP。生成后仍须人工完整播放视频，确认时长不超过 5 分钟。

## 报告重新生成

修改 `docs/submission/技术报告.md` 后，可执行：

```bash
python3 tools/render_submission_report.py \
  docs/submission/技术报告.md /tmp/AgentGuard-技术报告.html
libreoffice --headless --infilter='HTML (StarWriter)' \
  --convert-to odt --outdir /tmp /tmp/AgentGuard-技术报告.html
libreoffice --headless --convert-to 'docx:Office Open XML Text' \
  --outdir submission /tmp/AgentGuard-技术报告.odt
libreoffice --headless --convert-to pdf \
  --outdir submission /tmp/AgentGuard-技术报告.odt
```
