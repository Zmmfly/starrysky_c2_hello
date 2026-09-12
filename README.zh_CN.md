# StarrySky C2 Hello 与 Echo

[English](README.md) | 简体中文（zh_CN）

基于 xhive 构建的 StarrySky C2 裸机 SYS_UART 示例。固件在启动时输出一次问候语，
随后将接收到的字节原样回显。

## 目录

- [项目概述](#项目概述)
- [环境要求](#环境要求)
- [构建](#构建)
- [烧录与运行](#烧录与运行)
- [回显测试](#回显测试)
- [验证状态与限制](#验证状态与限制)
- [参考资料](#参考资料)

## 项目概述

板载 CP2102 USB 转串口连接 SYS_UART。应用采用轮询方式，不使用 RTOS、中断、
定时器延时或软件接收队列。启动问候语输出完成后，回显数据不添加前缀，也不转换
换行符。包括 `0x00` 和 `0xFF` 在内的二进制值均按字节处理。

回显循环放在内部 SRAM（`.ramfunc`）中执行，并启用 LTO 内联 UART 调用，避免收发
关键路径从 Flash XIP 取指，不增加中断或队列。这是针对下文突发丢字问题的修订候选，
仍需重新进行实板测试。

启动输出：

```text
Hello World [SRAM/LTO v2] from xhive vendor: opencos / StarrySky C2!
```

`[SRAM/LTO v2]` 标识用于区分本次诊断固件与此前版本。应先打开串口监视器，再按复位键，
以捕获这条只输出一次的标语。

仓库中的 [.config](.config) 采用以下配置：

| 配置项 | 值 |
| --- | --- |
| 目标 | StarrySky C2，RV32IMC / ILP32 |
| 串口 | SYS_UART，115200 波特率，8N1，无流控 |
| 板载晶振 | 72 MHz |
| Flash | 16 MiB，起始地址 `0x00000000` |
| 内部 SRAM | 128 KiB，起始地址 `0x30000000` |
| 栈 | 内部 SRAM 中的 1 KiB，按 16 字节对齐 |
| 外部 RAM | 禁用 |
| 执行配置 | `CONFIG_ENABLE_EXEC_IN_RAM=y`，`CONFIG_COMPILER_ENABLE_LTO=y` |
| 启动与链接脚本 | xhive 默认模板 |

`CONFIG_STARRYSKY_C2_CLOCK_HZ` 描述实际晶振频率；修改该配置不会改变硬件时钟。

## 环境要求

**当前主机环境：Linux / Ubuntu 26.04。** 本工程的实现、固件构建及主机侧软件检查
均在该环境下完成。C2 固件本身仍是裸机程序；Ubuntu 运行在开发主机上。

- 已准备好 RISC-V GCC 工具链及配置工具的 [xhive SDK](https://github.com/Zmmfly/xhive) 工作副本。
- Xmake，以及 SDK 所需的 Python 配置依赖，包括 `kconfiglib`。
- Python 3 和 `pyserial`，用于串口监视器及回显测试。
- Linux，以及 `lsblk`、`cp`、`sync`，用于工程内的板级烧录任务。
- 对已挂载烧录盘和串口设备的访问权限。

### 操作系统适用范围

| 流程 | 约束 |
| --- | --- |
| 构建 | 已在 Ubuntu 26.04 上验证；示例命令使用 POSIX shell 和 `realpath` |
| 烧录辅助任务 | 明确限制为 Linux；依赖 `lsblk`、`cp` 和 `sync -f` |
| 串口监视与回显测试 | 已在 Linux 测试；测试脚本也接受 Windows COM 端口，仅在 POSIX 上请求独占打开 |
| Windows 拖拽烧录 | 用户已确认通过 HFP-LINK 更新固件；不代表 Windows 构建或 Linux 辅助任务通过验证 |
| 其他环境 | 其他 Linux 发行版、Windows、macOS 和 WSL 尚未验证本工程的完整工作流程 |

Ubuntu 26.04 是当前验证基准，不代表最低版本要求。烧录、逐字节回显和突发回显的
验证结果分别记录，详见[验证状态与限制](#验证状态与限制)。
构建前应完成 SDK 和 Python 环境配置，不要求使用任何特定用户的安装目录。

## 构建

在本工程根目录执行命令。以下示例假设 xhive 工作副本位于同级的 `xhive` 目录；
若目录布局不同，请相应设置 `XHIVE_SDK_PATH`。

```sh
export XHIVE_SDK_PATH="$(realpath ../xhive)"
xmake f -y
xmake
```

仅构建应用目标时，使用 `xmake build c2_hello`。需要清理生成产物后重新构建时，
先执行 `xmake clean`，再执行 `xmake`。

| 路径 | 用途 |
| --- | --- |
| [src/main.c](src/main.c) | 启动问候语及回显循环 |
| [xmake.lua](xmake.lua) | 构建、烧录和串口监视任务 |
| [tests/test_echo.py](tests/test_echo.py) | 逐字节串口测试 |
| `dist/c2_hello.elf` | 链接后的 RISC-V 可执行文件 |
| `dist/c2_hello.bin` | 用于烧录的裸固件镜像 |
| `.vscode/compile_commands.json` | 自动更新的编译数据库 |

BIN 使用工具链的 `objcopy -O binary` 直接从 ELF 生成，不附加额外镜像头、校验尾，
也不进行固定大小填充。`xmake.lua` 当前未启用链接映射文件生成。
`build/`、`dist/`、`.xmake/` 和 `.vscode/` 是已从版本控制中排除的生成目录。

SDK 必须包含 LTO 下保留 `Reset_Handler` 的修复（`templates/startup_riscv.c` 中的
`used` 属性）。启动代码在调用 `main` 前将 `.ram_text` 从 Flash 复制到 SRAM。
更换编译器或修改驱动后，应检查 ELF 的 `.ram_text` 反汇编，确保回显循环不再调用
Flash 中的函数。仅将调用者放进 `.ramfunc` 不会自动搬移被调用者。

## 烧录与运行

C2 Pi 板卡通过物理模式开关选择 HFP-LINK 烧录模式或 UART 运行模式。
烧录器存储卷和 CP2102 串口接口不能同时使用。

用户已在 Windows 下将 BIN 拖到 `YSYX-HFPLnk`，成功更新了此前在 Flash 中执行的
回显固件。可用这一已确认的方法烧录新的 `dist/c2_hello.bin`，随后切回 UART 运行
模式并复位。等待启动问候语输出结束后再发送数据。

下述 Linux 辅助任务在当前设备上**尚未确认更新成功**。复制完成或主机侧文件哈希
一致，不属于 Flash 回读验证。

1. 选择 HFP-LINK 烧录模式，挂载标签为 `YSYX-HFPLnk` 的存储卷。
2. 执行 `xmake flash --dry-run`，构建并检查目标位置，不写入设备。
3. 执行 `xmake flash`。任务先构建应用，再以 `retrosoc_fw.bin` 为文件名复制两次，
   两次之间间隔 0.5 秒，每次复制后同步文件系统，最后显示 `STATE.TXT`。
   复制或同步失败时，任务会停止。
4. 等待烧录器活动结束，切换到 UART 运行模式并复位开发板。开关位置以实物丝印为准。
5. 执行 `xmake monitor`。仅检测到一个 CP2102 时会自动选用它；按 Ctrl+] 退出。

连接多个烧录器时，将 `HFP_MOUNT` 设为目标存储卷的实际挂载根目录，再执行
`xmake flash --mount="$HFP_MOUNT"`。任务会检查挂载标签和写入权限，不接受任意目录。

显式选择串口设备：

```sh
xmake monitor --port=/dev/ttyUSB0
```

`/dev/ttyUSB0` 是设备名示例，请以操作系统实际分配的设备为准。关闭终端的本地回显，
避免字符重复显示。要查看启动问候语，应先打开监视器再复位；应用不会周期性重复输出。

双次复制行为参考了文末所列的 ECOS SDK 脚本，但它不是已确认的 HFP-LINK 协议要求，
也不是已经验证有效的更新失败修复。串口监视器只是控制台，不是 GDB 服务。

## 回显测试

确认新固件已经启动后，关闭其他串口监视器并执行：

```sh
python3 tests/test_echo.py /dev/ttyUSB0
```

测试配置为 115200 8N1、关闭流控，并读取初始输出。默认执行两种模式：

- `byte`：共 273 字节，每收到一个字节的正确回显后，才发送下一个。
- `burst`：将 `Hello`、`Hello\r\n`、`0123456789` 和全部 256 个字节值分别整包写入，
  每种重复 20 次。包间等待完整回显，包内不逐字节等待。

出现不匹配、超时、写入不足或尾随多余字节时，测试失败。可用 `--mode byte` 或
`--mode burst` 单独测试，用 `--repeat 100` 增加突发测试次数。Windows 下可执行
`python tests/test_echo.py COM3`，将 `COM3` 换成实际端口；脚本不会传入仅适用于
POSIX 的独占打开参数。

测试成功时的预期输出：

```text
PASS: 273 stop-and-wait bytes at 115200 8N1
PASS: 80 bursts (5, 7, 10, 256 bytes) at 115200 8N1
Coverage: text, CR/LF, and every byte from 0x00 through 0xff
```

有限突发测试不是持续吞吐量或 FIFO 容量验证。启动问候语发送期间到达的数据不在
验证范围内。无需硬件即可运行测试脚本自身的检查：

```sh
python3 -m unittest discover -s tests -p test_echo_cli.py -v
```

## 验证状态与限制

| 检查项 | 状态 |
| --- | --- |
| 主机环境 | Linux / Ubuntu 26.04 |
| 固件构建 | 已使用 xPack RISC-V GCC 15.2.0 验证通过 |
| 主机回显测试脚本 | 模拟端口检查通过：逐字节/突发模式、错误拒绝和 Windows 参数选择；不是固件仿真 |
| 早期仅 Hello 固件 | 已在 C2 实板观察到 SYS_UART 发送输出 |
| Windows HFP-LINK 更新 | 用户已确认运行 Flash 版本的回显固件及其启动问候语 |
| Flash 版本回显实板运行 | 273 字节逐字节测试通过；Windows 报告的突发丢字也已在 Linux 复现 |
| SRAM/LTO 修订候选 | 构建及反汇编检查通过；Ubuntu 烧录后复位仍输出旧版启动标语 |
| 独立 Flash 回读 | 未执行 |
| OpenOCD/GDB 硬件调试 | 本工程尚无经过验证的流程 |

Flash 版本固件在 Linux 实板检查中，整串 `Hello` 完整回显为 1/5 次，`Hello\r\n`
为 0/5 次；字节间增加 1 ms 间隔后，`Hello` 为 5/5 次完整。115200 8N1 连续帧约每
86.8 微秒到达一字节。当前怀疑 Flash 取指延迟和可能的 TX 总线等待拖延了 RX 处理；
需要对 SRAM/LTO 修订进行实板前后对照，才能确认原因。

最近一次 Ubuntu 26.04 尝试中，`xmake flash` 对 476 字节的诊断 BIN 完成了两次复制
和文件系统同步；挂载文件与本地镜像哈希相同，`STATE.TXT` 仍保持默认提示。
先打开串口再按 RST 后，开发板仍输出不含 `[SRAM/LTO v2]` 的旧标语。这确认了板上
仍在运行旧固件，此前观察到的回显失败不能用于评价 SRAM/LTO 修订候选。

烧录失败的原因仍未确定。这些观察说明当前 Ubuntu/HFP-LINK 更新流程存在问题，
不能确定其具体原因，也不能泛化为 Ubuntu 均不兼容。主机文件哈希不属于独立 Flash
回读。后续使用 Windows 拖拽方式继续对照，先确认新启动标识，再运行逐字节和突发
测试。换主机重新构建前，应同时更新 xhive 和本工程；完整 Windows 构建及测试流程
尚未验证。

SYS_UART 没有公开的软件可读 TX 完成标志。寄存器写入成功，不表示最后一个停止位已经
在线路上发送完毕。本示例没有软件接收缓冲区或流控，不保证持续满速通信不丢数据。
逐字节测试通过不代表突发测试通过，主机测试脚本的检查也不代表固件通过验证。

## 参考资料

- [C2 Pi 官方板卡文档](https://embedded.openecos.com/zh-cn/latest/page/brd/starry-sky-c/v2.0_pi/)
- [官方 SDK 快速上手](https://embedded.openecos.com/zh-cn/latest/page/sdk/common/quickstart/)
- [ECOS SDK 烧录脚本](https://github.com/openecos-projects/embedded-sdk/blob/2.0/bin/ecos-flash)
- [早期星空板的 HFP-LINK 设计与操作说明](https://ysyx.oscc.cc/chip/board/official/boards/board-1/)
- [PicoRV32 源码与接口](https://github.com/YosysHQ/picorv32/blob/main/picorv32.v)

不同板卡版本及烧录器固件可能存在差异。早期星空板的操作说明或固件镜像，不能直接视为
适用于 C2 Pi。
