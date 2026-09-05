# AgentGuard 开发上下文与进度接力

> 最后更新：2026-08-19（Asia/Shanghai）
> 工作区：`/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui`  
> openvela 根目录：`/home/yhx/Desktop/openvela`  
> 用途：当 AI 对话上下文被压缩或更换模型时，先阅读本文，再继续开发。本文只记录已有证据，不把启发式算法或编译成功误报为完整 AI/真机功能完成。

## 最新交接（2026-08-19）：100 ms 绝对节拍版已烧录，等待真机反馈

200×150 局部刷新版的真机结果：扫描仍比较明显，但传输耗时已经稳定在：

```text
D:10-20 ms
S:0-10 ms
M:0-10 ms
```

这证明软件调度和分块长尾已基本消除，剩余现象来自 ST7789 面板扫描与 SPI 写入
没有 TE 同步。用户同时报告没有人脸框且显示 `NO PERSON`；代码复核确认框映射
路径存在，当前 `face_count=0` 与此前已知的双 TIE 完整模型数值故障一致，不是
200×150 缩放丢失人脸框。本轮为保持单一变量，没有修改人脸检测。

此前显示线程在每次渲染完成后固定 `usleep(80 ms)`，实际写入起点会随 10-20 ms
传输时间和调度抖动漂移。当前实验改为单调时钟驱动的 100 ms 绝对周期；100 ms
对应常见 60 Hz 面板的 6 帧。每轮只睡到既定目标时刻，超时则跳过错过的目标，
不突发补刷。由于硬件没有 TE 反馈，这只能尝试稳定撕裂位置，不能保证同步。

实现提交：

```text
18f17bd perf: stabilize LCD refresh cadence
```

TDD 与验证证据：

```text
周期调度测试先因实现文件不存在而失败：RED
无渲染漂移、超时跳点、时钟回退测试：PASS
完整主机测试：PASS（17 个 C/C++ 测试二进制）
TIE no-stdio contract：PASS
LCD DMA 配置行为测试：PASS
工作区契约检查：PASS
NuttX ESP32-S3 目标构建：PASS
```

已烧录镜像：

```text
size: 2085644 bytes
SHA-256: ca412c8dee17596ee5f3533d89bd881eea97e0875d94ac66040ae4e2f7bbcee9
esptool image-info: ESP32-S3，checksum 0x57 valid
flash range: 0x00000000..0x001fdfff（低于 LittleFS 0x300000）
flash: Hash of data verified
```

下一步只收集真机证据：扫描分界是否位置更稳定、仍上下移动或变差；扫描总体是
明显、轻微还是看不出；`D/S/M` 是否仍为上述范围；颜色、彩条和重启是否正常。
确认本实验结果后再单独处理人脸检测，建议切换纯 C 卷积并恢复官方 0.50 阈值。

## 上一交接（2026-08-19）：200×150 局部刷新版已烧录

> **下一会话从本节开始。** 下方 A-H 为此前诊断和实验历史。

显示线程提升到 `SCHED_FIFO/110` 后，用户真机反馈扫描已有改善，时序稳定为：

```text
D:30 ms
S:10-30 ms
M:10 ms
```

但扫描仍能看出。复核 ESP32-S3-EYE 官方原理图确认 LCD FPC 没有引出 TE 信号，
无法在应用层实现真正的撕裂同步；240×240 RGB565 全帧为 115,200 字节，40 MHz
SPI 的纯线速下限约 23 ms。用户批准降低每次动态传输像素，而不冒险把 LCD 时钟
提高到 80 MHz。

当前实现将完整 320×240 相机画面按最近邻缩放为居中的 200×150 预览，保留 4:3
视野；预览坐标为 `x=20, y=37`。人脸框同步缩放、外扩取整并裁剪。顶部 18 行和
底部 34 行 HUD 使用快照比较，只传输实际变化的最小包围矩形；预览区域仍每轮
更新。动态预览数据从 115,200 字节降到 60,000 字节，40 MHz 纯线速约 12 ms。
保持 16 行内部 DMA bounce、40 MHz SPI、RGB565 字节序、80 ms 刷新周期和显示
线程优先级 110 不变。

实现提交：

```text
5751273 perf: shrink live LCD update region
```

验证证据：

```text
工作区契约检查：PASS
完整主机测试：PASS（16 个 C/C++ 测试二进制）
TIE no-stdio contract：PASS
LCD DMA 配置行为测试：PASS
NuttX ESP32-S3 目标构建：PASS
git diff --cached --check：PASS
```

已烧录镜像：

```text
size: 2085644 bytes
SHA-256: 538984fa6c973a1d86a142ca6803510f88c5c8be05c50772dd4c58a192c5915c
esptool image-info: ESP32-S3，checksum 0x20 valid
flash range: 0x00000000..0x001fdfff（低于 LittleFS 0x300000）
flash: Hash of data verified
```

下一步只需用户观察 10-20 秒并反馈：扫描是“看不出来 / 轻微 / 仍明显”；
`D/S/M` 常见值及最高值；`C/Q/L` 范围；预览布局和人脸框是否正确；颜色、彩条、
启动与重启是否正常。没有真机反馈前，不得宣称扫描问题已经修复。

## 历史交接（2026-08-18）：LCD 扫描刷新优化进行中

> **下一会话从本节开始。** 本节覆盖下方较早的“白屏/诊断固件”等历史状态。
> 当前主线约束是：**尽量不要修改 NuttX 基础代码；优先只修改比赛仓库内的
> AgentGuard 应用、测试和可复现配置脚本。** 用户已经批准当前应用层方案。

### A. 已完成并经真机确认的 LCD 修复

此前先启用 ESP32-S3 SPI DMA，再使用内部 RAM 分块缓冲，解决了 PSRAM 直接 DMA
产生的彩色横条。随后确认 ESP32-S3 的 16 位 polling SPI 会交换字节，而 DMA
发送原始内存；因此在内部 bounce 缓冲中显式交换 RGB565 高低字节。

用户对当前已烧录的 **8 行 bounce + RGB565 字节序修复版** 给出的真机结论：

```text
红色恢复；摄像头与 UI 颜色正常；彩色横条消失；画面几何连续。
C:60-140，Q:40-90，L:60-90，D:70-210。
仍存在明显的上下扫描刷新。
```

对应已提交历史：

```text
beb1a26 feat: accelerate LCD writes with coherent SPI DMA
15181fb docs: design internal LCD DMA bounce
b794e13 docs: plan internal LCD DMA bounce
e761ff9 feat: add chunked LCD bounce controller
4b91bd8 fix: bounce LCD DMA through internal RAM
caffccf docs: design LCD DMA RGB565 byte order fix
bd12db1 docs: plan LCD DMA RGB565 byte order fix
e0d3762 fix: correct RGB565 byte order for LCD DMA
```

8 行字节序修复版最后一次明确烧录的镜像 SHA-256：

```text
ed2ec3658a9ee46995aa56c65b0b19454c6ad0549e226043463647ab598eb0ac
```

### B. 扫描现象的当前判断与用户批准方案

8 行缓冲会把 240 行画面拆成 30 次连续 `LCDDEVIO_PUTAREA`。ST7789 没有第二帧
缓冲，逐带写入会在屏幕上形成可见的上下扫描。完整 240x240x2 内部帧缓冲需要
115,200 字节，当前内部 RAM 不足，不应尝试。

用户已批准只做应用层的保守优化：

```text
AG_LCD_BOUNCE_ROWS:             8 -> 16
内部 bounce 大小:              3840 -> 7680 bytes
CONFIG_ESP32S3_SPI_DMA_BUFSIZE: 2048 -> 7680
CONFIG_ESP32S3_SPI_DMATHRESHOLD: 保持 64
每帧 PUTAREA 次数:              30 -> 15
```

预期只是使扫描更快、较不明显，**不能在真机确认前宣称扫描已消失**。不得为此修改
NuttX LCD/SPI 驱动基础代码；若效果不足，应先收集真机数据，再与用户讨论下一步。

### C. 当前未提交修改（只涉及 3 个文件）

```text
app/agentguard/src/agentguard_main.c
  AG_LCD_BOUNCE_ROWS 从 8 改为 16

app/agentguard/tests/test_lcd_dma_config.py
  SPI DMA BUFSIZE 契约从 2048 改为 7680

tools/apply_agentguard_config.sh
  生成 CONFIG_ESP32S3_SPI_DMA_BUFSIZE=7680
```

不要暂存或恢复其他脏文件；它们属于用户或此前工作的既有状态。真实 Git 目录仍为：

```text
/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git
```

所有 Git 命令继续显式使用 `--git-dir` 和 `--work-tree`。`.git.codex-hold` 不要删除。

### D. 本轮已完成的测试与构建证据

采用测试先行：配置测试先因仍生成 `2048` 按预期失败；旧 ELF 的 bounce 符号大小
仍为 `0x0f00`，集成边界检查也按预期失败。实现后：

```text
test_lcd_dma_config.py: PASS
完整主机测试：PASS
  core / vision / storage / display UI / frame timing
  lcd timing / lcd transfer / lcd bounce
  ESP-DL backend config / alloc policy / private pool / TIE selftest
TIE no-stdio contract: PASS
NuttX 目标构建：PASS
```

应用配置脚本后，外部 NuttX `.config` 已核对为：

```text
CONFIG_ESP32S3_SPI_DMA=y
CONFIG_ESP32S3_SPI_DMA_BUFSIZE=7680
CONFIG_ESP32S3_SPI_DMATHRESHOLD=64
```

最新目标 ELF/Map 证据：

```text
3fc9a7c0 00001e00 b g_agentguard_lcd_bounce
_sheap = 0x3fcc884c
到内部 DRAM 末端 0x3fcd0000 尚余约 30,644 bytes（0x77b4）
```

当前磁盘构建产物（**尚未烧录**）：

```text
/home/yhx/Desktop/openvela/nuttx/nuttx.bin
SHA-256: 67fb6cc3437ad609be6ebfc6d1dc452ef6fa015ec1cc70d6b9f20da0b1d14483
```

### E. 下一会话精确执行顺序

1. 先阅读本节并核对上述 3 个文件的 diff；不要修改 NuttX 基础代码。
2. 对当前产物执行 `git diff --check`、esptool `image-info` 和镜像边界检查。
3. 只暂存上述 3 个文件并提交，建议提交信息：
   `perf: double LCD DMA bounce rows`。
4. 检查 `/dev/ttyACM0` 和 USB 设备，然后烧录当前 16 行版本；等待 esptool 输出
   `Hash of data verified`，并记录实际烧录镜像 SHA-256。
5. 让用户实体观察并反馈：是否正常启动、颜色是否仍正常、有无彩色横条、
   `C/Q/L/D` 当前范围、扫描是否比 8 行版本更快/更不明显、是否有重启或不稳定。
6. 只有用户真机证据确认后，才决定是否结束优化或讨论更大的应用层分块；不要
   自动继续增大，也不要把“编译通过”表述成“扫描修复”。

注意：`tools/hw_serial_probe.py` 打开 USB 串口会触发该板已知的软件复位，并可能
只停留在 ROM/simple-boot 日志；本阶段没有必要重复串口探测。正常烧录会再次复位
设备，若之后需要实体 RESET，再明确请用户操作。

### F. 16 行版本真机结果与根因收敛

16 行版本已提交并烧录：

```text
13b7a3c perf: double LCD DMA bounce rows
```

用户真机反馈：颜色正常、没有彩色横条；扫描比 8 行版本更快，但仍明显影响观看；
时序范围为：

```text
C:60-130 Q:40-70 L:60-100 D:40-160
```

本地完整路径复核确认：16 行连续缓冲在 ST7789 `putarea()` 中确实作为一次
`SPI_SNDBLOCK` 发送，因此整帧为 15 次 DMA，而不是驱动内再次拆成 240 行。
40 MHz 下 115,200 字节的纯线速下限约 23 ms；`D:40-160` 表明除了没有帧交换、
边写边显示的物理扫描外，15 次提交中还存在约 17-137 ms 的调用或调度尾延迟。

### G. D/S/M 分解诊断固件已烧录，等待 LCD 读数

用户已批准只加入时序诊断，不改变 16 行缓冲、DMA、颜色字节序、刷新周期或线程
优先级。HUD 详情行暂时显示：

```text
D:<完整帧总耗时> S:<15 次 PUTAREA 耗时之和> M:<最慢一次 PUTAREA 耗时>
```

实现采用 TDD：聚合测试先因 API 不存在而失败，HUD 测试先因状态字段不存在而
失败；实现后全部 12 个主机二进制测试、TIE no-stdio 契约和 DMA 配置行为测试
通过，NuttX 目标构建通过。诊断提交：

```text
8a0b085 feat: display LCD DMA timing breakdown
```

已烧录镜像证据：

```text
size: 2085636 bytes
SHA-256: 8c095fe6bd2b14f9f4814d8bc57d6f3a107a0904e91c3cd43390d1c3695816dd
esptool image-info: ESP32-S3，checksum valid
flash range: 0x00000000..0x001fdfff（低于 LittleFS 0x300000）
flash: Hash of data verified
```

下一步只需让用户观察若干秒并报告 `D/S/M` 的常见范围和偶发高值。解释规则：

- `M` 偶发很高且接近 `D` 的尖峰：单笔 DMA 完成后的调度尾延迟占主导；
- `S` 相对稳定但 `D-S` 很大：分块复制、cache clean 或调用间开销占主导；
- `D/S/M` 都低且稳定但扫描仍明显：已接近 40 MHz SPI 和 ST7789 无帧交换的
  物理限制，应与用户讨论降低传输像素、谨慎提高 SPI 时钟或扩大底层修改范围。

