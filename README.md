# StarrySky C2 示例

基于 xhive SDK 的 StarrySky C2 嵌入式示例与验证记录。示例按目录组织，
各自保存源码、构建配置、测试和使用文档。

## 示例目录

| 目录 | 内容 | 使用文档 |
| --- | --- | --- |
| [ex.hello/](ex.hello/) | 启动时输出 Hello World，随后通过 SYS_UART 逐字节回显 | [中文](ex.hello/README.zh_CN.md) · [English](ex.hello/README.md) |
| [ex.rtthread/](ex.rtthread/) | RT-Thread Nano、SYS_UART 控制台与 PicoRV32 抢占端口；参考 RTL 仿真通过，实板中断探测受阻 | [说明](ex.rtthread/README.md) |

原先位于仓库根目录的 Hello / Echo 工程已移到 `ex.hello/`。
构建、烧录和测试命令均在该示例目录中执行；示例文档中的“工程根目录”也指该目录。

## 构建 Hello / Echo

先按[环境要求](ex.hello/README.zh_CN.md#环境要求)准备 xhive SDK、RISC-V GCC、
Xmake 和 Python 配置依赖。将下方 SDK 路径替换为本机工作副本的绝对路径，
从本仓库根目录执行：

```sh
export XHIVE_SDK_PATH="/absolute/path/to/xhive"
cd ex.hello
xmake f -y
xmake build c2_hello
```

产物位于 `ex.hello/dist/c2_hello.elf` 和 `ex.hello/dist/c2_hello.bin`。
生成的 `build/`、`dist/`、`.xmake/` 和 `.vscode/` 目录不纳入版本控制。

安装串口测试依赖 `pyserial` 后，可在 `ex.hello/` 下运行无需开发板的主机测试：

```sh
python3 -m unittest discover -s tests -p test_echo_cli.py -v
```

## 烧录与验证状态

Hello / Echo 使用 SYS_UART，串口参数为 115200、8N1、无流控。
已验证 WSL 构建后由 Windows 复制 BIN 到 HFP-LINK 烧录盘，并通过复位后的
启动标识确认固件更新。具体模式切换、烧录和串口操作见[烧录与运行](ex.hello/README.zh_CN.md#烧录与运行)。

当前 SRAM / LTO 版本仍在逐字节和突发回显测试中出现丢字，尚未解决可靠传输问题。
已有实板结果及 WSL USB 转发的限制见[验证状态与限制](ex.hello/README.zh_CN.md#验证状态与限制)。
