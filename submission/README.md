# AgentGuard 官网提交材料目录

本目录按官方作品提交模板整理。源码和 AI Coding 日志不放入官网压缩包，评审直接从赛事仓库获取。

## 已生成

- `AgentGuard-技术报告.docx`：必交技术报告，可继续用 LibreOffice/Word 微调。
- `AgentGuard-技术报告.pdf`：必交技术报告 PDF 版。
- `../docs/submission/演示视频脚本.md`：≤5 分钟必交视频的逐镜头脚本。
- `作品照片拍摄清单.md`：硬件作品必交照片的文件名和画面要求。
- `官网提交清单.md`：上传前最后核对。

## 仍需人工完成

1. 按脚本实拍并导出 `AgentGuard-演示视频.mp4`，总时长不超过 5 分钟。
2. 将硬件照片放入 `photos/`，至少包含清单中的 6 张。
3. 核对报告中的报名队名及 `yhx06` 是否应替换为真实姓名。
4. 如补测两小时稳定性或功耗，将结果同步到 Markdown、DOCX 和 PDF。

## 最终压缩包

建议名称：

```text
momohuhuhuluobudui-AgentGuard-contest2026_123_momohuhuhuluobudui.zip
```

压缩包只放：技术报告 PDF/DOCX、演示视频和 `photos/`。海报与答辩 PPT 为可选项，时间不足时不影响初赛必交材料。

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
