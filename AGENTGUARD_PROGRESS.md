# AgentGuard 开发上下文与进度接力

> 最后更新：2026-08-14（Asia/Shanghai）  
> 工作区：`/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui`  
> openvela 根目录：`/home/yhx/Desktop/openvela`  
> 用途：当 AI 对话上下文被压缩或更换模型时，先阅读本文，再继续开发。本文只记录已有证据，不把启发式算法或编译成功误报为完整 AI/真机功能完成。

## 0. 2026-08-14 紧急交接：ESP-DL TIE 自检导致当前 LCD 白屏

这一节是下一位 AI 的首要入口。当前板上不是可演示版本，而是诊断固件。

### 0.1 用户协作约束

- 用户明确要求：常规软件修改、编译、提交、烧录和自动串口检查不要逐项询问；
  只有需要实体按键、断电或读取 LCD 真机结果时再找用户。
- 用户选择内联执行，不使用子代理。
- 不得清理或恢复工作区中与本任务无关的既有修改和未跟踪文件。
- 当前真实 Git 目录为：
  `/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git`。
  常用命令必须显式带 `--git-dir` 和 `--work-tree`；工作区存在
  `.git.codex-hold`，不要擅自删除。

### 0.2 已获得的模型/TIE 实验矩阵

当前模型是 ESP-DL `MSRMNP_S8_V1`，输入和模型主链已经能够执行，但 TIE
卷积结果错误。LCD 数值的含义为：`T`=内置参考图最高分百分比，`F`=参考图
人脸数，`S`=实时最高分百分比，`M`=候选数，`AI`=推理毫秒。

1. 普通卷积和深度卷积均使用 TIE：
   `AI≈60, T/F/S=2/0/2, M=0, FACE=0`，有无真人完全相同。
2. 所有算子强制纯 C：
   `AI≈590-600, T/F/S=57/0/57`。官方阈值约 0.5，而产品当时设为 0.60；
   证明模型、预处理和后处理总体能执行，TIE 与 C 数值明显分叉。
3. 普通卷积和深度卷积均为 C，其他算子保留 TIE：仍约 `600/57/0/57`；
   故障定位到卷积族。
4. 普通卷积 TIE、深度卷积 C：`AI≈180-220, T/F/S=2/0/2`；普通卷积
   TIE 明确错误。
5. 普通卷积 C、深度卷积 TIE：`AI≈610-660, T/F/S=95/1/95, M=1`，
   无人也误报；深度卷积 TIE 也错误。

### 0.3 已排除的内存假设

第一次尝试启用 NuttX 全局 `CONFIG_XTENSA_IMEM_USE_SEPARATE_HEAP`，导致启动
黑屏。串口 panic 地址解析表明任务栈创建进入 `xtensa_imm_malloc` 后异常；这是
全局切换破坏 flat 启动，不可再次启用。配置脚本已经显式关闭它。

随后实现了 96 KiB AgentGuard 私有内部内存池：

- `app/agentguard/src/espdl_private_pool.cpp`
- `app/agentguard/src/espdl_compat.cpp`
- `app/agentguard/compat/espdl/espdl_private_pool.h`

池由 NuttX `mm_initialize` 管理，只路由 `INTERNAL/SIMD/DMA` 能力分配，不影响
系统任务栈。主机测试、内存映射和烧录均通过；激活张量确实进入 `0x3fc...`
内部 RAM，但双 TIE 结果仍为 `2/0/2`。因此“激活张量位于 PSRAM”已被否定，
不要再重复该方向。

相关提交：

```text
ff827b9 docs: design ESP-DL private internal pool
5c8f1da docs: plan ESP-DL private internal pool
63caa3d feat: add ESP-DL private internal pool
de5df5c fix: isolate ESP-DL internal allocations
```

### 0.4 最小 TIE 卷积自检实现

为区分“Flash/DROM 权重读取失败”和“TIE ABI/指令本身失败”，新增一个 16 输入、
16 输出、所有值均为 1 的 `dl_tie728_s8_conv2d_11cn` 自检：

```text
app/agentguard/include/agentguard/espdl_tie_selftest.h
app/agentguard/src/espdl_tie_selftest_logic.cpp
app/agentguard/src/espdl_tie_selftest.cpp
app/agentguard/tests/test_espdl_tie_selftest.cpp
```

标量期望为每个输出 16。自检分别使用 RAM 权重和 `const` Flash/DROM 权重。
ELF 已确认：

```text
RAM filter:   0x3fc9...
Flash filter: 0x3c01...
```

`ArgsType<int8_t>` 与汇编读取的偏移使用 `static_assert(offsetof(...))` 锁定；目标
构建和链接通过。分类约定：

```text
K:10 = RAM 通过、Flash 失败
K:00 = RAM 和 Flash 都失败
K:11 = 两者都通过
K:01 = RAM 失败、Flash 通过
```

相关设计与计划：

```text
docs/superpowers/specs/2026-08-13-espdl-tie-conv-selftest-design.md
docs/superpowers/plans/2026-08-13-espdl-tie-conv-selftest.md
```

相关提交：

```text
35e1516 docs: design TIE convolution self-test
a2c33fd docs: plan TIE convolution self-test
46dd669 test: define TIE convolution self-test
6f75297 feat: diagnose TIE convolution memory access
9ef0cec docs: route TIE self-test result to LCD
87f0095 feat: display TIE self-test result
698de5d docs: decouple TIE self-test from camera
220697c docs: plan camera-independent TIE diagnostic
0d3c22a fix: decouple TIE diagnostic from camera
```

### 0.5 当前白屏的直接因果

最初自检位于首帧模型入口。摄像头不出帧时屏幕只能看到重叠的
`CAM WORK.../V4L2...`，没有 `K:`，因为自检根本没有运行。随后提交 `0d3c22a`
将自检移动到 `ag_run()`：LCD 设备打开后、显示线程创建前、摄像头注册前直接
调用 `ag_espdl_tie_conv_selftest_run_once()`。

该版本完成了以下自动验证：

- 8 个主机测试可全部通过；
- NuttX 目标构建成功；
- objdump 确认启动路径调用 `run_once`，显示线程和模型桥接调用 `get_result`；
- 直接烧录写后哈希校验成功；
- 最终板上镜像 SHA-256：
  `aeb62b0998cd47ab5bee4b32122ac4d528cfd44d4ddab2b084e9265ccecca38b`。

