# 官方专属仓 PR #1 合并说明

## 当前状态（2026-09-19 核对）

- PR：https://github.com/open-vela/contest2026_123_momohuhuhuluobudui/pull/1
- 状态：Open，非 Draft
- 目标分支：`dev-ai-contest-2026`
- 来源分支：`yhx06:feature/ai-error-safe-state`
- 最新提交：`514ab50`
- GitHub 合并状态：`mergeable: true`、`mergeable_state: clean`
- `cla / cla-check`：Success

不要重复创建 PR。直接打开 PR #1，完成 Review 并合入。

当前最终代码和 AI Coding 日志位于个人 Fork：

- 仓库：`https://github.com/yhx06/contest2026_123_momohuhuhuluobudui`
- 分支：`feature/ai-error-safe-state`

比赛要求最终内容通过 PR 合入：

- 仓库：`https://github.com/open-vela/contest2026_123_momohuhuhuluobudui`
- 目标分支：`dev-ai-contest-2026`

## 若 PR #1 页面无法打开

浏览器打开：

https://github.com/open-vela/contest2026_123_momohuhuhuluobudui/compare/dev-ai-contest-2026...yhx06:feature/ai-error-safe-state?expand=1

确认页面顶部显示：

```text
base repository: open-vela/contest2026_123_momohuhuhuluobudui
base: dev-ai-contest-2026
head repository: yhx06/contest2026_123_momohuhuhuluobudui
compare: feature/ai-error-safe-state
```

## 建议把 PR 标题改为

```text
feat: submit AgentGuard desktop health and privacy guardian
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
- AI logs: 16 files, 2,229 events, official validator ALL OK

## Scope

The project detects and counts faces. It does not perform owner/identity recognition, KWS or camera gesture recognition.
```

## CLA 与合入

1. 当前 CLA 检查已经成功，无需重新签署或评论 `/check-cla`。
2. 在 PR #1 的 Files changed 页快速确认没有 Token、密码、构建产物或无关个人文件。
3. 执行 Review，并将 PR 合入 `dev-ai-contest-2026`。
4. 打开官方仓分支，确认根 README、`app/agentguard/`、`monitor/`、`skills/` 和 `logs/yhx06/` 均存在。
