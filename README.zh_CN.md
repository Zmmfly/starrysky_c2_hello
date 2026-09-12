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

启动输出：

```text
Hello World from xhive vendor: opencos / StarrySky C2!
```

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
| 串口监视与回显测试 | 针对当前 Linux 环境实现；需要串口设备访问权限，测试使用 POSIX 独占打开支持 |
| 其他环境 | 其他 Linux 发行版、Windows、macOS 和 WSL 尚未验证本工程的完整工作流程 |

Ubuntu 26.04 是当前验证基准，不代表最低版本要求。主机侧支持 Linux，不表示
HFP-LINK 烧录或实板回显已经通过，详见[验证状态与限制](#验证状态与限制)。
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

## 烧录与运行

C2 Pi 板卡通过物理模式开关选择 HFP-LINK 烧录模式或 UART 运行模式。
烧录器存储卷和 CP2102 串口接口不能同时使用。

> 在已测试的设备环境中，尚未验证出可靠的固件更新流程。复制完成或主机侧文件哈希
> 一致，不代表新固件已经写入 Flash，也不代表开发板正在运行新固件。

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

测试以独占方式打开串口，配置为 115200 8N1、关闭流控；读取初始输出后，发送文本、
CR/LF，以及从 `0x00` 到 `0xFF` 的全部字节值。每收到一个字节的正确回显后，才发送
下一个字节。出现不匹配、超时或尾随多余字节时，测试失败。

测试成功时的预期输出：

```text
PASS: 273 bytes echoed exactly at 115200 8N1
Coverage: text, CR/LF, and every byte from 0x00 through 0xff
```

这是基础双向通信测试，不是持续吞吐量或 FIFO 容量测试。启动问候语发送期间到达的
数据不在验证范围内。

## 验证状态与限制

| 检查项 | 状态 |
| --- | --- |
| 主机环境 | Linux / Ubuntu 26.04 |
| 固件构建 | 已使用 xPack RISC-V GCC 15.2.0 验证通过 |
| 主机回显测试脚本 | 伪终端检查通过：正确回显成功，错误字节、缺失字节和尾随多余字节被拒绝 |
| 早期仅 Hello 固件 | 已在 C2 实板观察到 SYS_UART 发送输出 |
| 回显固件实板运行 | 尚未验证通过；此前烧录尝试后仍运行旧版周期 Hello 固件 |
| 独立 Flash 回读 | 未执行 |
| OpenOCD/GDB 硬件调试 | 本工程尚无经过验证的流程 |

当前启动问候语修订版本已完成构建，尚未进行实板测试。此前尝试过单次复制、双次复制、
不同文件名，并在请求断电重上电后观察到了 USB 断开与重新连接；这些操作均未确认新固件
更新成功。`STATE.TXT` 保持默认提示不能作为成功依据，主机缓存也限制了文件读取结果
能够支持的结论。目前原因仍未确定。

SYS_UART 没有公开的软件可读 TX 完成标志。寄存器写入成功，不表示最后一个停止位已经
在线路上发送完毕。本示例没有软件接收缓冲区或流控，不保证持续满速通信不丢数据。
不能将模拟测试结果或早期仅 Hello 固件的结果当作回显功能的实板验证。

## 参考资料

- [C2 Pi 官方板卡文档](https://embedded.openecos.com/zh-cn/latest/page/brd/starry-sky-c/v2.0_pi/)
- [官方 SDK 快速上手](https://embedded.openecos.com/zh-cn/latest/page/sdk/common/quickstart/)
- [ECOS SDK 烧录脚本](https://github.com/openecos-projects/embedded-sdk/blob/2.0/bin/ecos-flash)
- [早期星空板的 HFP-LINK 设计与操作说明](https://ysyx.oscc.cc/chip/board/official/boards/board-1/)
- [PicoRV32 源码与接口](https://github.com/YosysHQ/picorv32/blob/main/picorv32.v)

不同板卡版本及烧录器固件可能存在差异。早期星空板的操作说明或固件镜像，不能直接视为
适用于 C2 Pi。