用户随后报告：**LCD 白屏**。这与代码顺序构成强证据：LCD 控制器已经初始化，
但显示线程尚未创建，程序进入直接 TIE 自检后未返回。因此当前最强假设是：
`dl_tie728_s8_conv2d_11cn` 直接调用发生阻塞或异常；不能再把白屏归因于 LCD
格式、摄像头或私有内存池。

ROM 串口只能看到 simple-boot 段，NuttX 应用 `stderr` 不映射到当前 USB CDC。
USB-JTAG OpenOCD 也无法使用，因为当前虚拟机没有 `/dev/bus/usb`；不要浪费时间
重复串口/JTAG读取，除非环境发生变化。

### 0.6 下一步已提出但尚未实现的方案 A

用户准备结束会话时，已提出以下推荐方案，但尚未获得该方案的最后确认、尚未
改代码或烧录：

1. 先启动 LCD 显示线程，再执行 TIE 自检，避免自检阻塞造成白屏。
2. 在两次直接内核调用周围发布原子阶段状态：
   - `D:40`：即将/正在调用 RAM 权重 TIE；若永久停在此处，说明第一条 TIE
     内核调用本身不返回。
   - `D:41`：RAM 调用已经返回，正在调用 Flash 权重 TIE。
   - `D:42`：两次调用都返回，此时同时显示 `K:xy`。
3. 显示线程必须在自检阻塞时仍持续刷新；阶段和结果跨线程发布要使用原子变量
   或可靠同步，不能留下 C/C++ 数据竞争。
4. 当前摄像头阶段显示已经改为稳定数字 `CAM:<phase>`，不要恢复会重叠的长
   文本标签。
5. 方案 A 完成后先运行 8 个主机测试、目标构建和链接检查，再烧录。只向用户
   询问 LCD 上的 `D:`、`K:`（若有）和 `CAM:`。

如果 `D:40` 卡住，下一步不应再尝试 Flash 权重搬运；应转向核对 TIE 直接调用
约定、协处理器状态初始化、内核所需寄存器/线程上下文，或与同工具链的 ESP-IDF
最小参考程序比较。如果 `D:41` 卡住，才说明 RAM 路径返回而 Flash/DROM 向量读取
是故障边界。如果到 `D:42`，读取 `K:xy` 后按分类表继续。

### 0.7 当前不可宣称事项

- 当前 LCD 是白屏，摄像头没有可见画面；不能宣称应用可用。
- 尚未取得 RAM/Flash 自检的 `K:xy`，不能宣称已定位到 Flash 权重。
- 私有内部池已证明没有修复 TIE 数值，不得把它描述为最终修复。
- TIE 卷积仍是主要故障，纯 C 路径虽有较合理信号但约 600 ms，且参考图最高分
  约 57、尚未完成最终阈值和真人精度验收。

## 1. 用户目标与约束

- 硬件：ESP32-S3-EYE。
- 操作系统：openvela/NuttX。
- 比赛仓库：`https://github.com/yhx06/contest2026_123_momohuhuhuluobudui`。
- 分支：`dev-ai-contest-2026`。
- 原始需求材料：`/home/yhx/Desktop/申请材料.docx`。
- 必须遵守比赛仓库结构和要求，并沉淀可复用 Skill，作为 AI Coding 加分材料。
- 用户已经明确允许不备份并恢复被错误修改的官方 Kconfig；不要再次恢复或删除其他未授权的本地改动。

从申请材料提取出的 AgentGuard 目标包括：

1. 久坐检测、提醒、确认、离座和超时锁屏联动。
2. 坐姿风险推断。
3. 多人出现时启动 PC 隐私遮罩，恢复单人时解除。
4. 离线语音命令、按键和手势交互。
5. 本地 LittleFS 行为日志与规则统计。
6. 通过局域网 PC 代理执行通知、隐私遮罩和可选锁屏。
7. 原始图像留在开发板，PC 只接收事件。

## 2. 环境与工具确认

### 2.1 ESP32-S3 工具链

openvela 内已有两套可用路径，实际构建使用预置 GCC：

```text
/home/yhx/Desktop/openvela/prebuilts/gcc/linux-x86_64/xtensa-esp32s3-elf/
/home/yhx/Desktop/openvela/xtensa-esp32s3-elf/
```

正确的 `nm` 示例：

```bash
/home/yhx/Desktop/openvela/prebuilts/gcc/linux-x86_64/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-nm
```

### 2.2 esptool

本地官方说明：

```text
/home/yhx/Desktop/openvela/docs/zh-cn/quickstart/development_board/ESP32-S3-EYE.md
```

已确认虚拟环境和版本：

```text
/home/yhx/Desktop/openvela/myenv/bin/esptool
esptool v5.3.1
```

后续所有编译和烧录操作必须先执行：

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
```

系统 Python 没有安装 esptool 是正常的。构建系统仍调用兼容入口 `esptool.py`，v5.3.1 会显示弃用警告，但当前不影响构建和烧录。

## 3. 官方仓库与 Kconfig 修复记录

已通过远端引用和本地 HEAD 核对以下项目与官方 `dev-ai-contest-2026` 一致：

- `nuttx`
- `nuttx-apps`
- `frameworks`
- `vendor_espressif`

发现先前 AI 错误缩短或替换了以下官方文件，已按用户授权直接恢复：

```text
apps/modbus/Kconfig
apps/nshlib/Kconfig
apps/platform/Kconfig
apps/tools/Kconfig
vendor/espressif/boards/esp32s3/esp32s3-eye/configs/openvela/defconfig
```

已删除下列错误生成或非官方的未跟踪 Kconfig/构建文件：

```text
apps/.gitee/Kconfig
apps/.github/Kconfig
apps/builtin/Kconfig
apps/cmake/Kconfig
apps/import/Kconfig
apps/include/Kconfig
external/esp-who/Kconfig
external/esp-who/Makefile
```

随后执行过：

```bash
repo sync -c -j8 apps nuttx frameworks vendor/espressif external prebuilts/build-tools/linux-x86_64
```

修复后官方 ESP32-S3-EYE 基线能够成功刷新配置、编译并生成 `nuttx.bin`，证明 BSP/Kconfig 主链已经恢复。

以下外层 openvela 改动不是本轮创建，已保留，不能擅自恢复：

```text
apps/nshlib/nsh_timcmds.c
nuttx/arch/xtensa/src/esp32s3/Make.defs
nuttx/boards/xtensa/esp32s3/esp32s3-eye/configs/wifi/defconfig
若干 NIST-STS 未跟踪文件
```

## 4. 已实现的比赛仓库内容

### 4.1 Manifest 集成

`contest2026_123_momohuhuhuluobudui.xml` 新增：

```xml
<linkfile src="app/agentguard" dest="packages/demos/contest2026_123_agentguard"/>
```

当前完整工作区中存在对应链接：

```text
/home/yhx/Desktop/openvela/packages/demos/contest2026_123_agentguard
  -> contest2026_123_momohuhuhuluobudui/app/agentguard