### H. D/S/M 真机结果与显示线程优先级实验

诊断固件真机观测结果：扫描观感依旧不好；常见范围和最高值为：

```text
D: 30-160，最高 260
S: 20-200，最高 270
M: 10-90，最高 90
```

系统 tick 为 10 ms，因此 `D/S/M` 均有 10 ms 量化误差，`S` 偶尔略大于 `D`
不代表实际子项超过总耗时。`M` 可达 90 ms 则说明单次 16 行 `PUTAREA` 的
DMA 完成唤醒存在明显长尾。调度路径复核发现：显示线程原先继承主线程优先级
100，camera/LPWORK 同为 100，RR 时间片为 200 ms；SPI DMA ISR 释放信号量后，
显示线程可能不能立即从同优先级工作中取得 CPU。

用户批准最小化实验：只把显示线程设置为显式 `SCHED_FIFO` 优先级 110；保持
camera/LPWORK 为 100、HPWORK 为 224，且不改变 16 行缓冲、DMA、SPI 40 MHz、
颜色字节序和刷新周期。实现采用真实 pthread 属性的主机测试，并通过全部 13 个
C/C++ 测试、TIE no-stdio 契约和 DMA 配置行为测试；NuttX 目标构建通过。提交：

```text
134dae7 perf: prioritize LCD DMA continuation
```

已烧录镜像证据：

```text
size: 2085636 bytes
SHA-256: af39b3f542a66bca529475a017f43b74b2b2e1da3bfa328084f918a5802dec8c
esptool image-info: ESP32-S3，checksum valid
flash range: 0x00000000..0x001fdfff（低于 LittleFS 0x300000）
flash: Hash of data verified
```

下一步必须由用户真机观察 10-20 秒，比较扫描是否改善，并报告新的 `D/S/M`、
`C/Q/L` 常见范围和最高值，同时确认颜色、彩条、启动与重启情况。不能在获得
真机反馈前宣称扫描问题已修复。

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

### 0.6 方案 A（历史计划，已在 0.8 实现）

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

### 0.7 当时不可宣称事项（由 0.8 和 0.9 更新）

- 当前 LCD 是白屏，摄像头没有可见画面；不能宣称应用可用。
- 尚未取得 RAM/Flash 自检的 `K:xy`，不能宣称已定位到 Flash 权重。
- 私有内部池已证明没有修复 TIE 数值，不得把它描述为最终修复。
- TIE 卷积仍是主要故障，纯 C 路径虽有较合理信号但约 600 ms，且参考图最高分
  约 57、尚未完成最终阈值和真人精度验收。

### 0.8 2026-08-17：TIE 阶段状态诊断固件已烧录，等待 LCD 读数

为避免同步 TIE 自检阻塞时 LCD 停留在控制器默认白屏，启动顺序现已调整为：
打开 LCD、创建并启动显示线程、等待两个刷新周期，然后才在 AgentGuard 主线程
执行自检。两次直接 TIE 调用仍保留原任务和协处理器上下文，没有移动到新线程。

跨线程状态使用 C++ 原子变量和 C 查询接口发布，LCD 详情行的含义为：

```text
D:40       RAM 权重 TIE 调用即将执行或尚未返回
D:41       RAM 调用已返回，Flash/DROM 权重 TIE 调用即将执行或尚未返回
D:42 K:xy  两次调用均返回；x/y 仍分别表示 RAM/Flash 是否通过
```

相关提交：

```text
bb70b98 docs: design TIE progress display
ed64981 docs: plan TIE progress display
00ea948 feat: display TIE self-test progress
9bf559d fix: keep LCD alive during TIE diagnostic
```

TDD 证据：`test_display_ui` 在新接口未实现时以
`implicit declaration of function ag_ui_format_tie_progress` 按预期失败；实现后
8 个主机测试全部通过。目标消费路径在 getter 未实现时以 undefined reference
按预期链接失败；加入原子实现后 NuttX 构建和镜像生成成功，且没有未解析的
atomic helper 符号。

ELF/objdump 证据：

```text
ag_display_worker_main                      0x420ba6e4
ag_run                                      0x420ba9a0
ag_espdl_tie_conv_selftest_run_once         0x420bed0c
ag_espdl_tie_conv_selftest_get_stage        0x420befd0
ag_espdl_tie_conv_selftest_get_result       0x420befe4
RAM filter                                  0x3fc9a820
Flash/DROM filter                           0x3c011750
```

反汇编确认显示线程先完成 `pthread_detach`，随后 `usleep` 两个刷新周期，再由
`ag_run` 调用 `run_once`；显示线程每轮调用 `get_stage` 和 `get_result`。

烧录镜像和最终复验构建证据：

```text
nuttx.bin size: 2085828 bytes（低于 LittleFS 0x300000 边界）
已烧录镜像 SHA-256:
756fec49859da1a0f72acee4b434fba6600d548ed3d95916398f85e215b8e237
esptool image-info: ESP32-S3，checksum valid
flash: Wrote 2085828 bytes；Hash of data verified
烧录后的同源码复验构建 SHA-256:
bc881b8bf0ff7346489005d38f413941e3763448eb4b7896101554c1f2d0f47b
```

NuttX 构建会更新镜像内的构建信息，因此同源码复验构建的二进制哈希不同。复验
构建完成后，已烧录镜像通过写后校验；软件复位随后使 `/dev/ttyACM0` 按此板的
已知限制消失，复验镜像未重复烧录。这不改变两次镜像所含的本轮源码，但后续若
需要逐字节对齐当前磁盘产物，应在实体 RESET 恢复 USB 设备后再次烧录。

当前仍不可宣称 TIE 已修复或人脸检测可用。软件烧录后的实体 LCD 读数尚未取得；
下一步只需在必要时短按 RESET，并报告可见的 `D:`、`K:`（若有）和 `CAM:`。

### 0.9 2026-08-17：移除自检完成后的阻塞 stdio，已烧录待复测

0.8 诊断固件实体 RESET 后，用户报告：

```text
D:42 K:11 CAM:0
```

这组结果证明 RAM 与 Flash/DROM 权重的两次最小 TIE 卷积都返回，且 16 个输出
均与标量参考一致。它否定了“直接 TIE 内核不返回”和“最小 Flash/DROM 权重读取
错误”两个假设，但只覆盖该 16x16、1x1 最小夹具，不能据此宣称完整人脸模型的
所有卷积都正确。

`D:42` 发布后到 `run_once()` 返回之间唯一的实质路径是一串
`lib_get_stream`、`fprintf`、`fputc` 和 `fwrite`。此前真机已经多次证明未就绪
USB 控制台会阻塞同步输出；`CAM:0` 又证明主线程尚未进入随后的视频打开阶段。
因此本轮根因定位为：**自检已经成功完成，但完成后的 stderr 向量日志阻塞了
AgentGuard 主线程。**

相关提交：

```text
31b4ca7 docs: design no-stdio TIE startup
bfb84b3 docs: plan no-stdio TIE startup
a788228 fix: remove blocking TIE startup logging
```

新增 `app/agentguard/tests/test_espdl_tie_no_stdio.py`，永久禁止启动自检源文件
重新引入 `<cstdio>`、stderr/stdout、`fprintf`、`printf`、`fputc`、`fwrite` 或
`print_vector`。该契约在修改前按预期失败并列出当前违例；移除日志后转为通过，
并已加入完整主机测试配方。

最终软件复验证据：

```text
8 个原有主机测试：PASS
TIE no-stdio contract：PASS
NuttX target build：PASS
run_once_stdio_refs=0
RAM filter:        0x3fc9a820
Flash/DROM filter: 0x3c0116c0
workspace checker: 全部 OK
git diff --check: 无输出
```

已直接烧录捕获后的固定镜像，没有在烧录命令中触发重建：

```text
size: 2085572 bytes
flashed SHA-256:
c1996618c93c086338a368fd85a515ac6d5db2f63ed0a9352e04811eca4ab73c
esptool image-info: ESP32-S3，checksum valid
flash: Wrote 2085572 bytes；Hash of data verified
post-flash verification-build SHA-256:
200d34d5268a176cfccf7f9ee1b7cd89e150ac1b34d3a94897a8ee3b91813f4b
```

两个哈希不同仅因为 NuttX 复验构建更新了嵌入的构建信息；板上明确写入并校验的
是 `c199...ab73c`。下一步实体 RESET 后应继续看到 `D:42 K:11`，同时
`CAM:` 应推进到大于 0 的阶段。取得该读数前，不能宣称此启动阻塞已经完成真机
闭环；即使 `CAM:` 推进，也仍不能宣称完整模型 TIE 数值或人脸检测精度正确。

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

## 14. 2026-08-19 LCD 80 MHz 单变量实验

提交 `18f17bd` 的 100 ms 绝对周期刷新在实机上没有锁定扫描相位：扫描线仍会上下
移动，只是频率改变。这说明 LCD 控制器扫描与主机 `PUTAREA` 之间没有共同的相位
基准，开环软件定时不能实现真正的时钟同步，因此本轮撤掉该调度模块，恢复每次
绘制完成后休眠 80 ms。

Espressif 官方 ESP32-S3-EYE BSP 将 ST7789 像素时钟配置为 80 MHz，并使用 DMA
队列显示；当前 NuttX 配置此前只有 40 MHz。已在提交 `c37bbe5` 中把
`CONFIG_LCD_ST7789_FREQUENCY` 固定为 `80000000`，保留现有 200x150 预览、
16 行 DMA bounce buffer、显示线程优先级、颜色字节序及人脸模型不变，以便单独
判断传输时间减半对扫描观感的影响。配置回归测试、全部 AgentGuard 主机测试、
工作区检查和目标固件构建均通过。

最终固件 `nuttx.bin` 为 2,085,644 bytes，SHA-256：
`b9ceab72cd1c7e40648c74c0b55dd2540478a6323116d4de6fc3d04e370131b5`。
已通过 `/dev/ttyACM0` 烧录，esptool 报告 `Hash of data verified` 并硬复位。
下一步等待实机反馈：扫描线不可见/轻微/明显、D/S/M 常见值与峰值、颜色和几何
是否正常、画面响应及持续运行稳定性。`NO PERSON` 仍是独立的 ESP-DL 双 TIE
卷积数值问题，本轮有意未修改。

### 80 MHz 实机反馈与下一步调查结论

用户反馈：80 MHz 版本的扫描线比 40 MHz 版本好一些，但观感仍然不好；当前
诊断值为 `D:10-20`、`S:0-10`、`M:0-10`。这说明提高 SPI 时钟有效缩短了
可见写入时间，但没有消除撕裂，继续微调 80/100 ms 刷新周期只能改变扫描线
移动频率，不能实现同步。

只读代码调查得到以下证据：

1. 当前 200x150 预览使用 16 行内部 bounce buffer，每帧约拆成 10 次同步
   `LCDDEVIO_PUTAREA`；每块之间 CPU 还要从 PSRAM 复制并交换 RGB565 字节，
   ST7789 驱动会重新发送 `CASET/RASET/RAMWR`，因此 SPI 数据流存在块间空隙。
2. NuttX `esp32s3_spi_dma_exchange()` 会等待每次 DMA 完成后才返回，应用层目前
   没有异步提交队列。`S/M` 已很低而扫描仍明显，也支持“同步分块/撕裂”而非
   单纯总带宽不足这一判断。
3. Espressif 官方 ESP32-S3-EYE BSP 使用 80 MHz、DMA-capable 内部绘制缓冲和
   `trans_queue_depth=10` 的异步 LCD 传输队列；官方摄像头示例用两个摄像头
   canvas buffer，经 LVGL invalidation 刷新。
4. ESP32-S3-EYE 板级引脚只连接 LCD PCLK/MOSI/DC/CS/背光，没有连接 ST7789
   TE（Tearing Effect）输出，因此无法直接用硬件垂直同步信号锁相。

下一轮不要再盲调刷新周期。应先在以下两个单变量方案中选择：

- 低风险验证：把内部 bounce buffer 从 16 行提高到 32 或 48 行，并相应增加
  `CONFIG_ESP32S3_SPI_DMA_BUFSIZE`，减少独立 `PUTAREA` 次数；预期只能进一步
  改善，未必完全消除扫描线，且需先通过链接确认内部 DRAM 余量。
- 更接近官方的正式方向：在应用/NuttX 显示边界实现双内部 bounce buffer 加
  连续异步 DMA 队列，使 CPU 准备下一块与当前 DMA 重叠，并避免块间断流。
  这是跨层改动，实施前需要单独设计和批准；若仍无 TE，理论上只能把撕裂压到
  肉眼难察觉，不能保证严格的逐帧垂直同步。

建议下一次会话先做 32/48 行内部缓冲的可构建性与 DRAM 边界验证，作为最小
假设实验；保持 80 MHz、200x150 预览、人脸模型和刷新间隔不变。若改善不足，
停止继续堆叠参数修改，转入异步双缓冲显示架构设计。

### 32 行 LCD DMA 批次实验（已实施并烧录）

用户已批准先实施低风险的 16→32 行单变量实验。本轮保持 LCD 80 MHz、
200x150 预览、刷新间隔、人脸模型及颜色字节序不变，只把 SPI DMA 缓冲从
7,680 bytes 增加到 15,360 bytes，使 240 像素宽的内部 bounce buffer 从 16 行
增加到 32 行，预览每帧的同步 `LCDDEVIO_PUTAREA` 批次数约由 10 次降为 5 次。

