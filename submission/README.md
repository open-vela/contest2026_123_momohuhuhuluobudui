# AgentGuard 官网提交材料目录

本目录按官方作品提交模板整理。源码和 AI Coding 日志不放入官网压缩包，评审直接从赛事仓库获取。

## 已生成

- `AgentGuard-技术报告.docx`：必交技术报告，可继续用 LibreOffice/Word 微调。
- `AgentGuard-技术报告.pdf`：必交技术报告 PDF 版。
- `AgentGuard-作品海报.pdf`：可选的一页作品海报，入围后可继续完善。
- `AgentGuard-答辩PPT.pptx`：可选的答辩演示文稿。
- `参赛信息填写稿.md`：官网表单和报告中需要人工填写的信息汇总。
- `材料校验值.sha256`：当前已生成文档的 SHA-256，修改或重新导出后需更新。
- `../docs/submission/演示视频脚本.md`：≤5 分钟必交视频的逐镜头脚本。
- `作品照片拍摄清单.md`：硬件作品必交照片的文件名和画面要求。
- `官网提交清单.md`：上传前最后核对。

## 仍需人工完成

1. 按脚本实拍并导出 `AgentGuard-演示视频.mp4`，总时长不超过 5 分钟。
2. 将硬件照片放入 `photos/`，至少包含清单中的 6 张。
3. 在 `参赛信息填写稿.md`、报告、海报和 PPT 中填写正式队名及真实姓名。
4. 如补测两小时稳定性或功耗，将结果同步到 Markdown、DOCX 和 PDF。
5. 将最终功能分支通过 PR 合入 open-vela 官方专属仓的 `dev-ai-contest-2026`。

## 最终压缩包

建议名称：

```text
<报名队伍名称>-AgentGuard-contest2026_123_momohuhuhuluobudui.zip
```

先将视频命名为 `AgentGuard-演示视频.mp4`（或 `.mov`），并把至少 6 张照片放入 `photos/`，然后执行：

```bash
python3 tools/build_submission_package.py \
  --team-name '报名时登记的正式队伍名称'
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