```

### 4.2 openvela/NuttX 应用

正式应用目录：`app/agentguard/`。

关键文件：

```text
app/agentguard/Kconfig
app/agentguard/Make.defs
app/agentguard/Makefile
app/agentguard/CMakeLists.txt
app/agentguard/include/agentguard/core.h
app/agentguard/include/agentguard/vision.h
app/agentguard/src/core.c
app/agentguard/src/vision.c
app/agentguard/src/agentguard_main.c
app/agentguard/tests/test_core.c
app/agentguard/tests/test_vision.c
```

已经实现：

- 久坐、离座、提醒、确认、暂停、隐私、锁屏请求和亮度事件状态机。
- 默认久坐阈值 30 分钟；提醒后 60 秒未确认产生锁屏请求。
- 28 条中文离线命令的文本映射层。
- RGB565 肤色连通域启发式检测，用于相机数据链路联调。
- 基于主区域位置/尺寸相对校准值的基础坐姿风险分数。
- NuttX V4L2 `/dev/video0` USERPTR 采集适配。
- `/dev/buttons` 和 `/dev/userleds` 接口适配。
- JSONL 事件追加日志。
- 使用 Bearer Token 的局域网 HTTP 事件上报。
- NSH 命令名：`agentguard`。

重要限制：`src/vision.c` 当前是肤色启发式，不是 ESP-WHO、ESP-DL 或 TFLite 人脸/姿态模型，不能宣称已完成人脸识别精度或颈椎角度模型。

### 4.3 PC 代理

目录：`monitor/`。

关键文件：

```text
monitor/agentguard_pc.py
monitor/test_agentguard_pc.py
monitor/requirements.txt
```

已实现：

- 标准库 HTTP 服务。
- Bearer Token 常量时间比较。
- 请求体大小限制。
- 事件类型白名单。
- Tk 隐私全屏遮罩切换。
- 桌面通知。
- 锁屏默认禁用，只有显式 `--allow-lock` 才允许执行。
- 默认不面向局域网暴露；需要显式 `--allow-lan`。

协议目前是 HTTP + 共享令牌，没有传输加密和防重放；只能用于可信局域网联调。

### 4.4 Skill 沉淀

已使用官方 `skill-creator` 规范创建：

```text
skills/develop-agentguard-openvela/SKILL.md
skills/develop-agentguard-openvela/agents/openai.yaml
skills/develop-agentguard-openvela/references/contest-layout.md
skills/develop-agentguard-openvela/references/esp32s3-eye.md
skills/develop-agentguard-openvela/scripts/check_workspace.sh
```

Skill 约束包括：优先官方 BSP、便携核心与硬件适配分层、明确区分启发式和真实模型、没有真机证据时不得宣称验证完成。

校验命令和结果：

```bash
python3 /home/yhx/.codex/skills/.system/skill-creator/scripts/quick_validate.py \
  skills/develop-agentguard-openvela
# Skill is valid!
```

工作区检查脚本也已通过。

## 5. 已完成的测试与构建证据

### 5.1 主机 C 测试

```bash
cd app/agentguard/tests
make clean test
```

结果：

```text
AgentGuard core tests: PASS
AgentGuard vision tests: PASS
```

编译参数包含 `-Wall -Wextra -Werror -pedantic`。

### 5.2 PC 代理测试

```bash
cd monitor
python3 -m unittest -v test_agentguard_pc.py
```

结果：4/4 通过，覆盖：

- 锁屏默认禁用。
- 隐私遮罩状态切换。
- 错误 Token 拒绝。
- 未知事件拒绝。

### 5.3 openvela 交叉编译

应用配置符号：

```text
CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD=y
```

本轮为了避免对恢复后的官方 defconfig 再做未经审查的提交，该符号启用在当前 `nuttx/.config` 中；仓库 README 记录了通过 `menuconfig` 复现的方法。

成功构建命令：

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx \
  EXTRAFLAGS='-Wno-cpp -Wno-deprecated-declarations' -j8
```

构建输出明确包含：

```text
Register: agentguard
CC: src/agentguard_main.c
CC: src/core.c
CC: src/vision.c
LD: nuttx
Generated: nuttx.bin
```

ELF 字符串和符号已确认包含 `agentguard` 和 `agentguard_main`。

固件产物：

```text
/home/yhx/Desktop/openvela/nuttx/nuttx       约 21 MB
/home/yhx/Desktop/openvela/nuttx/nuttx.hex   约 4.3 MB
/home/yhx/Desktop/openvela/nuttx/nuttx.bin   1,684,132 bytes
```

`esptool image-info` 验证结果：

```text
Chip: ESP32-S3
Image version: 1
Flash mode: DIO
Flash frequency: 40 MHz
Checksum: valid
```

## 6. 2026-08-11 真机烧录结果

已识别串口：

```text
/dev/ttyACM0
```

当前用户 `yhx` 属于 `dialout` 组，无需 sudo 即可访问串口。

按照本地官方 ESP32-S3-EYE 文档执行：

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
cd /home/yhx/Desktop/openvela/nuttx
make -j8 flash ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

烧录结果：

```text
芯片：ESP32-S3 revision v0.2
PSRAM：8 MB
自动检测 Flash：8 MB
USB 模式：USB-Serial/JTAG
写入地址：0x00000000
写入原始数据：1,684,132 bytes
压缩数据：1,018,650 bytes
Verifying written data... Hash of data verified.
Hard resetting via RTS pin...
```