实现不再硬编码行数：`AG_LCD_BOUNCE_PIXELS` 由
`CONFIG_ESP32S3_SPI_DMA_BUFSIZE / sizeof(uint16_t)` 推导，行数再由 LCD 宽度
推导，并加入静态断言，防止 DMA 缓冲不是完整 LCD 行。配置回归测试先按 TDD
得到 7,680≠15,360 的预期失败，修改后转绿；全部 AgentGuard 主机测试、工作区
检查、目标构建及 `git diff --check` 均通过。

链接后 `g_agentguard_lcd_bounce` 为 15,360 bytes，`.dram0.bss` 为 214,664
bytes，`_sheap=0x3fcca68c`，未越过内部 DRAM 边界。代码提交为 `af04e77
perf: double LCD DMA transfer rows`。最终 `nuttx.bin` 为 2,085,644 bytes，
SHA-256：`4e960ab96ae55a9ab67403f9a4150fde8a81c7b486428acd1c1dc3c0ec179d71`。
已通过 `/dev/ttyACM0` 烧录，esptool 报告 `Hash of data verified` 并硬复位；
烧录后的配置复核为 `CONFIG_LCD_ST7789_FREQUENCY=80000000`、
`CONFIG_ESP32S3_SPI_DMA_BUFSIZE=15360`。

下一步等待实机反馈：扫描线不可见/轻微/明显，D/S/M 常见值和峰值，颜色、横条、
画面响应及持续运行稳定性。`NO PERSON` 属于独立的 ESP-DL 双 TIE 数值问题，
本轮预期不变。若 32 行仍不足，不再继续试 48 行或调刷新周期，直接进入双内部
bounce buffer 加连续异步 DMA 队列的架构设计。

### PSRAM 单次连续 DMA 低刷新率方案（已实施并烧录）

32 行版本的实机反馈仍为扫描线明显，诊断值 `D:10-20`、`S:0-10`、
`M:0-10`。用户随后明确同意优先降低到 5–10 FPS，并批准方案 1：将完整
200x150 RGB565 预览放进一块 64 字节对齐的 PSRAM DMA 缓冲，每次预览只执行
一次 60,000-byte `LCDDEVIO_PUTAREA`；保持 ST7789 80 MHz，显示节拍改为
125 ms（约 7–8 FPS）。颜色字节序、预览几何、人脸模型、HUD 与 `NO PERSON`
逻辑均未改动。

内部 DRAM 方案在实施前已被否决：32 行版本 `_sheap=0x3fcca68c`，距内部 DRAM
末端 `0x3fcd0000` 仅 22,900 bytes，而把静态 bounce 从 15,360 扩大到
60,000 bytes 还需 44,640 bytes。最终实现删除静态内部 bounce，改为启动显示
线程前用 `memalign(64, 65536)` 分配 PSRAM，并只开放其中 60,000 bytes /
30,000 pixels 给提交路径；分配失败、未对齐或地址不在 ESP32-S3 外部 RAM
`0x3c000000..0x3dffffff` 时拒绝启动并释放资源。

为允许 SPI2 TX DMA 从 PSRAM 读取，openvela/NuttX 官方源码仅修改：

```text
/home/yhx/Desktop/openvela/nuttx/arch/xtensa/src/esp32s3/esp32s3_spi.c
函数：esp32s3_spi_dma_init()
行为：DMA channel 申请成功后，将 TX external-memory block 配置为 64 bytes
本地提交：7ac22d18890 esp32s3/spi: configure PSRAM TX DMA block size
状态：detached HEAD，仅本地提交，绝对未 push；push 前必须由 openvela 官方检查
回滚：官方审核并建立可引用分支后执行 git revert 7ac22d18890
```

该调用与同树 `esp32s3_lcd.c` 的 PSRAM framebuffer TX 配置一致，不改变公开 API。
NuttX 工作树中另有 `esp32s3_cam.c`、`Make.defs`、V4L2 与 Wi-Fi defconfig 等既存
用户修改，本轮未包含、未清理、未提交。

竞赛仓库实现与文档提交：

```text
679689d docs: design continuous LCD preview DMA
05730cc docs: move continuous LCD DMA buffer to PSRAM
0d557f8 docs: plan PSRAM continuous LCD DMA
9c78895 docs: record inline DMA execution constraints
8898534 feat: validate LCD PSRAM DMA buffer
3290a99 refactor: size LCD bounce batches by capacity
e2bb921 perf: send LCD preview from PSRAM DMA
```

应用修改严格按 TDD 完成：PSRAM 缓冲生命周期测试先因实现文件不存在而 RED，
实现对齐/外部地址/失败释放后 GREEN；bounce 容量测试先在 30,000 pixels 的
单次预览断言失败，改为按像素容量计算批次后 GREEN；DMA 配置测试先得到
`15360 != 60000`，修改配置脚本后 GREEN。2026-08-20 最终重新执行完整主机
测试，core、vision、storage、显示、LCD、ESP-DL 兼容层、TIE no-stdio 和 DMA
配置测试全部 PASS；工作区检查 6 项全部 OK，两个仓库 `git diff --check` 通过。

目标镜像验证结果：旧 `g_agentguard_lcd_bounce` 符号已消失；SPI2 RX/TX DMA
描述符数组各 `0xb4` bytes，即各 15 个 12-byte descriptor；
`.dram0.bss=199568` bytes，`_sheap=0x3fcc6b8c`，比 32 行版本释放约 15 KiB
内部 DRAM。烧录配置为：

```text
CONFIG_LCD_ST7789_FREQUENCY=80000000
CONFIG_ESP32S3_SPI_DMA_BUFSIZE=60000
```

最终 `nuttx.bin` 为 2,085,660 bytes，SHA-256：
`8d29851c4f8cd72364f41bcf4b8a8cf3fb60dd6efb861dd4e4ae3154d4a2c2c0`。
esptool 识别 ESP32-S3 rev 0.2 和 8 MB PSRAM，完成全部写入，报告
`Hash of data verified` 并通过 RTS 硬复位。烧录后没有打开串口，避免 USB
控制线再次触发 simple-boot/reset。

本轮只能确认代码、镜像与烧录链路，不能在没有肉眼反馈时宣称扫描线已消失。
下一步请观察 10–20 秒并反馈：扫描线“看不出来 / 轻微 / 仍明显”，颜色与几何
是否正常、有无彩色横条、画面响应是否可接受，以及新的 D/S/M 常见值和峰值。
`NO PERSON` 仍是独立的 ESP-DL 双 TIE 数值问题，不属于本次显示 DMA 修改。

### PSRAM TX 描述符对齐修复（已实施并烧录，等待实机确认）

上一版 PSRAM 单次 DMA 固件的实机结果为：整屏彩色条、没有文字，且画面只能
显示一半。这个结果否定了“只设置 GDMA TX external-memory block 为 64 bytes
即可复用通用描述符切分”的假设。

根因已从本地源码确定：`esp32s3_dma_setup()` 的通用 TX 路径仍按每 descriptor
最多 4095 bytes 切分。第一个 PSRAM 地址虽然 64-byte 对齐，但后续地址依次增加
4095，第二段开始即失去 64-byte 对齐；同时 buffer length 也没有向 64 bytes
补齐。这与彩条、文字同样损坏和长链半屏截断现象一致。

最初考虑修改通用 `esp32s3_dma_setup()`，但安全审查指出这会扩大到摄像头、SPI
RX 和所有 GDMA 用户，因此没有实施。最终只修改 SPI TX 的 PSRAM 分支：

```text
生产文件：arch/xtensa/src/esp32s3/esp32s3_spi.c
新增私有头：arch/xtensa/src/esp32s3/esp32s3_spi_psram_dma.h
内部 RAM TX：继续调用原 esp32s3_dma_setup()，行为不变
PSRAM TX：每 descriptor 最多 4032 bytes，起始地址始终 64-byte 对齐
末段：data length=3552，buffer length=3584，总有效数据仍为 60000 bytes
摄像头、SPI RX、通用 DMA：完全未修改
```

TDD 证据：先新增真实描述符测试，因生产头不存在而 RED；实现 SPI 专用绑定后
GREEN。测试逐个检查 15 个 descriptor 地址均可被 64 整除，并检查首段
4032/4032、末段 3552/3584、EOF、链尾与总有效长度 60,000。NuttX `nxstyle`
检查通过；随后完整 AgentGuard 主机测试（含该新测试）、TIE no-stdio、DMA 配置
测试及工作区 6 项检查全部通过，目标交叉编译通过。

新增提交：

```text
竞赛仓库：aecc037 test: cover aligned PSRAM SPI DMA descriptors
NuttX 本地：79eb108d07d esp32s3/spi: align PSRAM TX DMA descriptors
前置 NuttX：7ac22d18890 esp32s3/spi: configure PSRAM TX DMA block size
```

两个 NuttX 提交均位于 detached HEAD，仅存在本地，没有任何远端引用，绝对未
push；提交官方前必须由 openvela 官方检查。若官方审核要求整体撤销，按新到旧
顺序执行 `git revert 79eb108d07d`、`git revert 7ac22d18890`。NuttX 树内其他
既存用户修改仍未触碰、未提交。

2026-08-20 修复版 `nuttx.bin` 为 2,085,660 bytes，SHA-256：
`c783befe6ee84e818c2e842dc3825a6fef1a48d2c1a239e9d4656a92ef21ef75`；
esptool 镜像 checksum 有效，实机写入后报告 `Hash of data verified` 并硬复位。
当前只证明测试、构建、镜像和烧录链路，尚不能证明彩条/半屏已在实机消失。
下一步先观察颜色、文字与全屏画面；若仍有任一彩条或截断，立即回退到已知颜色
正常的 32 行内部 RAM bounce 版本，不再继续试探 PSRAM 直传。

### PSRAM 直传实验终止并回退到 32 行内部 RAM（已烧录）

描述符 64-byte 对齐修复后的实机反馈仍然失败：文字依旧显示为彩条，摄像头画面
依旧只显示上半部分。这证明问题不只是 descriptor 起始地址/长度对齐；当前
NuttX SPI、cache、GDMA 与 ST7789 组合下的 PSRAM 直接 TX 路径存在更深层限制。
连续两次单变量方案均失败，因此按照事先批准的退出条件停止该架构，不再继续用
实机试探 PSRAM 直传。

已使用可追溯回退恢复 `af04e77` 的关键显示实现：15,360-byte 静态内部 RAM
bounce、32 行一批、200x150 预览、80 MHz SPI、80 ms 显示休眠。恢复后镜像中：

```text
g_agentguard_lcd_bounce = 0x3c00 = 15360 bytes
SPI2 TX/RX descriptor array = 0x30 bytes，各 4 个 descriptor
_sheap = 0x3fcca68c
CONFIG_LCD_ST7789_FREQUENCY=80000000
CONFIG_ESP32S3_SPI_DMA_BUFSIZE=15360
```

竞赛仓库回退提交：

```text
487f132 revert: restore internal LCD DMA bounce
```

该提交撤销 `aecc037`、`e2bb921`、`3290a99`、`8898534` 的运行代码和实验测试，
但保留设计文档与完整 Git 历史。实验前已有、当时未跟踪的
`app/agentguard/CMakeLists.txt` 已按原内容恢复为未跟踪文件，没有丢弃用户文件。

NuttX 官方源码回退提交：

```text
a50da54c4a3 revert: drop PSRAM SPI TX experiment
```

该提交撤销 `79eb108d07d` 与 `7ac22d18890` 的源码效果；当前
`esp32s3_spi.c` 与 `dd92bcf4257` 基线一致，专用 PSRAM 描述符头已删除。三个实验/
回退提交都只保留在 detached 本地历史中，没有远端引用、没有 push，仍需官方
检查后才能决定是否保留历史或重新整理。

回退后的完整 AgentGuard 主机测试全部 PASS，工作区检查 6 项全部 OK，关键应用
文件与 `af04e77` 比较无差异，关键 NuttX 文件与 `dd92bcf4257` 比较无差异，目标
镜像完整重建通过。最终 `nuttx.bin` 为 2,085,644 bytes，SHA-256：
`542bc945eb155ffcfc4b201e3ec7aa120dda07da86d90be1ec9bc18d58ec536a`。
esptool 完成写入并报告 `Hash of data verified`，随后 RTS 硬复位。

下一步只确认显示是否已恢复到回退前的状态：颜色正常、文字可读、摄像头画面完整、
无彩色横条。预计扫描线会回到 32 行版本的“仍明显”水平；没有肉眼反馈前不能声称
显示已恢复。后续若继续消除扫描，必须放弃 PSRAM 直接 SPI TX，重新设计内部 RAM
双 bounce/异步队列或寻找可用的硬件同步边界。

### 冻结显示并恢复人脸检测数值路径（已实现，待实机验证）

32 行内部 RAM 回退版已经用户实机确认：文字正常可读、摄像头画面完整、没有彩色
横条；扫描线仍然明显。显示问题至此冻结，不再修改 LCD 时序、刷新率、颜色字节序、
预览几何或 DMA 链路，后续工作切换到人脸检测。

此前单变量实验已把 `NO PERSON` 定位到 ESP-DL 在当前 ESP32-S3/openvela 组合下的
两类 TIE728 卷积数值路径：普通卷积与深度卷积同时使用 TIE 时参考信号约为
`AI=60, T/F/S=2/0/2`；全部切到 C 时约为 `AI=590-600, T/F/S=57/0/57`；只将
普通卷积和深度卷积切到 C、其余算子保留 TIE 时得到相同的有效参考信号。只切换
其中一种卷积不能得到可信结果。因此本轮采用已批准的混合后端：

