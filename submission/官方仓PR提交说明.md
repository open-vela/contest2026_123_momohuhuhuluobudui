# 官方专属仓最终日志与文档 PR 说明

## 当前状态（2026-09-20 核对）

- 核心功能、摄像头修复和首轮隐私文档已合入官方分支。
- 目标分支：`dev-ai-contest-2026`
- 本轮来源分支：`yhx06:docs/submission-privacy`
- 本轮内容：补齐 Codex AI Coding 日志、同步最终统计与提交文档。

推送后从 `docs/submission-privacy` 向官方 `dev-ai-contest-2026` 创建或更新 PR，完成 Review 并在截止前合入。

当前最终代码和 AI Coding 日志位于个人 Fork：

- 仓库：`https://github.com/yhx06/contest2026_123_momohuhuhuluobudui`
- 分支：`docs/submission-privacy`

比赛要求最终内容通过 PR 合入：

- 仓库：`https://github.com/open-vela/contest2026_123_momohuhuhuluobudui`
- 目标分支：`dev-ai-contest-2026`

## 创建或打开本轮 PR

浏览器打开：

https://github.com/open-vela/contest2026_123_momohuhuhuluobudui/compare/dev-ai-contest-2026...yhx06:docs/submission-privacy?expand=1

确认页面顶部显示：

```text
base repository: open-vela/contest2026_123_momohuhuhuluobudui
base: dev-ai-contest-2026
head repository: yhx06/contest2026_123_momohuhuhuluobudui
compare: docs/submission-privacy
```

## 建议 PR 标题

```text
docs: complete AgentGuard AI logs and submission statistics
```

## 建议把 PR 正文改为

```markdown
## Summary

- add AgentGuard on-device ESP-DL face detection and multi-face bounding boxes
- add sedentary reminder, privacy mode, Power LED feedback and BOOT controls
- add USB-first desktop notifications, privacy shield and event statistics
- add host tests, reproducible build documentation, custom Coding Skill and contest materials
- include validated AI Coding logs under `logs/yhx06/`

## Hardware

- ESP32-S3-EYE
- openvela/NuttX
- OV2640 camera, ST7789 LCD, BOOT key and green Power LED

## Verification

- board host test suite passes
- PC monitor suite: 22 tests pass
- final firmware: 2,414,780 bytes, below the 0x300000 partition limit
- flash data hash verified
- empty/single/double-face states physically verified
- privacy FACE:2 → LED double flash → PC full-screen shield verified
- AI logs: 37 files, 10,970 events, official validator ALL OK

## Scope

The project detects and counts faces. It does not perform owner/identity recognition, KWS or camera gesture recognition.
```

## CLA 与合入

1. 当前 CLA 检查已经成功，无需重新签署或评论 `/check-cla`。
2. 在 PR #1 的 Files changed 页快速确认没有 Token、密码、构建产物或无关个人文件。
3. 执行 Review，并将 PR 合入 `dev-ai-contest-2026`。
4. 打开官方仓分支，确认根 README、`app/agentguard/`、`monitor/`、`skills/` 和 `logs/yhx06/` 均存在。