结论：烧录和 Flash 数据校验成功。

### 当前真机启动阻塞

首次串口没有出现 NSH。通过 RTS 复位抓到：

```text
ESP-ROM:esp32s3-20210327
rst:0x15 (USB_UART_CHIP_RESET),boot:0x22 (DOWNLOAD(USB/UART0))
waiting for download
```

因此开发板当时处于下载模式。当前**不能**宣称 openvela、NSH、相机或 AgentGuard 已在真机运行成功。下一步应先让板子退出下载模式；优先请用户确认没有按住 BOOT，然后短按 RESET，再立即抓取 115200 波特率串口日志。

随后执行了：

```bash
/home/yhx/Desktop/openvela/myenv/bin/esptool \
  --chip esp32s3 --port /dev/ttyACM0 run
```

芯片开始读取已烧录应用，115200 波特率串口出现：

```text
padd: lma 0x0013061c vma 0x00000000 len 0xf9dc
dmap: lma 0x00140000 vma 0x3c010000 len 0x5b2a4
total segments stored 6
```

这证明已经从单纯的 `waiting for download` 前进到 simple-boot 解析固件段，但之后仍没有 `nsh>` 输出。当前阻塞应表述为“simple-boot 段加载后未观察到 NSH”，仍需物理短按 RESET 后抓取从复位开始的完整日志。

诊断还确认当前 `nuttx/.config` 中实际启用了 `CONFIG_ESP32S3_USBSERIAL`、LCD、SPI2 和 ST7789。构建脚本曾使官方 vendor defconfig 出现一次 `savedefconfig` 精简差异，已再次恢复到官方 `openvela/dev-ai-contest-2026` 版本；当前 `.config` 和已烧录镜像未被删除。

### 物理 RESET 后的真机验收结果

用户松开 BOOT 并短按 RESET 后，开发板成功启动，串口明确出现：

```text
[CPU0] CAM: T1 board_camera_initialize start
[CPU0] CAM: LEDC XCLK 20MHz started on GPIO15
[CPU0] CAM: T7 all done
NuttShell (NSH)
nsh>
```

`help` 的 Builtin Apps 中已经包含：

```text
agentguard  buttons  camera  i2c  leds  lvgldemo  wapi  renew
```

`ls /dev` 真机结果：

```text
accel0  audio/  buttons  console  i2c0  lcd0  null  random
timer0  ttyACM0  userleds  video0  zero
```

`/dev/audio/pcm_in0` 也已注册。`ifconfig` 确认 `wlan0` 为 UP，当前静态地址为 `10.0.0.2/24`，但尚未验证与 PC 的实际连通性。

中文命令解析真机通过：

```text
nsh> agentguard command "开启隐私模式"
command=4
```

相机真机初始化通过：

```text
[CPU0] CAM: OV2640 sensor configured for QVGA RGB565
```

注意：此 BSP 中执行 `camera -h` 并不会显示帮助，而会进入“持续预览且不保存”模式，并占用前台且不响应 Ctrl-C。执行该命令后需要物理 RESET 恢复 NSH；后续不要再用 `camera -h` 探测帮助。

### 第二次复位时的启动期崩溃与 BLE 隔离

再次物理复位后，在尚未执行 `agentguard` 命令时，系统曾在 `hpwork` 高优先级工作队列触发 Xtensa 用户异常：

```text
[BT] HCI ...
xtensa_user_panic: User Exception: EXCCAUSE=001d task: hpwork
PC: 420c599d
```

使用当前 ELF 符号化部分地址：

```text
0x420c599d up_saveusercontext  arch/xtensa/src/common/xtensa_saveusercontext.c:56
0x420bfb2c nx_vsyslog          drivers/syslog/vsyslog.c:277
0x42093a88 work_thread         sched/wqueue/kwork_thread.c:300
```

`up_saveusercontext` 位于 panic/上下文保存路径，因此很可能是异常处理期间看到的地址，并不足以独立证明首个故障点。可以确定的是崩溃发生在 AgentGuard 启动前，最后可辨识的子系统日志属于 Bluetooth HCI，故当前采用“BLE 启动工作项导致崩溃”的诊断假设，而不是声称已经最终定因。

AgentGuard 不需要蓝牙，因此当前 `nuttx/.config` 已做最小隔离：

```text
# CONFIG_ESP32S3_BLE is not set
# CONFIG_ESPRESSIF_BLE is not set
# CONFIG_ESP32S3_WIFI_BT_COEXIST is not set
CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD=y
```

隔离版已重新构建并烧录成功，Flash 哈希校验通过：

```text
原固件：1,684,132 bytes
隔离版：1,477,912 bytes
```

烧录工具的自动 RTS 复位后设备没有自行进入 NSH，仍需物理短按 RESET。只有物理复位后连续启动不再出现 `[BT] HCI`/`hpwork` 异常，才能认为该隔离措施有效。

用户物理 RESET 后，隔离版已通过第一轮真机运行验证：

```text
没有出现 [BT] HCI 或 hpwork 异常
NuttShell (NSH) 正常出现
nsh> agentguard &
agentguard [7:100]
OV2640 sensor configured for QVGA RGB565
AgentGuard started: camera=/dev/video0, PC=192.168.1.100:8080
nsh> pidof agentguard
7
```

AgentGuard 连续运行超过 30 秒后 PID 仍为 7，NSH 保持响应。当前可以认为“关闭 BLE 后启动崩溃消失、AgentGuard 能进入相机循环”获得了首轮证据，但仍需要更多次冷启动和长时间运行才能判断完全稳定。

为保证配置可复现，仓库新增：

```text
tools/apply_agentguard_config.sh
```

该脚本在官方 BSP 已生成 `nuttx/.config` 后启用 AgentGuard、关闭三个 BLE/共存符号并刷新 `config.h`。

PC 网络尚未闭环：电脑当前地址为 `192.168.131.128/24`，开发板显示 `10.0.0.2/24`，固件默认 PC 地址为 `192.168.1.100`，三者不在同一子网。必须获得实际 Wi-Fi SSID/密码并将板子接入电脑所在网络，同时把 AgentGuard PC 地址配置为 `192.168.131.128`（若 DHCP 后电脑地址变化则重新确认）。

进一步执行 `wapi show wlan0` 后确认开发板其实尚未关联任何 AP：