```text
普通卷积：portable C
深度卷积：portable C
其他 ESP-DL 算子：保留 TIE728/Xtensa 优化
两级人脸分数阈值：恢复官方 0.50（原诊断值 0.60）
推理节拍：保持每 3 帧一次
```

实现提交：

```text
43a043b fix: restore portable face convolutions
```

修改范围只有 `compat/espdl/sdkconfig.h`、`src/vision_espdl.cpp` 和对应后端配置测试。
测试先要求两个 FORCE_C 宏为 1 且阈值为 50，旧实现因两个静态断言失败并缺少阈值
宏而 RED；实现后 GREEN。随后执行 `make clean` 和完整 NuttX 交叉编译，确保第三方
ESP-DL 对象不是增量构建残留。新生成的普通卷积/深度卷积共 4 个对象（两种应用
构建变体）均经 target `nm -u` 验证，不再引用任何 TIE conv/depthwise 符号。

2026-08-20 完整 AgentGuard 主机测试全部 PASS（含 backend config、TIE no-stdio、
LCD DMA config），工作区检查 6 项全部 OK，`git diff --check` 通过，完整目标构建
成功。构建时显示配置仍为：

```text
CONFIG_LCD_ST7789_FREQUENCY=80000000
CONFIG_ESP32S3_SPI_DMA_BUFSIZE=15360
```

烧录前 `nuttx.bin` SHA-256 为
`acc669007ed31518960aebc6364060f23961810a7d200266e08665d51d16f634`。
烧录命令触发版本信息重链接后，最终 `nuttx.bin` 为 1,954,572 bytes，SHA-256：
`01481d79e1ee1dd501702e8da47ec833a93c6e4a0f0508eb00224e6314e45a5a`。esptool
识别 ESP32-S3 rev 0.2、8 MB PSRAM 与 8 MB flash，写入 1,954,572 bytes 后报告
`Hash of data verified`，并通过 RTS 硬复位。烧录后未打开串口，避免 USB 控制线再次
触发复位。

代码、测试、对象核验、完整构建和烧录只能证明所选数值路径已进入实机，不能替代
肉眼人脸识别验证。下一步需要记录：参考诊断 T/F/S，实时 S/M/AI，有人脸时 FACE
数量和框是否出现，无人场景是否误报，以及显示是否仍保持文字可读、画面完整、无
彩条。

### 实机检测边界诊断：优先显示 MSR 候选数与推理耗时

提交 `43a043b` 的首轮实机结果为：屏幕刷新率极低；有人脸时仍无框并显示
`NO PERSON`；顶部为 `T:57/F:1 S:57`；原底部 LCD 计时约为 `D:10`、
`S:0-10`、`M:0-10`。这组数据证明内置参考图已经通过两级检测并得到 1 张人脸，
实时 MSR 原始最高分也达到 57%，但尚不能判断实时 MSR 后处理是否生成候选，或
候选是否在第二级 MNP 被全部拒绝。

现有 `ag_ui_format_diagnostic_detail()` 总是优先显示 LCD 分段计时，导致已经采集的
`msr_candidates` 和 `inference_ms` 无法在当前运行状态下看到。已批准并实施最小
诊断改动：当 LCD 分段计时与模型诊断同时有效时，底部改为显示
`M:<MSR候选数> AI:<推理毫秒> K:<自检>`；顶部 `T/F/S`、`FACE`、阈值、图像
方向、模型后端和 LCD 数据路径均不改变。提交：

```text
8011b7e diag: show live face model timing
```

TDD 证据：测试先要求同时有效时输出 `M:3 AI:847 K:11`，旧实现仍输出 LCD
`D/S/M` 并在对应断言处 RED；只增加这一条优先级后 GREEN。完整 AgentGuard 主机
测试全部 PASS，工作区检查 6 项全部 OK，目标固件构建成功，`git diff --check`
通过。烧录后需在真人稳定位于画面中央时记录底部 `M` 和 `AI`：`M=0` 将故障边界
定位到 MSR 后处理，`M>0` 且 `FACE=0` 将边界定位到 MNP；`AI` 用于量化同步 C
卷积造成的刷新阻塞。最终 `nuttx.bin` 为 1,954,572 bytes，SHA-256：
`a6cdcb97af69afb2ae75588a7d3252e2543da19034075f14b1d193a4dbf83cba`。esptool
完成写入、报告 `Hash of data verified` 并通过 RTS 硬复位；烧录后未打开串口。
取得新的 `M/AI/K` 实机证据前不调整阈值、方向或线程架构。

### 单变量方向实验：将 180°旋转移动到推理之前

提交 `8011b7e` 的实机结果为：有人脸时 `M:1 AI:840 K:11 FACE:0`，无人时
`M` 也为 1。由此确认一级 MSR 始终产生一个与真人无关的背景候选，二级 MNP 始终
拒绝该候选；`FACE:0` 不是人脸框绘制路径造成。`AI:840` 同时确认当前同步 C
卷积推理是屏幕刷新率极低的主要原因。

用户进一步确认：手持开发板时，LCD 文字与摄像头人脸画面均相对用户旋转 180°，
需要把开发板倒过来才能正常阅读。代码数据流显示模型在原始帧上先推理，随后才将
帧旋转 180°供预览；因此下一单一假设是模型实际看到倒置人脸。

已批准并实施仅改变旋转时点的实验：既有原地 180°旋转从推理后移动到推理前；
推理后的重复旋转和人脸框坐标反转删除。整帧仍只旋转一次，传给 LCD 的摄像头像素
方向与上一版相同；阈值、C/TIE 后端、推理间隔、LCD 时序和 DMA 链路均不改变。
提交：

```text
d1215e4 diag: orient frames before face inference
```

方向行为测试使用手工推导的 3x2 像素序列，验证模型输入方向入口产生
`6,5,4,3,2,1`；生产方向入口成为唯一旋转实现。完整 AgentGuard 主机测试全部
PASS，工作区检查 6 项全部 OK，目标交叉构建成功，`git diff --check` 通过。
原有未跟踪的 `vision.h`、`vision.c`、`test_vision.c` 在边界复核后保持原内容和
未跟踪状态，没有纳入提交。最终 `nuttx.bin` 为 1,954,572 bytes，SHA-256：
`dcb5c298325c7359d714b713470082d25f931e49f9fd330c81d9a0f939e51639`。esptool
完成写入、报告 `Hash of data verified` 并通过 RTS 硬复位；烧录后未打开串口。
现在需再次记录有人/无人时的 `T/F/S`、`M/AI/K`、`FACE` 和人脸框；若真人仍不
改变结果，撤销本实验并转向 RGB565 caps/字节序诊断。

方向实验实机结果：推理前旋转后仍固定 `M:1/FACE:0`，真人没有改变任何检测结果；
屏幕方向也保持上一版不变。后一点属于实验设计的预期，因为同一次旋转只是从推理后
移动到推理前，LCD 最终仍收到相同方向的像素；该实验没有试图修正物理屏幕朝向。
方向假设已被否定，并按预先批准的退出条件创建可追溯回退：

```text
cc17b2e Revert "diag: orient frames before face inference"
```

回退后仓库恢复为推理后旋转与原框坐标变换；暂未单独烧录回退镜像，计划与下一版
RGB565 caps 单变量诊断一起构建烧录，避免无信息的额外烧录轮次。屏幕物理朝向仍是
独立显示问题；在人脸检测定位期间继续冻结，不与像素格式实验捆绑。

### RGB565 输入 caps 四模式实机诊断

方向实验失败并回退后，已按批准方案实施 RGB565 预处理 caps 的四模式循环诊断：

```text
P0 = 0
P1 = RGB_SWAP
P2 = RGB565_BIG_ENDIAN
P3 = RGB_SWAP | RGB565_BIG_ENDIAN（此前固定模式）
```

每个模式保持 4 次完整实时推理，再切换至下一模式；跳过推理的预览帧不推进模式。
底部 HUD 在模型诊断有效时显示 `P:<模式> S:<最高分> M:<一级候选> F:<最终人脸>
AI:<毫秒>`。内置参考图仍固定使用原 P3，不参与模式轮换；阈值、C/TIE 后端、每
3 帧推理节拍和 LCD 链路均未改变。本次提交为：

```text
f49e619 diag: cycle RGB565 face input modes
```

TDD 证据：新增模式序列测试要求索引 0..16 输出四组 `0000/1111/2222/3333/0`，
在生产头文件缺失时先 RED，加入纯函数后 GREEN；HUD 测试先要求
`P:2 S:57 M:3 F:1 AI:847`，旧格式断言失败后再实现新格式。最终完整 AgentGuard
主机测试全部 PASS，工作区检查 6 项全部 OK，暂存差异 `diff --check` 通过；加载
openvela Python 环境后 NuttX 完整链接并成功生成镜像。

已将方向回退 `cc17b2e` 与本次诊断 `f49e619` 一并烧录。esptool 识别 ESP32-S3
rev 0.2、8 MB PSRAM 与 8 MB flash，写入 1,954,620 bytes 后报告
`Hash of data verified`，并通过 RTS 硬复位。最终 `nuttx.bin` 为 1,954,620 bytes，
SHA-256：`04328dcadd159e30b2ab447330359b215d8ec6d11df7251acae69f7b31f89dcc`。
烧录后未打开串口，避免 USB 控制线再次触发复位。

下一步实机观察：先让真人稳定居中覆盖一个完整 P0→P3 周期，再对空场景重复一次，
逐模式记录 `P/S/M/F/AI`、`FACE` 和是否出现框。按当前约 840 ms/次推理估算，每个
模式约 3–5 秒、完整周期约 14–20 秒。若某一 P 模式能稳定区分有人/无人，则固定该
模式；若四种模式结果相同，则停止继续猜测 caps，转而检查摄像头缓冲区内容与缓存
一致性。

四模式实机结果：P0、P1、P2、P3 在真人场景下均保持 `S:57 M:1 F:0`，没有任何
模式产生最终人脸或人脸框。因此 RGB565 的 `RGB_SWAP` / `RGB565_BIG_ENDIAN`
caps 组合不是实时输入固定为背景候选的原因；后续人脸诊断停止继续枚举 caps，应按
既定退出条件检查摄像头缓冲区实际内容与缓存一致性。当前诊断轮换暂留在固件中，未与
独立的显示方向修复捆绑清理。

### 最终 LCD 合成画面旋转 180°

用户确认此前 LCD 文字和摄像头预览相对手持方向均倒置。根因边界位于最终显示坐标，
而不是模型输入方向：摄像头帧此前只在发布预览前旋转，随后绘制的 HUD 文字并未一起
旋转。已将 180° 旋转移到显示线程的最终合成出口：预览、未来的人脸框和 HUD 全部
绘制完成后，对完整 `240x240` framebuffer 做一次原地 180° 旋转。模型推理输入、
摄像头发布前的既有预览旋转、阈值和后端均未改变。

为保留当前降低扫描影响的局部 LCD 更新，输出预览区域同步从逻辑 `y=37` 映射为
旋转后的 `y=53`；物理顶部区域改为 34 行、底部区域改为 18 行，HUD 快照内存偏移
也按 34/18 行重新分配。没有恢复全屏传输，也没有修改 openvela 官方 LCD 驱动源码。
提交：

```text
30d8a9f fix: rotate composed LCD output
```

TDD 证据：测试先调用不存在的最终合成旋转入口，编译因
`ag_ui_render_oriented_rgb565` 缺失而 RED；实现后验证既有摄像头像素从 `(100,100)`
移动到 `(139,139)`，HUD 左上角状态色块移动到 `(235,235)`，测试 GREEN。提交前
完整 AgentGuard 主机测试全部 PASS，工作区检查 6 项全部 OK，暂存差异
`diff --check` 通过，NuttX 完整链接并成功生成镜像。

最终烧录镜像为 1,954,636 bytes，SHA-256：
`7dce67aa8997c9a851abbe5564b5fd6916a0739af38eca6e7d053d632821a3fa`。esptool
识别 ESP32-S3 rev 0.2、8 MB PSRAM 与 8 MB flash，报告 `Hash of data verified`
并通过 RTS 硬复位；烧录后未打开串口。自动测试证明像素与局部更新坐标变换一致，
但最终物理观看方向仍需用户确认文字和摄像头画面均已转正且画面完整。

显示方向实机复验通过：用户确认摄像头画面方向和文字方向均已正常。摄像头刷新仍有
问题，但按用户要求继续暂缓，优先实现可用的人脸检测。

### ESP-DL 优先的混合人脸检测回退

P0-P3 恒定 `S:57 M:1 F:0` 后进一步核对数据链：ESP32-S3 摄像头下半层已在 DMA
完成时对内部帧缓冲执行 `up_invalidate_dcache()`，随后通过 CPU `memcpy` 写入
V4L2 用户缓冲，因此没有依据再向用户缓冲盲目增加 cache 操作。ESP-WHO 官方示例
同样将 RGB565 帧直接交给 `HumanFaceDetect`，当前模型类型和默认 P3 caps 与官方
一致。故障边界保留在当前 openvela ESP-DL 移植的实时评分链。

