# StarrySky C2 / RT-Thread Nano

使用 xhive SDK 自带的 RT-Thread Nano 4.1.1，控制台直接连接 SYS_UART，
115200、8N1、无流控。构建方式与 `ex.hello` 相同，工程命令在本目录执行。

**当前状态：交叉编译及参考 RTL 仿真通过；实板指令探测在 `maskirq` 处未继续，
NOP 对照完整输出，当前抢占端口不能直接用于这块板子。**
表现与 `ENABLE_IRQ=0` 一致，但尚无实片 trap 状态或流片参数确认。Nano BIN 尚未实板运行；
参考模型通过不能作为 C2 支持这套中断机制的证据。

## 构建

准备与 [ex.hello](../ex.hello/README.zh_CN.md#环境要求) 相同的 xhive、Xmake、
RISC-V GCC 和 Python Kconfig 环境，在仓库根目录执行：

```sh
export XHIVE_SDK_PATH="/absolute/path/to/xhive"
cd ex.rtthread
xmake f -y
xmake build c2_rtthread
```

产物：`dist/c2_rtthread.elf`、`dist/c2_rtthread.bin`；链接映射：`build/c2_rtthread.map`。
需要包含 `RTTNANO_RISCV_PORT_CUSTOM` 选项的 xhive SDK（从 `f2886db` 引入）。
本次使用该版本与 xPack RISC-V GCC 15.2.0；BIN 17,764 字节，RAM 占用
22,336 字节（含 16 KiB 静态堆与 4 KiB 启动栈）。

在 `xmake menuconfig` 的 `Third-party configurations` 中选择 `RT-Thread Nano`，
再在 `RT-Thread Nano configurations → RISC-V port implementation` 中选择
`Application-provided RISC-V port`。本例 `.config` 已启用
`CONFIG_RTTNANO_RISCV_PORT_CUSTOM=y`。
SDK 负责 Nano 内核、MSH 和 `rtconfig.h` 生成，跳过内置 `libcpu` 和 `port` 源码；
工程通过普通的 `add_files("src/*.c", "src/*.S")` 编译本地 CPU/板级端口，
不再覆盖 SDK target。标准 RISC-V 端口仍为 SDK 默认选项。

## 运行行为

启动后输出 Nano 版本，创建 MSH 控制台，并自动执行两项检查：

- `preempt: PASS`：两个同优先级线程不调用 yield/delay，持续检查寄存器值并计数；
  高优先级线程延时 100 tick 后抢占它们，两者均推进且寄存器校验成功才通过。
- `irq-mask: PASS`：嵌套屏蔽中断期间 tick 不推进，恢复后补齐经过的 tick。

检查结束后测试线程退出，可在 `msh >` 输入 `help`、`uptime`、`list_thread`、`free`。
`uptime` 输出由中断推进的 tick，频率为 1000 Hz。
SYS_UART 使用 xhive 的 `c2_sys_uart_*` 驱动，分频值为 625；输入无数据时 shell
睡眠 1 tick，输出将 LF 转为 CRLF。打印时屏蔽中断，避免不同上下文的输出交错；
恢复后按 `rdcycle` 补偿 tick。请逐字输入，长输出期间不要粘贴命令或发送突发流，
本例没有解决此前实板 SYS_UART 丢字的问题。

实板烧录可参考 [Windows / HFP-LINK 流程](../ex.hello/README.zh_CN.md#烧录与运行)，
应复制本例的 `dist/c2_rtthread.bin`，不要误用 Hello BIN。
烧录状态成功不代表本例的中断参数已获得实板验证。
切到 UART 模式后，可用 pyserial 打开控制台（按实际设备替换端口；Windows 使用 COM6）：

```sh
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200 --raw
```


## 中断端口及依据

最新的 [C2 RTL 来源核验](docs/c2-rtl-provenance.md) 已定位到 2025-03-19 的
`c96eb4c`：同一次提交切换到 C2 地址布局，并将 `ENABLE_IRQ` 从 1 改为 0。
该候选保留 16 路 GPIO，但仍缺少与 C2 实片对应的最终流片清单。

[retroSoC 早期参考集成](https://github.com/retroSoC/retroSoC/blob/4b2a1faea8918f1a5a8da32eb62cd35375cbe83a/rtl/mini/retrosoc.v)
与 [PicoRV32 指令说明](https://github.com/YosysHQ/picorv32/tree/ef203c2b0a3fb793280f5114941416c425c5b461)
是端口依据。参数要求为：复位/IRQ 入口均为 `0x0`、`ENABLE_IRQ=1`、
`ENABLE_IRQ_QREGS=0`、`ENABLE_IRQ_TIMER=1`，IRQ 0 未被永久屏蔽，支持 `rdcycle`。
Flash 位于 `0x0`，SRAM 为 `0x30000000` 起的 128 KiB，CPU 时钟 72 MHz。

- `src/port.S`：复位及 IRQ 共用入口，通过旧 IRQ mask 的 bit 0 区分；
  实现 `maskirq`、`timer`、`retirq`，保存/恢复 128 字节上下文。
- IRQ 硬件占用 `gp/tp`，因此所有 C 代码均使用 `-ffixed-x3 -ffixed-x4`，
  禁止小数据和链接器 gp relaxation；本例不支持 TLS、外设 IRQ 或中断嵌套。
- `src/port.c`：通过内部 timer IRQ 驱动 tick、抢占和主动切换。
  主动切换也触发 IRQ，使所有现场恢复都在硬件 IRQ 上下文完成，避免在普通上下文
  恢复 gp 后、执行 `retirq` 前再次被中断而丢失返回地址。
- `link.ld`：固定 C2 内存布局，保留 MSH 命令表；`src/main.c` 初始化内核、堆和控制台。
  IRQ 使用线程栈，每个线程的栈必须为保存现场和内核中断调用预留空间。
- `src/probe.S`：抢占验证用的寄存器校验循环。

已按 `/home/zmmfly/repos/embedded-sdk` 的 `StarrySkyC2` 板型核对 `main`（`eb84d50`）、
`2.1-dev`、`3.0-dev`、`starryskypi`：C2 的 `start.S` 仅初始化 C 环境并进入 main，
timer 驱动轮询 MMIO，没有提供 IRQ 入口、q 寄存器或内部 timer 的集成参数。
`3.0-dev` 的中断驱动属于 `StartySkyT1Pico / CL1-2512`，使用标准 CSR/PLIC，
不能用来证明 C2 支持同一套中断接口。参考 retroSoC 快照也不能替代 C2 流片版本确认。
进一步检查公共 HAL 和其他分支后，`hal_intr_*` 的实现仍只归属于 T1/T1-Pico；
`starryskypi` 中的 `mycpu` 异常代码使用 RV64、`sd/ld` 和标准 CSR，且 `kcontext()`
未实现，不能作为 C2 的上下文端口。C2 的 `coroutine_test` 使用 LightCoroutine，
`TASK_YIELD` 保存恢复点并返回，由 `task_scheduler` 轮询任务，是协作调度。
这些 SDK 实现尚未提供 C2 抢占方案；缺少 C2 中断驱动不等于证明硬件没有任何中断机制。

SDK 中可参考的完整示例为 `2.1-dev` 的 `templates/timer_interrupt/t1/main.c`
和 `3.0-dev` 的 `templates/timer_interrupt/t1-pico/main.c`。后者执行
`hal_intr_init()` → `hal_timer_init()` → `hal_timer_register_callback()` →
设置 PLIC 阈值 → `hal_intr_global_enable()` → `hal_timer_start()`。
Timer0 使用 PLIC source 3；分发时先读取定时器 EOI 清除中断，再调用计数回调，
最后写 PLIC complete。`trap.S` 保存通用寄存器、`mepc/mstatus`，通过 `mret` 返回。
示例每观察到回调计数变化就打印一行；本工程只检查了源码，未在 T1/T1-Pico 上运行。

这套示例不是只需替换 Timer 地址：它使用 `0x10080000` 定时器、CONTROL bit 2
中断屏蔽和 EOI 寄存器，与 C2 的 CONFIG/VALUE/DATA 布局不同；初始化首先操作
`mstatus`，也没有绕过本次实板受阻的 CSR 路径。SDK 的模板支持表仅为 T1/T1-Pico
勾选 `timer_interrupt`、`gpio_interrupt`、`plic`、`clint`，C2 未勾选。
另外，`clint` 示例仅测试 mtime/mtimecmp 读写，`plic` 示例仅测试分配/启停/释放 API，
两者均不能当作已收到周期中断的示例。xhive 的 C2 `c2_timer_systick_*` 同样通过
读取 DATA 获取时间，驱动明确不安装 CPU 中断处理函数。


版本差异不能忽略：`4b2a1fa` 已加入新的 native wrapper，但顶层尚未接入它，
不能据此认定顶层使用 C2 的 UART 地址布局；该版本 CPU 设置 `ENABLE_IRQ=1`。
后续 [`b196dea` 的 core_wrapper](https://github.com/retroSoC/retroSoC/blob/b196dea41fd85efed755fc86b571b00429f9bf03/rtl/mini/top/core_wrapper.sv)
却设置 `ENABLE_IRQ=0`。本例采用前者的 IRQ 模式。**若实片关闭了 IRQ，软件不能重新启用，
本抢占端口不能在该芯片上运行**；届时只能另行选择协作调度方案或具备中断的硬件。

### 可选实板指令探测

`tests/irq_probe.S` 是独立的 216 字节诊断程序，不运行 Nano。编译器和 objcopy 在 PATH 时：

```sh
riscv-none-elf-gcc -march=rv32imc -mabi=ilp32 -nostdlib \
  -Wl,-Ttext=0,-e,_start,--no-relax tests/irq_probe.S -o dist/c2_irq_probe.elf
riscv-none-elf-objcopy -O binary dist/c2_irq_probe.elf dist/c2_irq_probe.bin
```

它先输出 `C2 IRQ probe: before maskirq`，再依次尝试 `maskirq` 和 `timer`，始终屏蔽 IRQ。
两条指令均能执行时，后续输出包含 `maskirq supported` 和 `timer supported`。
若只出现首行且固件更新已经确认，说明执行在探测阶段停止，仍需结合后续诊断判断原因；
不能仅凭没有串口输出认定芯片不支持中断。
指令成功也不证明 IRQ 路由、入口地址或 q 寄存器模式匹配。

2026-09-13 实板对照：两份 BIN 均由 Windows 复制到 HFP-LINK，文件哈希一致，
`STATE.TXT` 均返回 `write successful !!!`；切回 UART 后先打开 COM6（115200、8N1），
再按 RST 捕获输出。两次开头均有一个 NUL 字节。

| 固件 | 实际输出 | 结论 |
| --- | --- | --- |
| 216 字节 IRQ 探测 | 仅 `C2 IRQ probe: before maskirq`，共 31 字节 | 未观察到执行越过 `maskirq`，也尚未验证 `timer` |
| 220 字节 NOP 对照 | `control: before nop`、`control: after nop`、`control: end (no IRQ instructions)` 三行完整，共 120 字节 | 同一串口能继续输出，基本排除普通串口输出故障 |

NOP 对照将三条 `.insn r` 各替换为一个 32 位 `0x00000013` NOP，并修改三条文本标识。
IRQ 探测 SHA256：`f1296b770f8653020070790c49e91e3ac7cd973b736a2d61c28bff6f3c1e05b4`；
NOP 对照 SHA256：`73963e8b7161e64c1f8b65b60ced08fd2a51c1134a0303573817f823124fd459`。
该结果定位到自定义指令执行受阻，与关闭 IRQ 的模型一致，但不等于已读出实片配置或 trap 原因。
如果实片确实裁掉 IRQ 支持，软件无法重新启用；此时本端口不能实现抢占调度。
该轮结束时板上保留 NOP 对照固件，随后已更换为下述外设定时器探测固件。

### 外设定时器中断探测

`embedded-sdk` 的 C2 寄存器定义包含 Timer0（`0x10002000`）和 Timer1
（`0x10003000`），每组依次为 CONFIG、VALUE、DATA，偏移为 0、4、8。
xhive 的公共 timer 驱动将 `0x0101` 注释为关闭 IRQ 的连续倒计数模式。
[参考 counter_timer RTL](https://github.com/retroSoC/retroSoC/blob/b196dea41fd85efed755fc86b571b00429f9bf03/rtl/ip/native/counter_timer.sv)
中 CONFIG bit 3 为 IRQ 使能，VALUE 为重装值，DATA 为当前计数；因此 `0x109`
可开启连续倒计数和外设 IRQ。
[已接入 native wrapper 的参考顶层](https://github.com/retroSoC/retroSoC/blob/036ac3c540865ff27b5c02dfd301ce4a05ff3750/rtl/mini/retrosoc.v)
将 Timer0/1 接至 CPU IRQ 7/8。这些是探测依据，尚未确认与实片一致。

`tests/timer_irq/` 是独立诊断程序，不使用 PicoRV32 自定义中断指令，也不启动 Nano。
在本目录构建：

```sh
riscv-none-elf-gcc -march=rv32imc_zicsr -mabi=ilp32 \
  -ffreestanding -fno-builtin -msmall-data-limit=0 -Os -g \
  -Wall -Wextra -Werror -nostdlib -Wl,--no-relax,-T,link.ld \
  tests/timer_irq/start.S tests/timer_irq/main.c -o dist/c2_timer_irq_probe.elf
riscv-none-elf-objcopy -O binary dist/c2_timer_irq_probe.elf dist/c2_timer_irq_probe.bin
```

探测先打印两个定时器的配置、重装值、倒计数前后值和 IRQ 使能位读回值。
随后尝试标准 `mstatus`、`mie`、`mtvec`；只有向量地址读回匹配才打开 CPU 中断。
若标准 CSR 可用，则逐个开启定时器 IRQ，用另一个定时器测量三次中断的间隔。
连续两次间隔均在设定周期的 0.5～2 倍以内，才打印 `PERIODIC IRQ OBSERVED`。
未探测未知的 PLIC 地址，未观察到中断也可能是路由或控制器配置不同。

本次 BIN 为 1,892 字节，SHA256：
`533f2d5d5683fc644eb3cfb39fca197877857dfe5d4f1c43c3b8c76ea45fac49`。
交叉编译通过。在真实参考 counter_timer RTL 与关闭 IRQ 的 PicoRV32 组合模型中，
两个定时器均读回 `reload=000afc7f`，计数从 `000afc7a` 降至 `000a0a32`，
IRQ 配置读回 `00000008`；之后在标准 `mstatus` 读取处产生预期 trap。
这仅验证探测程序的 MMIO 阶段，未验证标准 CSR 中断处理路径。
该 BIN 已通过 Windows 复制到 HFP-LINK，源文件和目标文件 SHA256 一致，
`STATE.TXT` 返回 `write successful !!!`。

2026-09-13 切回 UART 后，先打开 Windows COM6（115200、8N1、无流控），
再按 RST，共捕获 371 字节（含开头一个 NUL）。两个定时器均得到以下结果：

```text
stopped cfg=00000000
reload=000afc7f
loaded count=000afc7f
count before=000afbe6
count after=0004fe3f
IRQ bit readback=00000008
```

最后一行为 `Before standard mstatus read`，未出现 `mstatus=` 或 IRQ 测试结果。
反汇编显示标记输出后先执行 `0x470: csrr a1,mstatus`，再执行
`0x474: csrci mstatus,8`，两条之后才打印 `mstatus=`，因此实板结果只能定位到
这段 CSR 操作，不能区分读取与清除 MIE 哪一步受阻；未获得实片 trap 状态。
实板已确认计数器工作、CONFIG bit 3 可读写，但尚未确认 IRQ 输出或 CPU 中断入口。
串口采集已停止，板上保留此探测固件。本次未修改 Nano 调度端口。
定时器能产生 IRQ 还需要 CPU 支持中断入口、现场保存和返回，才能驱动 Nano 抢占调度。


## QEMU 尝试

本机 QEMU 8.2.2 的 `-machine help` 无 StarrySky C2，`-cpu help` 无 PicoRV32。
实际用 `virt` 的 loader 加载本例 ELF 并单步，得到 `epc=0x0, desc=fault_fetch`。
为单独核对指令支持，将 BIN 开头两条指令装入 `virt` 的 DRAM `0x80000000` 后单步，
第二条 `maskirq` 得到 `epc=0x80000004, tval=0x0602020b, desc=illegal_instruction`。

因此这版 QEMU 不能直接运行本例。更换链接地址还不够；还需要实现 PicoRV32 自定义
中断指令及 C2 外设模型。[QEMU virt](https://www.qemu.org/docs/master/system/riscv/virt.html)
使用另一套板级外设，不能作为 SYS_UART 验证结果。

## 可复现的 RTL 仿真

仿真运行的是同一份 BIN，CPU 为上游 PicoRV32 RTL；内存和 UART 是简化的 C++ 模型。
它验证软件中断/调度端口和控制台接口，**不模拟 C2 UART RTL、Flash XIP 时序或真实丢字行为**。

安装 Verilator、C++ 编译器和 make，然后在本目录执行：

```sh
mkdir -p build/sim-source
curl -fL https://raw.githubusercontent.com/YosysHQ/picorv32/ef203c2b0a3fb793280f5114941416c425c5b461/picorv32.v \
  -o build/sim-source/picorv32.v
python3 tests/run_rtl.py --rtl build/sim-source/picorv32.v
```

脚本检查 RTL SHA256，构建 CPU 模型，分别加入 0 和 8 个额外总线等待周期，
每组运行 7200 万个 CPU 周期，检查抢占、寄存器校验、嵌套中断屏蔽及 MSH 命令响应。
日志位于 `build/rtl/build.log`、`build/rtl/run-wait0.log`、`build/rtl/run-wait8.log`。
本次 Verilator 5.020 两组均通过，输出包含：

```text
preempt: PASS tick=100
irq-mask: PASS catchup=5
tick=401 rate=1000Hz
RTL smoke test PASS
```

计数器数值及 shell 执行时的 tick 随总线等待改变。所有生成产物均在 Git 忽略目录内。

可用同一脚本验证探测程序：

```sh
python3 tests/run_rtl.py --rtl build/sim-source/picorv32.v --probe dist/c2_irq_probe.bin
```

它还会分别构建 IRQ 开启/关闭的 CPU 模型：前者须输出两条指令成功标识，
后者须仅输出探测首行、随后产生预期 CPU trap。