```text
ESSID: <empty>
Flag: WAPI_ESSID_OFF
Mode: WAPI_MODE_MANAGED
AP: ff:ff:ff:ff:ff:ff
```

因此 `10.0.0.2` 只是预设静态地址，不能视为 Wi-Fi 已联网。下一步必须由用户在本机串口中输入实际凭据，避免把密码写入 AI 对话、仓库或日志：

```text
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <实际密码> 3
wapi essid wlan0 <实际SSID> 1
sleep 5
renew wlan0
wapi show wlan0
ifconfig wlan0
```

其中 `3` 表示 CCMP；当前 wapi 在没有额外版本参数时默认 WPA2，这也是本地官方 Wi-Fi 测试文档使用的格式。若实际网络类型不同，需要按 `wapi` 帮助调整。关联成功并获得与电脑可互通的地址后，再把固件中的 PC 代理 IPv4 改为电脑当前地址。

2026-08-11 后续诊断：扫描可以稳定发现目标 2.4 GHz AP（2432 MHz、信号约 -59 至 -64 dBm），说明射频扫描正常；设置 ESSID 后 `WAPI_ESSID_ON`、频率和信号也正常，但 BSSID 持续为 `ff:ff:ff:ff:ff:ff`，收发包为 0，DHCP 失败。因此故障阶段是 WPA 认证/关联，而不是 DHCP。

用户曾误将实际 Wi-Fi 密码发入 AI 对话。本文没有保存该密码，但比赛对话日志可能包含它；必须将该密码视为已经泄露并在路由器上轮换。后续不得在聊天、仓库、命令截图或接力文档中记录新密码。

本地源码确认 `wapi psk wlan0 <密码> 3` 默认选择 WPA2，`3` 选择 CCMP。下一轮建议将路由器 2.4 GHz 安全模式固定为 WPA2-PSK/AES（关闭 WPA3 和 WPA2/WPA3 混合模式），然后用新密码重新配置并通过 `wapi ap wlan0 <扫描到的BSSID>` 显式选择 AP。若仍失败，立即执行 `dmesg` 收集认证失败原因。

显式选择 BSSID 后，Wi-Fi 关联和 DHCP 已成功：

```text
ESSID: Redmi_9D17
AP: 9c:9d:7e:54:e3:f0
wlan0: RUNNING
开发板地址: 192.168.31.188/24
网关: 192.168.31.1
```

开发板 ping `192.168.131.128` 仍为 100% 丢包。主机侧诊断确认 openvela 开发环境运行在 VMware 虚拟机中，虚拟机接口 `ens33` 为 `192.168.131.128/24`，默认网关 `192.168.131.2`，虚拟化类型为 VMware。这是 VMware NAT 私有网段，开发板所在的物理 LAN `192.168.31.0/24` 无法直接访问。

推荐在 VMware 设置中将虚拟机 Network Adapter 从 NAT 改为 Bridged（桥接），使 Ubuntu 也通过路由器获得 `192.168.31.x` 地址。切换后用 `ip -4 -brief address` 获取新地址，再从开发板 ping 新地址。备选方案是在宿主机运行 PC 代理，或配置宿主机端口转发；比赛开发阶段优先桥接，拓扑最简单。

### 2026-08-11 后续真机回归与双网卡拓扑确认

用户确认 VMware 适配器 1 使用 NAT 负责虚拟机上网，适配器 2 使用桥接模式负责连接开发板。Guest 内对应关系为：`ens33` 是 NAT 接口（`192.168.131.128/24`），`ens37` 是桥接接口。`ens37` 是 VMware 提供的 Intel E1000 PCI 虚拟网卡，不是开发板直接枚举出的 USB 网卡。

曾将 `ens37` 临时配置为 `192.168.50.1/24`。开发板此前关联路由器后为 `192.168.31.188/24`，两者不在同一子网，因此该静态配置不能完成现有 Wi-Fi 链路的 PC 闭环。桥接接口最终应从板端所在 LAN 获取 `192.168.31.x/24`，或在确认地址不冲突后配置同网段静态地址；默认路由继续保留在 `ens33`。

开发板再次启动后，串口实测 Wi-Fi 配置未持久化：

```text
wapi show wlan0
IP: 10.0.0.2
ESSID: <empty>
Flag: WAPI_ESSID_OFF
AP: ff:ff:ff:ff:ff:ff
```

此时 ping `192.168.31.1` 的 `sendto` 返回 101，符合未关联 AP、无可用路由的状态。不得把之前一次成功关联误认为重启后仍在线；每次断电或重启后都要重新核对 `wapi show wlan0`。

同一轮串口回归再次确认：

```text
/dev/video0 /dev/lcd0 /dev/buttons /dev/userleds /dev/audio/pcm_in0 均存在
agentguard [9:100]
OV2640 sensor configured for QVGA RGB565
AgentGuard started: camera=/dev/video0, PC=192.168.1.100:8080
pidof agentguard -> 9（约 30 秒后仍存活）
agentguard command "开启隐私模式" -> command=4
```

启动期间未观察到 `[BT] HCI` 或 `hpwork` 异常。主机 C 测试和 PC 代理 4 项单元测试也已再次通过。将 `ens37` 恢复为 DHCP、设置 `ipv4.never-default=yes` 后，NetworkManager 连续发起 45 秒 DHCP 事务但均以 `ip-config-unavailable` 超时，没有获得租约；`ens33` 的 NAT 地址和默认路由保持正常。

用户确认适配器 2 已明确桥接到正确物理网卡后，使用 NetworkManager 的 5 秒 IPv4 DAD 对候选地址进行重复地址检测。`192.168.31.250/24` 未发现冲突并成功启用在 `ens37`，且保留 `ipv4.never-default=yes`。从该接口测试网关：

```text
ping -I ens37 -c 3 -W 2 192.168.31.1
3 packets transmitted, 3 received, 0% packet loss
```

这证明 VMware 桥接二层/三层链路正常，只有 DHCP 租约获取失败。开发板旧地址 `192.168.31.188` 此时 ARP 为 `FAILED`、ping 不可达，与串口所见重启后 ESSID 为空相符。当前 PC 代理可使用 `192.168.31.250` 作为稳定桥接地址；下一阻塞是由用户在串口本地重新输入轮换后的 Wi-Fi 凭据，使板端恢复同网段地址。不得在对话或仓库记录凭据。