为先恢复产品可用的人脸框和在位状态，按用户批准实现混合策略：ESP-DL 返回人脸时
保持其结果不变；仅当其 `face_count=0` 时，对同一 RGB565 帧执行 4 像素步长的肤色
连通区域检测。回退候选至少需要 40 个采样格、宽高各至少 4 格，且宽高比限制在
1:2 到 2:1；这会拒绝小肤色色块和明显细长的手臂/条带。回退仅补充人脸数量与主框，
本轮姿态分数保持 0。ESP-DL、P 模式诊断和约 840 ms 同步推理仍保留，刷新性能不在
本次修改范围内。没有修改 openvela 官方源码。

实现提交：

```text
ab1ad76 feat: add hybrid face detection fallback
```

TDD 证据：先新增 `test_face_fallback`，构建因生产模块不存在而 RED；实现后测试验证
56x68 合格区域得到精确人脸框、已有 ESP-DL 人脸结果完全不变、80x20 宽条和
16x16 小色块均不产生人脸，转为 GREEN。最终完整 AgentGuard 主机测试全部 PASS，
工作区检查 6 项全部 OK，暂存差异 `diff --check` 通过，NuttX 目标构建明确编译
`face_fallback.c` 与主程序并成功生成镜像。

烧录镜像为 1,954,700 bytes，SHA-256：
`fb1b288214d3697f24c55f5c4c09f61e0670ff29005981e9d3967788c9870cc5`。esptool
识别 ESP32-S3 rev 0.2、8 MB PSRAM 与 8 MB flash，写入后报告
`Hash of data verified` 并通过 RTS 硬复位；烧录后未打开串口。下一步实机分别验证：
真人居中时是否显示 `FACE:1` 与稳定框，离开后是否恢复 `NO PERSON`，手掌单独入镜或
空场景是否误报。该方案先保证功能可用，不应描述为 ESP-DL 神经网络根因已经修复。

### 稳定回退检测的人脸计数

实机验证中，真人居中能够得到 `FACE:1`，但画面刷新过程中会在 `FACE:1` 到
`FACE:3` 之间变化；无人时稳定显示 `NO PERSON`。这说明回退肤色门限并未对空场景
普遍误报，而是将脸、手、颈部或暂时分裂的肤色连通区域分别累计成了多人。轻量回退
检测本身不具备可靠的多人分类能力，因此将其语义收紧为“存在性检测”：只要存在至少
一个合格候选就输出 `FACE:1`，并仅使用面积最大的候选绘制主框。ESP-DL 已返回非零
结果时仍保持原结果不变，真实多人计数能力没有从神经网络路径移除。

实现提交：

```text
720d228 fix: stabilize fallback face count
```

TDD 证据：新增三个彼此分离且均满足尺寸条件的肤色区域，要求回退结果仍为
`face_count=1`，并精确选择最大的 `64x72` 区域。旧实现因返回 3 而 RED，最小修改后
GREEN；既有单区域、ESP-DL 结果不覆盖、宽条和小区域拒绝测试继续通过。完整
AgentGuard 主机测试全部 PASS，工作区检查 6 项全部 OK，目标 NuttX 构建成功，差异
检查通过。本次没有修改 LCD/摄像头参数或 openvela 官方源码。

烧录镜像为 1,954,700 bytes，SHA-256：
`9d147c2dac523957da18bb64ea86f560150d82692551ecaaeb9e06590ce96206`。esptool
识别 ESP32-S3 rev 0.2、8 MB PSRAM 与 8 MB flash，写入后报告
`Hash of data verified` 并通过 RTS 硬复位；烧录后未打开串口。下一步实机验证真人
居中时 `FACE` 是否持续为 1、最大人脸框是否稳定，以及离开后是否恢复
`NO PERSON`。若框本身仍跳动，应单独为主框选择增加空间/时间迟滞，不应重新恢复
回退路径的多人计数。

### 卡顿帧的人脸离场迟滞

实机进一步确认：真人居中时大多数时间为 `FACE:1`，人脸框稳定，人离开后也能恢复
`NO PERSON`；仅在摄像头画面卡顿或扫描线明显的瞬间短暂变为 `FACE:0`。数据流检查
表明每一帧的神经网络/肤色回退结果都会立即同时送入业务状态与 UI，单个不完整或异常
帧的漏检因而直接清除人脸状态。这不是多肤色区域计数问题，也没有证据需要再次修改
检测阈值。

按用户批准增加 1.5 秒时间迟滞：检测到人脸时立即更新计数和主框；暂时漏检时仅沿用
最近的人脸计数和框，不沿用旧姿态分数；连续超过 1.5 秒没有检出才清零。使用单调时间
而不是帧数，避免当前不稳定帧率改变离场延迟；时钟回退时立即清除保存状态。过滤位置
位于 ESP-DL/肤色回退之后、业务状态和 UI 发布之前。没有修改 LCD、摄像头参数或
openvela 官方源码。

实现提交：

```text
8a99d6c fix: hold face presence through frame stalls
```

TDD 证据：新测试先因 `face_presence` 生产模块不存在而 RED；实现后验证 1.499 秒与
恰好 1.5 秒的漏检继续输出最近的 `FACE:1`/主框，1.501 秒时清零，新检测立即替换
旧框，时钟回退清除状态，转为 GREEN。完整 AgentGuard 主机测试全部 PASS，工作区
检查 6 项全部 OK，NuttX 目标构建明确编译 `face_presence.c` 并成功生成镜像。构建镜像
为 1,954,700 bytes，烧录前 SHA-256：
`5eeeb4517a2f13c7a7b7a7336bcb61f0efcf9ec1fac01ff7e2aa12c839c685fd`。

本轮首次烧录请求因自动授权检查网络连接中断而未执行；告知用户后获得重新明确批准，
第二次请求成功访问 `/dev/ttyACM0` 并完成烧录。烧录命令重新链接后的最终镜像仍为
1,954,700 bytes，SHA-256：
`a673b745bf2722ad6ca1dc697e2a9e29ee9cd436649bd9cbf8f7fa19cc58ad7d`。esptool
识别 ESP32-S3 rev 0.2、8 MB PSRAM 与 8 MB flash，报告
`Hash of data verified` 并通过 RTS 硬复位；烧录后未打开串口。下一步实机观察：
扫描线或短时卡顿时 `FACE` 是否保持 1，以及真人完全离开后约 1.5 秒是否恢复
`NO PERSON`。

### ESP-DL 实时人脸 Stage A 同次推理诊断

实机继续发现肤色回退存在根本误报：真人离开后错误 `FACE:1`/框可持续 2–3 秒，
手掌也可能被识别为人脸。根因是肤色连通区域只能表达“肤色区域存在”，无法可靠区分
脸、手和肤色背景；继续叠加肤色阈值或迟滞会掩盖 ESP-DL 实时链路的真实故障。因此
本阶段冻结 fallback 与 1.5 秒 hold，不宣称修复准确率，只增加只读、同次推理诊断，
定位 V4L2 RAW、MSR 预处理、MSR 输出、MNP 裁剪/输出之间的首个分歧边界。

设计与实施计划提交：

```text
7ef0fdb docs: design ESP-DL live face diagnostics
132eb49 docs: plan ESP-DL live face diagnostics
```

Stage A 按任务独立提交：

```text
73e5f54 test: add deterministic face diagnostic fingerprints
8670c91 feat: add bounded face diagnostic snapshots
fbc426a diag: capture ESP-DL face stage fingerprints
0aa924b diag: publish same-inference face snapshots
6b3955d diag: show live ESP-DL stage changes
```

实现内容：使用显式低字节/高字节顺序的 FNV-1a 为 RGB565、int8 和 int16 数据生成
确定性哈希与范围；RGB565 同时记录全图及上/中/下三区哈希。最多保存前 4 个 MNP
详细条目，但尝试总数饱和累计。完整快照用 pthread mutex 原子复制，缓存预览帧不递增
代次、不采集也不发布。MSR 输入在 `Model::run()` 前采集，MSR 输出在后处理前采集；
每个 MNP 的裁剪框在预处理前保存，输入在模型运行前、score/box/landmark 在运行后
立即采集，避免 `Model::minimize()` 的张量内存复用污染诊断。

实时预处理固定为官方 P3：`RGB_SWAP | RGB565_BIG_ENDIAN`，删除 live P0-P3 轮换；
旧 HUD 模式字段固定报告 3。MSR/MNP 阈值仍为 0.50，普通卷积与 depthwise 卷积仍为
portable C，每 3 个采集帧执行一次完整推理。摄像头重启前清除已发布快照与跨流变化
比较。新增只读 `agentguard face-diag` 命令；正常 LCD 底部最高优先级显示
`G:<代次> R:<RAW变化> I:<MSR输入变化> M:<尝试>/<接受> F:<原始神经人脸数>`，
其中 `F` 明确来自 ESP-DL trace，不来自 fallback/hold 修改后的 UI 人脸数。

TDD 证据：确定性指纹测试先因生产模块缺失 RED，随后验证 int8 哈希
`6fab6075`、int16 哈希 `defc708b`、RGB565 全图/三区固定哈希及无效输入清零后
GREEN；快照测试先因模块缺失 RED，随后验证 4 条上限、互斥复制、跨代变化与 768
字节格式后 GREEN；张量适配先以未定义符号 RED，随后验证只接受有符号 int8/int16
后 GREEN；同次快照组装先以未定义符号 RED，随后验证首代无变化、次代只比较 RAW
全图与 MSR 输入后 GREEN；LCD 测试先以缺失格式化符号 RED，随后验证精确字符串、
上限截断和最高优先级后 GREEN。首次 GREEN 过程中曾发现测试错误地用 1 像素缓冲
模拟 `65535x65535` 图像；该乘积在 64 位主机不溢出，因而测试自身越界。已删除这个
无法由接口验证的错误夹具，保留空指针、零维度和输出清零边界。

最终新鲜验证：全部 AgentGuard 主机测试 PASS；工作区检查的 contest manifest、官方
ESP32-S3-EYE defconfig、AgentGuard Kconfig、Make.defs、manifest mapping、package
link 六项均为 `OK`；`git diff --check` 无输出。NuttX 完整构建实际编译新增 C 模块、
修改后的 vendored `human_face_detect.cpp`、`vision_espdl.cpp`、主程序与 LCD UI，并
成功链接。`human_face_detect.cpp/.hpp` 此前是应用内未跟踪的 vendored fork，本次从
`fbc426a` 起首次纳入版本控制；没有修改 openvela 官方源码，没有 push。

烧录前镜像为 1,955,028 bytes，SHA-256：
`ddda2f7b0df319d5d4002596c066272be79f9204e93649f342bb7ccc105e39e0`。
确认 `/dev/ttyACM0` 对应 USB `303a:1001` 且未占用后烧录；esptool 识别 ESP32-S3
rev 0.2、8 MB PSRAM 与 8 MB flash，写入 1,955,028 bytes，报告
`Hash of data verified` 并通过 RTS 硬复位。flash 目标重新链接后的最终镜像仍为
1,955,028 bytes，SHA-256：
`878b182644f299ca91fe09ce381135b3079e0fc53788be595e6e42c90b20c467`。
烧录后未立即打开串口。

待采集 Stage A 四场景证据；每个场景需等待 `G` 至少变化 4 次并记录：

```text
scene  sample  G  R  I  M_attempt/M_accept  F
EMPTY  1-4     changing  1  1  1/1  1 (user reports all observed generations identical)
FACE   1-4     changing  1  1  1/1  1 (stable centered face; all generations identical)
PALM   1-4     changing  1  1  1/1  1 (isolated palm; all generations identical)
EMPTY2 1-4     changing  1  1  1/1  1 (returned empty scene; all generations identical)
```

首个 `EMPTY` 屏幕结果：用户确认连续代次始终为 `R:1 I:1 M:1/1 F:1`。
这证明 V4L2 RAW 与 MSR 预处理输入都在逐次变化，空场景却稳定产生 1 个 MSR 候选、
1 个 MNP 接受和 1 个原始神经人脸。故障不再符合“RAW 不变”或“MSR 输入不变”两条
早期边界；仍需与 `FACE/PALM/EMPTY2` 的完整张量哈希对比后，才能在 MSR 数值输出、
MNP ROI 或 MNP 输出/后处理之间选择唯一首个分歧，不能仅凭计数提前修复。

尝试用不触碰 DTR/RTS 的 `hw_raw_serial.py` 读取 `agentguard face-diag`，首次因脚本
文件没有执行位而未运行；改由 Python 解释器调用时，权限审批服务发生网络响应解码
中断并拒绝执行，开发板未被操作。没有绕过审批，完整 EMPTY 快照仍为 pending。

`FACE` 屏幕结果：真人稳定居中并跨至少 4 个 `G` 代次后，仍始终为
`R:1 I:1 M:1/1 F:1`，与 `EMPTY` 完全相同。RAW/MSR-input 的跨代变化位只能证明
连续帧不同，不能证明 MSR/MNP 输出对场景有区分；必须取得完整快照中的 MSR
score/box、MNP crop/input/score/box/landmark 哈希后再分类。

`PALM` 屏幕结果：只让一只手掌稳定入镜并跨多个 `G` 代次后，仍始终为
`R:1 I:1 M:1/1 F:1`，与 `EMPTY` 和居中真人完全一致。现有摘要证明 RAW 与 MSR
输入持续变化，但一级始终给 1 个候选、二级始终接受 1 个并输出 1 张原始神经人脸；
尚不能仅凭计数区分“输出张量固定”和“输出变化但后处理恒接受”。

