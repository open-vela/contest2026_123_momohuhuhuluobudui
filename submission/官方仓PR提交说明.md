# 官方专属仓 PR 提交说明

当前最终代码和 AI Coding 日志位于个人 Fork：

- 仓库：`https://github.com/yhx06/contest2026_123_momohuhuhuluobudui`
- 分支：`feature/ai-error-safe-state`

比赛要求最终内容通过 PR 合入：

- 仓库：`https://github.com/open-vela/contest2026_123_momohuhuhuluobudui`
- 目标分支：`dev-ai-contest-2026`

## 创建 PR

浏览器打开：

https://github.com/open-vela/contest2026_123_momohuhuhuluobudui/compare/dev-ai-contest-2026...yhx06:feature/ai-error-safe-state?expand=1

确认页面顶部显示：

```text
base repository: open-vela/contest2026_123_momohuhuhuluobudui
base: dev-ai-contest-2026
head repository: yhx06/contest2026_123_momohuhuhuluobudui
compare: feature/ai-error-safe-state
```

## PR 标题

```text
feat: submit AgentGuard desktop health and privacy guardian
```

## PR 正文

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

1. 创建 PR 后查看 `cla/signature` 检查。
2. 若未通过，使用报名时的 GitHub 账号在 openvela 官网签署 CLA。
3. 回到 PR 评论 `/check-cla`。
4. 检查通过后执行 Review，并将 PR 合入 `dev-ai-contest-2026`。
5. 打开官方仓分支，确认根 README、`app/agentguard/`、`monitor/`、`skills/` 和 `logs/yhx06/` 均存在。