## 7. 下一步执行顺序

1. 已完成：物理 RESET 后确认进入 `nsh>`。
2. 已完成：确认 AgentGuard 注册以及相机、LCD、按键、LED、麦克风设备节点。
3. 已完成首轮：BLE 隔离版正常进入 NSH，`agentguard &` 持续运行超过 30 秒且 PID 存活。
4. 后续在 NSH 中继续检查：

   ```text
   help
   ls /dev
   ifconfig
   agentguard command "开启隐私模式"
   ```

5. 配置安全的随机共享 Token 和正确 PC IPv4；不要使用默认 `change-me-before-deployment` 做正式演示。
6. 启动 PC 代理，先验证通知和隐私遮罩，不要立即启用锁屏。
7. 逐项验收相机采集、按键确认、LED、JSONL 和 PC 事件闭环。
8. 后续功能优先级：真实人脸/姿态模型、ESP-SR KWS、LCD 动画、手势调光、HMAC/TLS 防重放。

### 2026-08-12 LittleFS 有界日志与统计

新增独立持久化适配层：

```text
app/agentguard/include/agentguard/storage.h
app/agentguard/src/storage.c
app/agentguard/tests/test_storage.c
```

已实现：

- 本地 JSONL 记录增加 `unix_ms` 外层字段，原有事件 JSON 保持不变；
- 实时时钟有效时保留最近 7 天，启动后每 6 小时整理一次；
- 实时时钟无效时保留未知时间记录，但始终以默认 512 KiB 上限兜底；
- 整理时使用临时文件和 `rename` 替换，损坏/超长行不会进入新文件；
- `agentguard stats` 输出总事件、离座、久坐、姿态、锁屏和确认计数；
- `agentguard prune` 支持手动整理；
- 配置脚本启用官方 `ESP32S3_SPIFLASH_LITTLEFS` 路径，将
  `0x300000..0x3fffff` 挂载为 `/mnt/spif`，避免官方默认 `0x180000`
  紧贴当前固件尾部。

主机严格告警测试（core、vision、storage）、ASan/UBSan 测试、PC 代理 4 项
测试均通过；openvela 交叉编译成功并生成 1,610,544 字节的新镜像，低于
`0x300000`（3,145,728 字节）的存储分区边界。ELF 已确认包含
`board_spiflash_init`、`g_littlefs_operations`、`lfs_mount` 和三个 `ag_log_*`
公开接口。该实现完成时尚未烧录；后续 LittleFS 首次格式化、挂载、跨复位保留
及板上 `stats` 输出的实机验收结果见第 11 节。

## 8. 仓库状态与保护事项

本轮关键新增/修改包括：

```text
README.md
contest2026_123_momohuhuhuluobudui.xml
app/agentguard/
monitor/
skills/
AGENTGUARD_PROGRESS.md
```

仓库中还有先前存在的修改/未跟踪内容，例如：

```text
board/contest_board/configs/nsh/defconfig
board/contest_board/src/board_boot.c
app/face_detection/
ESP32-S3-EYE_Face_Detection_Guide.md
Quick_Start_Face_Detection.md
test_lcd.c
```

这些内容未被本轮擅自删除或覆盖。后续提交前需要人工判断哪些属于有效比赛成果、哪些属于旧实验。

工作区 `.git` 是 repo 工具生成的外部链接，沙箱可能因它跨越可写目录而拒绝执行。此前编辑时只临时改名为 `.git.codex-hold`，补丁后立即恢复；每次操作后都要确认 `.git` 链接存在。

## 9. 常用命令速查

```bash
# 激活工具环境
source /home/yhx/Desktop/openvela/myenv/bin/activate

# 检查 esptool
esptool version

# 主机测试
make -C app/agentguard/tests clean test
python3 -m unittest discover -s monitor -p 'test_*.py' -v

# 配置和构建（从 openvela 根目录）
./build.sh vendor/espressif/boards/esp32s3/esp32s3-eye/configs/openvela -j8
make -C nuttx menuconfig
make -C nuttx EXTRAFLAGS='-Wno-cpp -Wno-deprecated-declarations' -j8

# 烧录
make -C nuttx -j8 flash ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./

# 检查仓库 Skill
./skills/develop-agentguard-openvela/scripts/check_workspace.sh
python3 /home/yhx/.codex/skills/.system/skill-creator/scripts/quick_validate.py \
  skills/develop-agentguard-openvela
```

## 10. 真实性边界

截至本文更新时间，可以确认：需求已拆解、官方 Kconfig 主链已修复、AgentGuard 基础闭环代码已实现、主机测试已通过、openvela 已交叉编译、AgentGuard 已链接进 ELF、ESP32-S3 镜像有效且已写入开发板并通过 Flash 哈希校验。

现在还可以确认：物理 RESET 后 openvela 已进入 NSH；AgentGuard 已注册为内置应用；`video0`、`lcd0`、`buttons`、`userleds`、`pcm_in0` 等关键设备节点已经注册；OV2640 已完成 QVGA RGB565 初始化；中文命令文本解析已在板上执行成功；`wlan0` 已处于 UP 状态。

尚不能确认：AgentGuard 持续采集循环稳定、PC 网络闭环正常、按键/LED/LCD/麦克风数据功能均经过操作级验收、启发式人脸计数达到可用精度、真实 AI 模型已接入、28 条命令已通过麦克风离线识别、LittleFS 首次格式化与跨复位保留已在真机完成、完整比赛演示闭环已验收。

## 11. 2026-08-12 LittleFS 版本实机测试

正式固件已构建并烧录至 `/dev/ttyACM0`，写后哈希校验通过：

```text
镜像大小：1,610,544 bytes
固件擦写范围：0x00000000..0x00189fff
LittleFS 分区：0x00300000..0x003fffff
```

固件范围未触及数据分区。新增 `tools/hw_serial_probe.py`，用于在不依赖
minicom 终端界面的情况下抓取 USB-Serial/JTAG 启动输出及发送 NSH 命令。
自动 RTS、USB-UART 和 esptool watchdog 三种软件复位均能让 ROM 读取固件，
但都停在以下已知 BSP/开发板复位限制处：