`EMPTY2` 屏幕结果：移开真人和手掌、恢复完全空场景后，仍跨多个 `G` 代次稳定为
`R:1 I:1 M:1/1 F:1`。四场景屏幕矩阵至此完整，`EMPTY/FACE/PALM/EMPTY2` 的摘要
完全一致。RAW 与 MSR 输入的跨代变化位均为 1，故 V4L2 内容固定和预处理输入固定
两条早期故障边界已排除；计数本身不足以在 MSR 数值输出、MNP ROI/输入和 MNP
输出/后处理之间继续分类，仍必须取得完整哈希快照。

每个场景还需保存一次完整 `agentguard face-diag` 数值输出。取得数据后只按批准的决策
表分类一个首个分歧边界，Stage A 随即停止；不得在同一阶段继续修改阈值、fallback、
hold、摄像头或 LCD。

#### Stage A 物理采集阻塞：LCD 未覆盖行残留修复

采集完 `EMPTY/FACE` 后，用户报告摄像头残留遮挡下方数字、上方状态栏卡住并遮挡
文字，因此暂停 `PALM` 采集。代码复核确认局部提交区域此前只覆盖物理行 `0–33`、
`53–202`、`222–239`，物理行 `34–52` 与 `203–221` 共 38 行从不刷新；旧摄像头或
HUD 像素可永久留在 LCD GRAM 中。差异快照又假设实际 LCD 内容仍与历史一致，新的
诊断代次可能不会重新建立完整上下区域。

用户批准最小显示修复：屏幕改为连续的 53 行顶部区域、150 行摄像头区域、37 行
底部区域，三者不重叠且完整覆盖 240 行；摄像头大小/位置不变。首次出现诊断或 `G`
代次变化时让上下区域快照失效并完整重刷，同一代次继续差异刷新。没有修改刷新周期、
人脸检测、阈值、摄像头或 openvela 官方 LCD 驱动。提交：

```text
044a970 fix: cover all LCD refresh rows
```

TDD 证据：测试先要求带 1 行空洞的三段配置返回失败，并调用尚不存在的诊断代次接口，
旧实现编译 RED；实现后验证连续覆盖、首次/新代次同时失效上下快照、同一代次不重复
强刷，转为 GREEN。随后全部 AgentGuard 主机测试 PASS，工作区 6 项结构检查均为
`OK`，`git diff --check` 无输出。提交前的 NuttX 目标构建已成功编译
`agentguard_main.c` 与 `display_regions.c` 并链接镜像。

提交后的首次最终目标构建/尺寸/哈希/烧录请求未执行：自动权限审批服务发生网络响应
解码中断并拒绝命令。没有绕过审批；向用户说明后取得了对“重试目标构建并烧录”的
明确批准。重试构建成功，烧录前镜像为 1,955,028 bytes，SHA-256：
`819512488598aa62f7050646d1535ce41ebfa82dbf51a3fd8731b3c3cb33c54c`。
再次确认 `/dev/ttyACM0` 为未占用的 USB `303a:1001` 后烧录。esptool 识别
ESP32-S3 rev 0.2、8 MB PSRAM 与 8 MB flash，写入 1,955,028 bytes 后报告
`Hash of data verified`，并通过 RTS 硬复位。flash 目标重新链接后的最终镜像仍为
1,955,028 bytes，SHA-256：
`4dd4ccdffa3f92e251bb7d618336ff065ea821f421a16ae39f5a63015f406bf4`。
烧录后未立即打开串口。下一步先由用户确认上下文字可读且无摄像头残留遮挡；通过后
恢复 `PALM/EMPTY2` 采集。

LCD 修复实机复验通过：用户确认摄像头残留、上方状态栏遮挡和下方数字遮挡均已消失，
当前显示正常。`044a970` 的连续 240 行覆盖方案有效，Stage A 物理数据采集恢复。

#### 完整快照出口与产品启动模式不匹配

四场景屏幕矩阵完成后，经用户批准尝试读取当前 `EMPTY2` 的完整快照。第一次调用
`python tools/hw_raw_serial.py` 因系统不存在 `python` 命令而未启动；改用 `python3`
后，脚本按设计不触碰 DTR/RTS，并安全运行 12 秒，但没有检测到 `nsh>`，因此未发送
`agentguard face-diag`，退出码为 2，开发板未复位。

根因不是串口故障：当前 NuttX 配置为 `CONFIG_INIT_ENTRYPOINT="agentguard_main"`，
产品固件复位后直接以前台入口运行 AgentGuard；虽然 NSH 库和 builtin 表仍被编入，
但 shell 任务没有启动。因此 Stage A 设计的 NSH `agentguard face-diag` 命令在当前
产品启动模式下不可达。该命令代码本身保留，但不能作为本轮实机证据出口。

ELF 符号确认快照仍在 RAM 中：

```text
0x3fccbf94 (anonymous namespace)::g_face_diag_store
0x3fc9e500 (anonymous namespace)::g_face_diag_sequence
```

可选的无固件修改方案是用现有 ESP32-S3 USB-JTAG 暂停双核数毫秒、读取该 RAM
快照后立即恢复；不会写 flash，但短暂停核可能触发一次摄像头 watchdog 重启或 LCD
瞬时停顿。查找已安装 Xtensa GDB/OpenOCD 的只读权限请求再次因审批服务网络解码
中断而被拒绝，尚未连接 JTAG。必须向用户说明风险并取得明确批准后才能重试。

用户随后明确批准只读 JTAG 采集。ESP32-S3 复合设备为 USB `303a:1001`，虚拟机内
节点 `/dev/bus/usb/002/014` 原权限不足；用户手动执行 `sudo chmod 666` 后，OpenOCD
以官方 `board/esp32s3-builtin.cfg` 和降速 `adapter speed 1000` 成功识别双 TAP。
40 MHz 初始连接曾遇到一次 USB 字符串描述符超时，主机侧 `usbreset 303a:1001`
后低速连接正常。Xtensa GDB 17.1 与该 OpenOCD 的 `vMustReplyEmpty` 探测不兼容，
连接失败时 OpenOCD 曾短暂停核；随后确认目标已恢复为 `CPU0=running / CPU1=running`。
后续不再使用 GDB 协议，改用带 Tcl `catch` 的单次 OpenOCD 事务执行
`halt -> dump_image 896 bytes -> resume -> shutdown`，保证失败路径也恢复核心。

首个完整 `EMPTY2` 快照已保存到临时文件 `/tmp/agentguard-face-empty.bin`，序号
`2540`。目标采集后再次只读确认双核均为 `running`。关键字段如下：

```text
RAW full/top/mid/bottom = b3e7d5a8/a132ad66/0b5cb35f/f813e1e9
live MSR input          = 91f445db, n=57600, range=-128..127
live MSR score0/box0    = 68c49881/a56ca17f
live MSR score1/box1    = 6bfd48ef/fddc7399
live largest MSR box    = (0,0,0,0)
live MNP[0] crop/input  = (0,0,0,0) / 30a52505
live MNP[0] score/box/landmark = 4a7ee31f/ba23a125/69447972
live MNP[0] score_pct/valid    = 99/0
live counts candidates/attempts/recorded/accepted/faces/valid = 1/1/1/1/1/1
```

参考黑帧同样产生 `1/1/1/1/1/1`：参考 MSR 输入 `ee2f51c5`，57600 个元素全部为
0；MNP[0] 分数 99、`valid=0`，但 accepted 与 final_faces 都为 1。真实 EMPTY2 数据
与参考黑帧的 MSR/MNP 张量哈希不同，说明模型数值输出不是完全固定；然而两者都同时
出现零尺寸 largest/crop、MNP diagnostic `valid=0` 与 accepted/faces=1 的矛盾。
这把首要疑点收窄到 ESP-DL 框坐标提取/后处理接受计数或诊断对象生命周期边界，
而非 V4L2 输入固定或预处理输入固定。还需采集同固件的 FACE 与 PALM 完整快照进行
场景对照，之后按 Stage A 决策表停止并只分类首个分歧，不在本阶段直接修复。

`FACE` 完整快照随后采集成功，序号为 `64`。与 `EMPTY2` 相比，真人居中时 RAW
`e7eedb9b`、MSR 输入 `69a522d6`、MSR score0/box0 `34b785e8/0866cd3b`、MSR
score1/box1 `8af32bc5/40acaa90`、MNP 输入 `da00a113`、MNP score/box/landmark
`477c9fcf/fa864edd/da786421`，每一个数值边界的哈希都与 EMPTY2 不同。这排除了
“MSR 输出固定”与“MNP 输入/输出张量固定”。但 FACE 仍与 EMPTY2 完全相同地报告
largest/crop `(0,0,0,0)`、MNP score 99、diagnostic `valid=0`，以及
`candidates/attempts/recorded/accepted/faces/trace-valid = 1/1/1/1/1/1`。

此次 OpenOCD 初次连接日志报告 debug controller/core 状态重建，快照序号由先前
`2540` 变为 `64`，说明 JTAG 重连期间固件发生过一次重启；因此不使用跨重启序号
或时序作判断，只比较同一固件的结构字段与哈希。采集事务后另行只读确认
`CPU0=running / CPU1=running`。剩余 PALM 完整快照用于完成场景矩阵；当前首个可见
矛盾已位于张量输出之后的框坐标/后处理接受或诊断对象生命周期边界。

`PALM` 完整快照序号 `142`：RAW `5c2f48ec`、MSR 输入 `d3a91c11`、MSR
score0/box0 `c9a68e30/4603add5`、score1/box1 `8787d9fb/babcacea`、MNP 输入
`3b18730c`、MNP score/box/landmark `4a7ee31f/b9239f92/d7785f68`。PALM 的 MNP
score 哈希与 EMPTY2 相同，其余列出的张量哈希随场景变化；但仍固定报告零尺寸
largest/crop、score 99、MNP diagnostic `valid=0` 与计数 `1/1/1/1/1/1`。采集后
再次确认 `CPU0=running / CPU1=running`。

Stage A 唯一分类：**MNP ROI transformation/preprocessor boundary**。依据是 RAW、MSR
输入、MSR score/box 张量和 MNP 输入/多数输出均随 EMPTY2/FACE/PALM 明显变化，
排除 V4L2、预处理输入固定、MSR 数值输出固定及 MNP 张量整体固定；但每个场景的
MSR 候选经记录都成为 `(0,0,0,0)` 退化框，MNP crop 同样为零框，诊断有效位为 0，
流程却仍尝试并接受该候选。复核 vendored ESP-DL `result_t::limit_box()` 可知坐标会
裁剪至图像范围，而 `copy_candidate_box()` 只有在 `x2>x1 && y2>y1` 时才有效；因此
当前零框表示候选裁剪后退化，但 `MNP::run()` 仍对该退化 ROI 执行 preprocess/model/
postprocess。它是决策表中“MSR tensors/candidates change, MNP input unchanged or
wrong crop”的明确分支，也是本轮第一个可操作分歧。

Stage A 现已停止，未据此修改行为代码。MSR/MNP 阈值仍为 0.50，fallback/hold 仍按
现有产品逻辑工作，摄像头、LCD transport 和 openvela 官方源码均未修改。Stage B
需要新的设计批准；候选最小修复方向是在 AgentGuard vendored ESP-DL 兼容层中，
于 MNP 预处理前拒绝裁剪后退化的 MSR ROI，并为此先建立可复现测试，不直接改官方
openvela 源码。

### Stage B：拒绝裁剪后退化的 MNP ROI

用户批准进入 Stage B，并确认 bounded 设计：仅在 AgentGuard vendored 人脸检测兼容
层中抽取可主机测试的 ROI 归一化函数；保持原有正方形化与边界裁剪，对裁剪后不满足
`x2>x1 && y2>y1` 的候选在 MNP preprocess/model/postprocess 前直接跳过。正常框和
部分越界但裁剪后仍有面积的框保持原行为；MSR/MNP 0.50 阈值、fallback/hold、摄像头、
LCD 和 openvela 官方源码均不变。

TDD RED：新增 `test_espdl_roi_filter.cpp` 及测试 Makefile 目标，测试手算验证居中正常框、
跨左边界但裁剪后有效的框、完全位于右边界外并退化为零宽的框。首次构建按预期因
生产头 `human_face_detect_roi.hpp` 不存在而失败。GREEN：新增该 header-only 辅助函数，
`MNP::run()` 用返回值过滤退化框；补齐真实 ESP-DL include 路径并将 vendored 头标为
system include，避免其既有警告被主机测试的 `-Werror` 提升为错误。新测试输出
`AgentGuard ESP-DL ROI filter tests: PASS`，随后全部 AgentGuard 主机测试 PASS，
工作区 6 项结构检查全部 `OK`，`git diff --check` 无输出。

首次 NuttX 目标构建已成功重新编译 `human_face_detect.cpp` 并链接 `nuttx`，证明 Xtensa
实际目标代码编译通过；仅在生成 ESP32-S3 镜像时因未激活 Python 环境、找不到
`esptool.py` 而退出。随后尝试激活已有 `/home/yhx/Desktop/openvela/myenv` 后重跑，
沙箱禁止写入 workspace 外的 NuttX 构建目录；请求标准目标构建提权时，自动审批服务
再次发生网络响应解码中断并拒绝，命令未执行。当前尚未完成最终镜像、提交或烧录，
也尚未做 Stage B 实机 EMPTY/FACE/PALM 复验；必须取得用户在获知该阻塞后的明确批准
再重试同一标准构建命令，不得绕过审批。