```text
rst:0x15 (USB_UART_CHIP_RESET),boot:0x2a (SPI_FAST_FLASH_BOOT)
*** Booting NuttX ***
total segments stored 6
```

将 SPI Flash、MTD 和 LittleFS 全部临时关闭后构建并烧录的二分诊断固件也在
同一位置停止，因此该现象不是新增存储驱动、LittleFS 或 AgentGuard 日志代码
导致。历史实测已经证明此板必须短按实体 RESET 才能越过该状态进入 NSH；诊断
结束后已恢复正式存储配置、重新构建并再次烧录正式固件。

本轮软件回归结果：3 组主机 C 测试通过；ASan/UBSan 通过（容器不支持
LeakSanitizer，使用 `ASAN_OPTIONS=detect_leaks=0`）；PC 代理 4 组测试通过；
工作区结构检查通过；正式 openvela 交叉构建通过。LittleFS 首次格式化、文件
写入、`agentguard stats` 和跨实体复位保留仍需在下一次实体 RESET 后继续验收，
不得提前标记为通过。

### 启动死锁根因与修复

再次实机复位并读取 ROM `Saved PC` 后，地址 `0x403795a9` 和
`0x403795b2` 均符号化到：

```text
spiflash_start() at esp32s3_spiflash.c:319
```

该行位于 SMP SPI Flash guard 等待 `g_flash_op_can_start` 的忙循环。驱动先以
全 CPU affinity 创建两个最高优先级阻塞线程，随后才分别设置 affinity，存在
线程在错误 CPU 上启动并监听错误 per-CPU 信号量的竞态。AgentGuard 不依赖
双核，因此产品配置改为：

```text
# CONFIG_SMP is not set
CONFIG_SMP_NCPUS=1
```

必须同时固定 `SMP_NCPUS=1`；仅关闭 `SMP` 会使旧值 2 残留，并导致单核数组
被双核循环越界访问。修复版烧录后，Saved PC 已变为：

```text
0x42188472 up_idle at esp32s3_idle.c:227
```

证明系统已经离开 Flash guard 死锁并正常进入调度器空闲态。

### LittleFS 与行为日志实机验收结果

修复版实体 RESET 后明确进入 NSH，板级相机初始化 T1 至 T7 全部完成。实机
结果：

```text
/mnt/spif type littlefs
agentguard-events.jsonl  123 bytes
AgentGuard stats: total=1 breaks=0 sedentary=0 posture=0 locks=0 acknowledgements=0 malformed=0
AgentGuard stats: wall clock unavailable in retained records
```

首次空目录上执行 `agentguard prune` 成功，随后启动 AgentGuard：

```text
OV2640 sensor configured for QVGA RGB565
AgentGuard preview: LCD=/dev/lcd0, 240x240 RGB565
AgentGuard started: camera=/dev/video0, PC=192.168.31.250:8080
AgentGuard {"event":"presence_started",...}
```

写入的 JSONL 内容包含 `unix_ms` 包装和完整事件对象。由于开发板尚无有效墙钟，
7 天时间裁剪不会误删记录，并正确输出 wall clock 警告；512 KiB 容量上限仍然
生效。

再次实体 RESET 后，LittleFS 自动重新挂载，123 字节文件仍存在，统计仍为
`total=1, malformed=0`，确认日志跨复位保留。新系统确认无旧 AgentGuard 进程后
重新启动为 PID 7，相机和 LCD 再次初始化成功；稍后 `pidof agentguard` 仍返回
7。至此 LittleFS 挂载、首次整理、事件写入、统计、JSONL 内容、跨复位保留和
AgentGuard 复位后重启均通过实机验收。

## 12. 2026-08-12 LCD 状态界面

为解决用户无法直观看出是否检测到人脸的问题，LCD 已从纯摄像头预览升级为
实时画面加状态 HUD。新增 `display_ui.c/.h` 和独立像素级主机测试，界面显示：

```text
FACE:n    POST:n
CALIBRATING / NO PERSON / MONITORING / MULTI PERSON
POSTURE ALERT / STAND UP / PRESS BOOT TO ACK / PAUSED
SIT:mm:ss PRIV:ON|OFF
```

状态色为红色无人、绿色单人正常、黄色校准、紫色多人/隐私、橙色健康提醒；
视觉候选框同步使用状态色。界面直接绘制进 RGB565 摄像头帧，不依赖额外图片、
字体文件或 LVGL 内存。

4 组 C 测试（core、vision、storage、display UI）全部通过，openvela 交叉编译、
烧录和 Flash 哈希校验通过。实体 RESET 后 OV2640、LCD 初始化成功，AgentGuard
以 PID 6 启动；稍后 `pidof agentguard` 仍返回 6，证明界面运行未破坏采集循环。
实际人数准确率仍受肤色启发式限制，正式模型接入前不得宣称真实人脸识别完成。

## 13. 2026-08-12 ESP-DL 人脸检测接入（进行中）

用户明确要求将肤色连通域启发式人数检测替换为 ESP-DL。当前已完成以下工程
接入：

```text
ESP-DL 版本：v3.2.0
模型：human_face_detect MSR + MNP，S8 量化
模型文件：app/agentguard/third_party/human_face_detect/human_face_detect.espdl
模型大小：191,248 bytes
输入：OV2640 QVGA 320x240 RGB565
推理间隔：每 3 个摄像头帧执行一次模型，其余帧复用最近结果
最多上报：8 张人脸
两级置信度阈值：0.60 / 0.60
```

新增的主要适配文件：

```text
app/agentguard/include/agentguard/vision_model.h
app/agentguard/src/vision_espdl.cpp
app/agentguard/src/espdl_model.S
app/agentguard/src/espdl_compat.cpp
app/agentguard/compat/espdl/
app/agentguard/third_party/esp-dl/
app/agentguard/third_party/human_face_detect/
```

`vision.c` 在 `CONFIG_AGENTGUARD_ESP_DL=y` 时已调用真实 ESP-DL 模型，并继续
依据模型主脸框计算现有相对坐姿分数。LCD 原有 HUD 和主脸框不需要改协议，
`FACE:n`、`MONITORING`、`MULTI PERSON` 将直接使用模型检测数。