用户在获知审批阻塞后明确批准重新构建并烧录。激活已有 `myenv` 后最终 NuttX 构建
成功生成 ESP32-S3 镜像；烧录前为 1,955,028 bytes，SHA-256
`7811810a826bdb23b3fb1e781c9eacb9b1f135299868498d0cf21d31d76aa56e`。
只读检查确认 USB `303a:1001` 与 `/dev/ttyACM0` 存在且串口未被占用。esptool v5.3.1
识别 ESP32-S3 rev 0.2、8 MB PSRAM 和 8 MB flash，写入 1,955,028 bytes 后报告
`Hash of data verified`，并通过 RTS 硬复位。flash 目标重新链接版本元数据后的最终镜像
仍为 1,955,028 bytes，SHA-256
`ef5074c73c0d8550fa254d1973394eb0bc92919066f7395e2dbfa32571bd2f4c`。
下一步按 EMPTY、FACE、PALM 顺序实机复验：退化候选应显示 MNP attempts/accepted 与
raw neural faces 为 `0/0`、`0`；正常真人若产生有效 ROI，则仍应进入 MNP 并显示人脸。

Stage B 首个实机 `EMPTY` 结果通过：用户保持画面完全无人并观察多个诊断代次后，LCD
显示 `M:0/0 F:0` 与 `NO PERSON`。这证明裁剪后退化的 MSR 候选已在 MNP 前被过滤，
不再执行 MNP、不会被接受，也不再产生原始神经人脸。仍需 FACE 验证正常有效 ROI
没有被误过滤，并用 PALM 验证非人手掌不再触发人脸。

Stage B `FACE` 实机复验未满足成功条件：用户让真人稳定居中后仍为 `M:0/0 F:0`，
但 LCD 出现一个覆盖脸部以下很大区域的框。`M/F` 证明 ESP-DL 神经路径没有进入
MNP、没有输出人脸；该大框来自现有肤色 fallback，而非神经人脸框。Stage B 的
退化 ROI 过滤消除了空场景假阳性，但同时证明真人的 MSR 候选也会裁剪成退化框；
因此不能把当前固件宣称为人脸检测修复完成。

系统化回溯排除了两个旧假设。其一，推理前旋转已在提交 `d1215e4` 做过单变量实机
实验，真人结果仍固定，随后由 `cc17b2e` 按预案回退，不能重复猜测方向。其二，当前
合并 PDL2 模型的两段数据已逐字节核对：offset `0x70` 的 129,968 bytes 等于官方
S3 MNP 模型，offset `0x1fc20` 的 61,168 bytes 等于官方 S3 MSR 模型，header 名称
顺序正确，排除模型名/数据错配。MSR/MNP 后处理与 ImagePreprocessor 逻辑也和工作区
官方参考一致；预处理仅增加 caps setter，普通/深度卷积的差异仅是既有的强制 C 后端。

内置 `human_face_rgb565be.bin` 已解码确认是清晰正向的蒙娜丽莎，但 Stage A 快照中
其 MSR 输入全零，说明 Flash/DROM 参考图路径本身不能作为实时 PSRAM 摄像头的正确
基准。当前最小证据缺口是 MSR 裁剪前的有符号候选坐标：官方 `get_result(width,
height)` 会先把候选原地裁剪到图像范围，现有诊断随后才读取，因而丢失画外方向与
距离。下一诊断必须在 `get_result()` 前保存 top candidate 的原始 `x1/y1/x2/y2/
score`，并在裁剪后保存同一候选，才能区分 MSR 解码错误与 ROI 变换错误；未经新的
短设计批准不得继续修改。

用户确认实施上述 bounded 只读诊断。先将已验证的退化 ROI 过滤独立提交：

```text
4d3b00b fix: reject degenerate face ROIs
```

随后按 TDD 扩展 MSR 候选证据。第一轮 RED 在测试调用缺失的有符号候选类型与
`capture_candidate()` 时编译失败；GREEN 新增 `ag_face_candidate_diagnostic`，原样
保存 int32 `left/top/right/bottom`、0–1000 分数和有效位，并在 trace 中增加
`msr_candidate_before_clip`/`after_clip`。测试验证正常框、负坐标、完全位于右边界外
以及裁剪后 `319..319` 退化框均不丢失或无符号回绕。第二轮 RED 因缺少
`capture_top_candidate()` 失败；GREEN 验证按后处理排序记录 `front()`，空列表明确
无效。

生产接入在 `MSR::run()` 的 `postprocess()` 后、`get_result(width,height)` 前保存
最高原始候选，再在官方裁剪后保存同一排序位置；不修改候选、阈值或决策。为避免
触碰当前 contest git 未跟踪的通用 ESP-DL 目录，只在已跟踪的 AgentGuard
`human_face_detect.cpp` 内使用 `MSRTracePostprocessor` 只读派生类访问 protected
结果列表；曾临时添加到通用后处理 header 的一行已撤回，并用 `cmp` 确认该文件与
工作区官方参考逐字节一致。全部 AgentGuard 主机测试两次 PASS，工作区 6 项检查
全部 `OK`，`git diff --check` 无输出。

下一步目标构建请求未执行：自动审批服务再次发生网络响应解码中断并拒绝。当前尚未
生成/烧录扩展坐标诊断固件，诊断提交也尚未创建。必须在向用户说明后取得明确批准，
再重试标准 `myenv + make -C openvela/nuttx -j8`；构建成功后必须由 ELF/DWARF 重新
获取扩展 store 地址、结构大小和字段偏移，禁止沿用 Stage A 的 896 字节布局。

扩展坐标诊断随后完成，提交为 `60dcc1a diag: capture unclipped MSR candidates`。首次
增量目标构建暴露旧对象 ABI 混用：C++ trace 为 440 bytes，而未重编译的 C/vision
对象仍按 400 bytes；该不安全镜像未烧录。将 8 个精确相关对象可恢复地移至
`/tmp/agentguard-abi-KinZJA/` 后用标准构建重新生成，最终一致验证 store/snapshot/
trace 分别为 976/948/440 bytes。诊断 store ELF 地址为 `0x3fccc014`，snapshot 位于
store `+28`，live trace 位于文件 `+528`，裁剪前/后候选位于文件 `+616/+636`，计数
位于 `+960..965`。最终烧录镜像 1,955,020 bytes，SHA-256
`a57d4e9a0d77c19071448c9dacf6e2324c3bbc9161e14304888121be0f92241e`，烧录校验通过。

2026-08-23 只读 JTAG 实机对照已完成。VMware USB 直通曾卡死为幽灵设备，重新连接后
从 Device 018 依次枚举为 019/020；udev 会把节点恢复为 0664，因此采集时用短时权限
监视器在用户执行精确 `sudo chmod 666 /dev/bus/usb/002/020` 后立即连接。空场景快照
`/tmp/agentguard-msr-empty.bin` 序号 16841：裁剪前和裁剪后均为
`(0,0,0,0), score=577 permille, valid=1`，计数 `1/0/0/0/0/1`。人脸快照
`/tmp/agentguard-msr-face.bin` 序号 191：结果逐字段完全相同。两次转储都是 976 bytes；
空场景 SHA 前缀 `fb4647e90ddb753b`，人脸 SHA 前缀 `241d96bdcb597668`。空场景采集后
另行确认 `CPU0=running / CPU1=running`；人脸采集命令也在 dump 后执行 `resume`。

场景数据本身并未固定：空场景 live RAW hash `25f4d48f`、MSR input `7615415e`，人脸
live RAW hash `308e33ad`、MSR input `7dfa45e2`；两场景 score0/box0/score1/box1 哈希
也全部不同。由此可排除摄像头场景不变、模型输出张量固定、`get_result()` 裁剪和
ROI filter。首个分歧已前移至 MSR `parse_stage()` 的坐标生成。该实现把四个解码坐标
统一乘以 `ImagePreprocessor::get_resize_scale_{x,y}(true)`；当前四坐标精确同时为 0，
而 score 正常为 0.577，最强单变量假设是预处理器逆缩放系数仍为初始化值 0。下一步
只记录 MSR preprocess 后的正向/逆向 x/y scale（定点数），确认该假设后才允许修改
行为；阈值、模型、LCD、fallback/hold 与 openvela 官方源码保持不变。

缩放诊断按 TDD 完成并提交为 `7346b04 diag: capture image resize scales`。RED 测试因
缺少 scale 类型/采集函数编译失败；GREEN 将 x/y 正向和逆向 scale 以百万分之一
记录，零值也是有效观测，不参与任何决策。全部 AgentGuard 主机测试 PASS，6 项工作区
检查 OK，`diff --check` 无输出。为避免旧对象 ABI 混用，再次只移动 8 个明确引用 trace
的对象到 `/tmp/agentguard-scale-abi-tiQnxE/`，标准构建明确重编译其双份对象。最终 ELF
store/snapshot/trace/scale 为 1016/988/460/20 bytes；store 地址 `0x3fccc054`，live
trace 文件偏移 548，scale/裁剪前/裁剪后/计数分别为 628/656/676/1000。镜像
1,955,020 bytes；烧录写入相同大小，报告 `Hash of data verified` 并硬复位。

用户重新明确批准只读 JTAG 后，人脸快照 `/tmp/agentguard-scale-face.bin` 采集成功，
1016 bytes，SHA-256 `0510f3f10a7e42d3ba5f72c9c72eeb0f4e97b5148da309ebfad822200542883d`，
序号 441。四个缩放值 `x/y/inv_x/inv_y` 均为 `0/0/0/0` 且 `valid=1`；同帧 RAW
hash `673efcf0`，MSR input hash `dda2bf4a`，输入范围 -128..127，四个输出张量均有效且
变化，候选仍为 `(0,0,0,0), 577 permille`，计数 `1/0/0/0/0/1`。采集后独立确认
`CPU0=running / CPU1=running`。最强假设由此得到直接证实：MSR 坐标归零是因为
ImagePreprocessor 首次全帧空 crop 未在该目标环境建立 resize scale，而非模型或框
解码数值固定。

下一 bounded 修复保持在已跟踪 AgentGuard 人脸适配层：把语义相同的全帧预处理从
空 crop 显式写成 `{0,0,width,height}`。ESP-DL 既有 `set_src_img_crop_area()` 会在
empty -> full-frame 变化时设置 `m_gen_xy_map=true` 并生成 scale；像素范围和采样区域
不变。MNP 已为每个候选传显式 crop，不需修改。先为全帧 crop helper 做主机 RED/GREEN，
再实机验证 scale 应为约 2.0/2.0 的逆比例、MSR 框应非退化；不修改官方 openvela 或
通用 vendored ESP-DL 源码。

显式 full-frame crop 固件实机仍为 scale `0/0/0/0`、候选零框，因此该假设被证伪；
快照 `/tmp/agentguard-scale-fixed-face.bin` 为 1016 bytes，SHA-256
`2b07d0b9e723e3555cb2d664b756d195b525261141efa41727b38ce6f7888bc1`，序号 28，采集后
双核均为 running。实验提交 `816e3c8 fix: initialize face resize mapping` 已用独立提交
`a371a1a Revert "fix: initialize face resize mapping"` 完整撤销，保留审计历史。

真正根因随后由目标反汇编确认。AgentGuard ESP-DL 兼容头 `compat/espdl/esp_log.h` 把
`ESP_ERROR_CHECK(value)` 定义成 `assert((value) == 0)`，而 NuttX 配置明确
`CONFIG_NDEBUG=y`。标准 C/C++ 的禁用 assert 不会求值参数，因此
`ImagePreprocessor::preprocess()` 中被该宏包裹的
`set_src_img(...).set_src_img_crop_area(...).transform()` 整条表达式被编译器删除。
目标 `dl_image_preprocessor.cpp..._2.o` 反汇编显示 preprocess 在设置背景后直接
`retw`，没有 `set_src_img` 或 `transform` relocation/call。这完整解释 scale 永远为 0、
MSR 坐标全零、预处理 fingerprint 实为 minimize 后复用张量以及场景间伪变化。

TDD RED 新增 `test_espdl_error_check.c`，以与目标一致的 `-DNDEBUG` 编译；旧宏测试打印
`ESP_ERROR_CHECK skipped its expression` 并退出 1。GREEN 将兼容宏改为始终把表达式
求值一次，非零时打印错误码/文件/行并 `abort()`，与官方宏的 fail-fast 语义一致；测试
PASS，全部 AgentGuard 主机测试 PASS，6 项结构检查 OK，`diff --check` 无输出。该修复
仅涉及 AgentGuard 兼容层、测试和测试 Makefile，不改官方 openvela 源码。为强制消除
所有旧宏代码，已将实际引用宏的 fbs_loader、ImagePreprocessor、dl_tool 共 6 个目标
对象可恢复地移至 `/tmp/agentguard-error-check-vdqDeb/`。

当前阻塞：请求标准目标构建时，自动审批服务发生远端 compact/响应解码网络中断并
拒绝，命令未执行。按系统要求不能绕过或重复同一操作，需用户在获知后明确批准重试
`myenv + make -C /home/yhx/Desktop/openvela/nuttx -j8`，成功后必须反汇编确认 transform
调用恢复，再提交、烧录并用现有 JTAG scale 诊断实机验证。

用户两次明确批准后审批服务恢复，标准目标构建成功并明确重编译 dl_tool、fbs_loader、
ImagePreprocessor 和 human_face_detect。两份 ImagePreprocessor 目标对象反汇编均出现
`set_src_img`、`set_src_img_crop_area`、`ImageTransformer::transform<false>` 及错误
分支 `abort` relocation，证明 release 固件不再删除预处理表达式。修复提交为
`cbd7b3c fix: evaluate ESP-DL error checks in release builds`；不推送。最终烧录镜像
2,283,604 bytes，SHA-256
`355e5b856d80549ee31fcda5ad6fafb4f2ed2dd723af85c3300374fddfd1df77`，写入后报告
`Hash of data verified` 并硬复位。镜像相对旧版增长约 328 KB，符合此前被错误消除的
预处理模板代码重新链接。