为使 ESP-IDF/ESP-DL v3.2.0 在 openvela/NuttX 和当前 Xtensa GCC 工具链上
编译、链接，已完成：NuttX heap/timer/指针类别兼容层、ESP-DL 单核运行适配、
官方 FlatBuffers 模型解析对象接入、GCC 13 `std::string` 冷路径 ABI 兼容、
禁用异常和 RTTI，以及移除不适用的 IDF cache/mbedtls 解密路径。还修复了
`ModuleCreator::get_instance()` 原先使用 guarded static 时触发的
`__cxa_guard_acquire` / pthread ABI 崩溃：在本产品明确单核的前提下改为常量
初始化指针和延迟分配。

完整 openvela 镜像已经多次成功链接、生成和烧录，最近诊断镜像约
1,932,316 bytes，小于 LittleFS 的 `0x300000` 起始边界；esptool 每次均完成
写后哈希校验。主机端 core、vision、storage、display UI 测试和 PC 代理测试
此前均通过，但 ESP-DL 真机路径仍在继续验收，不能仅凭编译成功标记完成。

### 当前实机证据与精确阻塞点

USB Serial/JTAG 在宿主打开串口时会触发 `USB_UART_CHIP_RESET`，而该启动方式
只输出 simple-boot 段信息，运行期 stdout/stderr 不可靠。曾加入 `up_putc()` 和
成功阶段 `fprintf(stderr, ...)` 作为诊断标记，确认这些输出会在 USB 控制台
未就绪时阻塞主任务；相关裸串口标记和成功日志已经移除，不属于产品功能。

为在无可靠串口/JTAG（OpenOCD 被宿主 USB 权限拒绝）的条件下取得可重复证据，
当前源码和已烧录镜像临时包含“阶段看门狗 + 唯一 PC 地址”验收代码。ROM 下次
USB 复位打印的 Saved PC 已依次证明：

1. AgentGuard 完成 LCD 初始化和显示工作线程启动；
2. OV2640 注册、视频流启动和第一帧 `VIDIOC_DQBUF` 均完成；
3. 主任务进入 `ag_vision_process_rgb565()` 和 `HumanFaceDetect` 构造；
4. 90 秒后阶段仍停在模型构造内部，尚未到 `g_detector->run(image)`；
5. 最新一次 Saved PC 为 `ag_acceptance_stop_11`，对应“开始构造
   `HumanFaceDetect`，构造未返回”。

因此当前不能宣称 ESP-DL 人脸推理已经在真机完成。最新诊断固件进一步在
`dl::Model` 构造中细分以下阶段，已成功构建和烧录，正在等待下一轮 Saved PC：

```text
20 进入模型构造 / ModuleCreator 注册前
21 模块注册完成
22 heap 信息读取完成
23 ModelContext 分配完成
24 FlatBuffers 模型加载完成、内存规划前
25 内存规划完成
26 dl::Model 构造完成
```

下一步必须先运行该固件超过 90 秒并读取 Saved PC，确定阻塞在模块注册、模型
解析还是内存规划；随后修复对应兼容问题。真机首次推理返回后，必须删除所有
`ag_acceptance_*` 临时代码和 `espdl_hardware_acceptance_pass()` 无限循环，恢复
正式刷新循环，重新构建烧录，并验收 LCD 帧号持续增长、画面活动、单人不再
误报多人。当前板上是诊断固件，不是最终可交付固件。

上述 20..26 阶段诊断随后已完成。运行 105 秒后 ROM Saved PC 为
`0x4209a8ec`，符号化到 `ag_acceptance_stop_23`。这证明
`ModuleCreator::register_dl_modules()`、两次 heap 查询和 `ModelContext` 分配均
已返回；阻塞范围已缩小到 `Model::load(...)` 的 FlatBuffers 加载/解析过程，
尚未返回到阶段 24，也未执行内存规划。当前正在加入 30..37 子阶段，区分
`FbsLoader` 构造、按模型名解包、`FbsModel::load_map()`、元数据读取、拓扑排序
及算子执行计划创建。

30..37 子阶段在将验收线程提高到优先级 200、超时缩短为 30 秒后得到稳定结果：
Saved PC `0x4209a970` 符号化为 `ag_acceptance_stop_31`。因此
`FbsLoader` 构造已完成，阻塞发生在 `FbsLoader::load(model_name, ...)` 内部。
在未允许同优先级看门狗抢占的一次采样中，主任务 PC 同时落在 ESP32-S3 ROM
`memcpy`（`0x40056f66`）。PDL2 文件头经主机核对正常：2 个模型、两段模型名
长度均为 33、MSR 数据偏移 `0x70`、MNP 数据偏移 `0x1fc20`。当前修复将
`get_model_offset_by_name()` 中临时 `std::string` 构造替换为长度检查加定长
`memcmp`，以避免该 NuttX/libstdc++ 路径中的分配和复制；仍需重新烧录验证。

进一步细分后阶段 61 表明 PDL2 头部和模型名查找均已完成，阻塞实际来自错误
的地址分类：早期兼容层把所有指针都返回为 PSRAM，导致嵌入 Flash DROM 的模型
误入 ESP-DL 的“rodata 已搬到 PSRAM”警告输出；USB 控制台未就绪时该输出会
阻塞。`espdl_compat.cpp` 已改为使用当前镜像 `_image_drom_vma/_size` 精确识别
Flash DROM，并按 ESP32-S3 地址段区分内部 RAM 和非 Flash 的 PSRAM。

修复地址分类后模型构建首次进入真实 AI 指令并触发：

```text
EXCCAUSE=0x23 = EXCCAUSE_CP3_DISABLED
```

ESP32-S3 的 `cop_ai` 是协处理器 3，而官方 BSP 默认
`CONFIG_XTENSA_CP_INITSET=0x0001` 只启用 CP0/FPU。可复现配置脚本现已设置
`CONFIG_XTENSA_CP_INITSET=0x0009`，即为所有 AgentGuard 线程启用 CP0 + CP3。
重建烧录后不再发生 CP3 异常，30 秒 Saved PC 到达 `ag_acceptance_stop_26`，
证明第一阶段 MSR 的 FlatBuffers 加载、执行计划、内存规划和完整 `dl::Model`
构造均已返回。当前剩余阻塞是紧随其后的 `Model::minimize()` 固定警告再次写入
未就绪 USB 控制台；该提示与推理无关，已移除，等待复测第二阶段 MNP 和 run。