修复后真人居中 JTAG 快照 `/tmp/agentguard-preprocess-fixed-face.bin` 为 1016 bytes，
SHA-256 `e1a6ad9645bb296c0f2dd4a6d5d4be46e78ee550801101d5f85264deaaceadf1`，序号 9。
scale 从旧版 `0/0/0/0` 修复为正向 `500000/500000`、逆向
`2000000/2000000` millionths，即 320x240 到 160x120 的 0.5 和回映射 2.0。MSR
裁剪前最高框从零框变为 `(17,168,135,262), score=0.577`，裁剪后为
`(17,168,135,239)`；输入范围从伪复用的 -128..127 变为真实预处理的 0..126。
计数为 candidates/attempts/recorded/accepted/faces/valid = `5/5/4/5/5/1`，采集后
`CPU0=running / CPU1=running`。这证明预处理主故障已修复、MSR/MNP 全链路首次产生
非退化框；但真人场景同时有 5 个最终结果，尚可能存在阈值误检或多框，不能宣称产品
检测完成。下一步必须采集 EMPTY，再按 FACE/PALM 对比 false positives 和框稳定性。

修复后空场景 JTAG 对照 `/tmp/agentguard-preprocess-fixed-empty.bin` 同为 1016 bytes，
SHA-256 前缀 `adfc998ec8daea2a`，序号 748。RAW 与真人快照不同，五组已记录张量哈希
也都随场景变化；scale 仍为正确的 `500000/500000/2000000/2000000`。然而最高候选、
四个 MNP crop 与计数都和真人场景相同，仍为约 `(17,168)-(135,262)` 和
`5/5/4/5/5/1`。MNP 分数张量在两个场景均接近饱和（99%），说明修复后的真实预处理
已让模型看到不同数据，但当前 mixed C/TIE 后端仍把空场景与人脸场景接受为相同五框。
修复预处理之前得到的后端判断因当时 preprocess 表达式被删除，不能继续作为依据。

因此按单变量重新隔离后端：提交 `fa217f0 diag: isolate portable ESP-DL backend` 将
`CONFIG_AGENTGUARD_ESP_DL_FORCE_C` 从 0 改为 1，并用编译期测试确认
`CONFIG_XTENSA_BOOST=0`、`CONFIG_TIE728_BOOST=0`，保留已有普通/深度卷积强制 C。
TDD RED 先在旧配置下命中 3 个 static assertion，GREEN 后目标测试和全部 AgentGuard
主机测试 PASS，6 项工作区检查 OK，`diff --check` 无输出。执行完整 `make clean` 后
标准目标构建成功；实际 conv2d/depthwise/max-pool 对象无 TIE/Xtensa 未解析引用。
固件 2,218,076 bytes；构建时 SHA-256
`686e7a6ff20d98b9216ed8f809238a8862e6fe6fd0c6e3c258ad621db1fa53d3`，烧录阶段重新链接
后的 SHA-256 为 `9da49c12cbc1e8314b58a3d0ba08d208f3d1f7c0967920e9f1ae012e3ea8fec5`。
烧录报告 `Hash of data verified` 并硬复位；没有修改 openvela 官方源码，也未推送。

全 C 固件最终 ELF 诊断 store 地址为 `0x3fccac54`，大小和 ABI 仍为 1016 bytes。
只读 JTAG 空场景快照 `/tmp/agentguard-full-c-empty.bin` 采集成功，SHA-256
`3adfb0c2525631f96aa39b273451f0180ee07aa0a2ff95a81651292e145b8aca`，序号 41。
scale 正确为 `500000/500000/2000000/2000000`；最高候选裁剪前
`(17,167,135,262), score=577 permille`，裁剪后 `(17,167,135,239)`；计数仍为
`5/5/4/5/5/1`。所以空场景五框误检并非剩余 TIE/Xtensa 优化算子造成。下一步仅采集
同一全 C 固件的真人 FACE 对照；若仍同样饱和，则首要方向转为 portable C 算子/模型
量化或权重装载，不在取得场景对照前调整阈值。

JTAG 转储事务在成功和异常路径均执行 `resume`；采集后的独立 OpenOCD `resume` 检查
返回 `cpu0 not halted`，确认目标已在运行而不是遗留为暂停状态。

## 2026-09-05 截止期收敛：全 C ESP-DL 真人对照

从提交 `1fc8f9b build: make AgentGuard source tree reproducible` 对应源码执行完整
`make clean` 和 ESP32-S3-EYE 目标重建，烧录后固件为 2,218,068 bytes，SHA-256
`88a581efc5cb75e23261226cb5e14ad89e344183c97f1611a8fd6913ff86cf5e`；esptool
报告 `Hash of data verified`。镜像低于 `0x300000` LittleFS 边界，ESP32-S3
image-info checksum 有效。内嵌 `human_face_detect.espdl` 模型跨度为 191,248 bytes。

目标 ELF 的 `(anonymous namespace)::g_face_diag_store` 地址为 `0x3fccac54`、大小为
`0x3f8`（1016 bytes）。离线 target GDB 从 `struct ag_face_diag_store`、
`struct ag_face_diag_snapshot` 和 `struct ag_face_detector_trace` 解出并确认以下相对
偏移：sequence 28、raw hash 60、live MSR input hash 548、resize 628、候选裁剪前
656/score 672、裁剪后 676/score 692、六个计数/有效位 1000..1005、snapshot valid
1012。六份转储均为 1016 bytes，两个有效位均为 1；resize 六次均为
`500000/500000/2000000/2000000`。

三份 EMPTY 空场景样本如下（字段依次为：快照 SHA-256；sequence；raw hash；live MSR
input hash；裁剪前框/分数；裁剪后框/分数；candidates/attempts/recorded/accepted/
faces/live-valid）：

- EMPTY-1：`bcbdfc63e7de7176aa35d3919e8f7af1c883f8a124341a1491757946d5d3946b`；
  `313`；`ec061b4d`；`fbe10bf7`；`(17,167,135,262)/577`；
  `(17,167,135,239)/577`；`5/5/4/5/5/1`。
- EMPTY-2：`d7eb5daeac32880e491c28dec7031faf328cfcd5eacc401a61b0c70b998c02d1`；
  `336`；`a9b8b4ea`；`7256ec5f`；`(17,168,135,262)/577`；
  `(17,168,135,239)/577`；`5/5/4/5/5/1`。
- EMPTY-3：`25bcdc2d7ca3d8d190bb8c5d3965fdad6c70eeba8a59b9e8016968ba11c64d17`；
  `356`；`463d58a0`；`7bf68c2f`；`(17,168,135,262)/577`；
  `(17,168,135,239)/577`；`5/5/4/5/5/1`。

三份 FACE 单真人居中样本如下：

- FACE-1：`9006c64a49ed1df5a571e7bf621eb34156a26058f2bfc373474b74a048e1878d`；
  `379`；`2a437f40`；`f346cf47`；`(17,168,135,262)/577`；
  `(17,168,135,239)/577`；`5/5/4/5/5/1`。
- FACE-2：`a6da0754d5d911e303729d88b2cb01f2baa639bb81d9531092e80bd8c512dae7`；
  `398`；`e73935a9`；`a21a7c35`；`(17,167,135,262)/577`；
  `(17,167,135,239)/577`；`5/5/4/5/5/1`。
- FACE-3：`ad4f0d0701cbd856a34d2f88f889cb5cbe4c19da0ce400814a9ef143ab5ed19a`；
  `417`；`ae80bda2`；`5a569c13`；`(17,167,135,262)/577`；
  `(17,167,135,239)/577`；`5/5/4/5/5/1`。

相邻样本 sequence 分别增加 23、20、23、19、19，超过至少三个完整推理周期的门槛；
raw 和 live MSR input 哈希每次都变化，证明实时相机帧确实进入预处理。每个 dump
事务都包含 `resume`。六次采集结束后的独立 OpenOCD 恢复事务返回
`[esp32s3.cpu0] not halted`，证明 CPU0 已经在运行，没有遗留暂停状态。

**Decision: `REPLACE_MODEL`。** 决策表命中原因：empty and face samples retain the
same accepted count and effectively identical boxes/scores；all scenes saturate to the same
false-positive pattern；only raw-frame hashes differ while neural decision fields do not。
六个样本的接受数和最终数均为 5、分数均为 577 permille，框仅有 1 像素抖动，无法
构造不依赖场景坐标或肤色的确定性后处理规则。此次证据门禁没有修改阈值、模型、
ESP-DL 代码或产品行为。下一步应制定有明确官方版本、来源、许可证、内存预算和回滚点
的模型替换方案，而不是继续调阈值。

## 2026-09-05 官方 ESP-DL A/B 最小验证

为区分“模型本身错误”和“openvela/NuttX 适配路径错误”，在 `/tmp` 中从未修改的
ESP-DL `v3.2.0`（commit `dc380d450835d42f92777121a0cd4fc67d7c3a8c`）构建官方
`examples/human_face_detect`，没有把 probe 或官方工程改动写入仓库。最初使用本机
ESP-IDF 6.0.2 时，官方 `esp-dsp` 依赖与 C++26 不兼容；改用 Espressif 官方维护的
ESP-IDF `release/v5.4`（commit
`62c1a66ebaff1b12db25eb0a56ceba297e11f1df`，识别为 5.4.4）及官方
GCC `esp-14.2.0_20260121` 后构建成功。经用户批准安装的 Python 构建依赖包括
`tree-sitter 0.26.0` 与 `tree-sitter-c 0.24.2`，安装在 ESP-IDF 5.4 独立环境
`idf5.4_py3.10_env`，不影响项目源码。

官方示例镜像为 1,562,528 bytes，SHA-256
`eb604eeffff892c68d6a043e18256ffde60a291bbeefed6ca0edbc40419ae1a5`。在同一块
ESP32-S3-EYE 上运行官方内嵌人脸测试图，串口明确输出一张人脸：score
`0.899121`，框 `(100,65)-(194,189)`，并给出双眼、鼻和双嘴角五个关键点。这是
官方模型、官方 ESP-DL 运行时和同一硬件组合的正对照；它证明模型文件并非天然只能
输出当前 NuttX 路径中的固定五框。该示例使用内嵌测试图，不等价于实时摄像头验收，
但已经把故障边界收敛到当前 NuttX ESP-DL 兼容/算子/模型装载路径，而不是继续盲目
更换阈值或模型。

官方示例写入范围低于约 `0x190000`，未触及 AgentGuard 从 `0x300000` 开始的
LittleFS 数据区。随后标准 NuttX 构建与烧录成功，esptool 报告
`Hash of data verified`。恢复前备份镜像 SHA-256 为
`88a581efc5cb75e23261226cb5e14ad89e344183c97f1611a8fd6913ff86cf5e`；重新链接并烧录的
镜像 SHA-256 为
`e7b195d768ca4452631740e6792817015f32862b4306f2532f25f15dc5d25472`。两者均为
2,218,068 bytes，逐字节比较只有偏移 2,073,431、2,073,433、2,073,434、
2,073,436 四个字节不同，对应内嵌构建时间从 `20:43:44` 变为 `22:27:04`；其余字节
一致。因此恢复镜像与备份的程序内容等价。恢复后的串口 `pidof/stats` 只读确认因平台
外部执行审批链路连续中断尚未补做，不能把该项记为已通过。

**Decision: `FIX_NUTTX_ADAPTER`。** 官方 A/B 已推翻上一节基于 NuttX 异常输出作出的
`REPLACE_MODEL` 优先级判断。截止期内的最小可行路线是：保留官方 v3.2.0 人脸模型，
以官方示例的单脸结果为 golden reference，逐层对照 NuttX 模型加载、量化张量布局及
portable C 算子；先保证 0/1/2+ 人数档位和单脸框，不在该门禁通过前做主人身份识别。

### 同一 RGB565 输入与第一轮兼容层收敛

官方探针进一步直接内嵌并运行 AgentGuard 使用的同一份 320x240
`human_face_rgb565be.bin`。官方 ESP-DL v3.2.0 在同一开发板上输出恰好一张人脸，score
`0.904651`、框 `(96,64)-(194,191)`；原 JPEG/RGB888 路径仍为 score `0.899121`、
框 `(100,65)-(194,189)`。因此可同时排除测试图转换、RGB565-BE caps、模型文件和硬件，
后续只修 NuttX/openvela 兼容边界。

按“尽可能保持 ESP-DL 算法层原样”的要求，第一轮单变量恢复了官方
`human_face_detect.cpp/.hpp`，移除其中 AgentGuard 私有 trace、ROI 和运行模式改写；
`vision_espdl.cpp` 改为只调用官方 `run()`/threshold API，并在 adapter 外侧汇总最终
人数和最高分。新增上游 wrapper SHA-256 完整性门禁，已完成预期 RED（两个文件均不匹配）
和 GREEN（匹配 v3.2.0），全部 AgentGuard 主机测试通过，目标交叉构建成功。固件
2,218,068 bytes，SHA-256
`2ec0277ef7b5953b3cc0a2f878af98d4bbdab7f229a3535609eca999592cfee6`，烧录校验通过。
本轮实机 `face-diag` 只读查询在用户明确批准后仍因平台审批服务网络解码中断而未执行，
所以“参考图已由 5 变为 1”仍是待验证项，不能提前宣称修复完成。
