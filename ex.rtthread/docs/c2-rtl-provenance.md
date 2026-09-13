# C2 RTL 来源与中断配置核验

核验日期：2026-09-13。

已确认 C2 官方指定的上游为 `retroSoC/retroSoC`，并定位到同时具有 C2 地址布局、
16 路 GPIO 和关闭 CPU IRQ 配置的历史 RTL。**尚未找到把这块 C2 实片绑定到某个
commit 的流片清单，因此以下版本是可核查的候选，不能称为已确认的实片源码。**

## 官方来源

[官方 C2 Pi 文档](https://github.com/openecos-projects/embedded-doc/blob/416ce31b3d0281290fd181a7ea2ba92a5494113d/doc/src/zh/page/brd/starry-sky-c/v2.0_pi.md)
直接链接 [retroSoC](https://github.com/retroSoC/retroSoC)，说明使用 PicoRV32、
72 MHz、128 KiB SRAM 和 16 路 GPIO，但未注明 RTL commit 或流片文件清单。
当前 retroSoC 主线已采用不同的管理核与扩展架构，不能直接作为 C2 源码使用。

## 同时改变地址布局与关闭 IRQ 的提交

[`c96eb4c2f28f1f15a2694c7b65445b75e187ad9a`](https://github.com/retroSoC/retroSoC/commit/c96eb4c2f28f1f15a2694c7b65445b75e187ad9a)
提交于 2025-03-19，标题为 `feat: use define macro to replace the hardcode`。
实际 diff 不仅替换宏，还将 PicoRV32 的 `ENABLE_IRQ` 从 1 改为 0，并删除
`ENABLE_IRQ_QREGS(0)` 显式配置。同一提交更新总线地址映射。

| 核验项目 | 此提交的 RTL | C2 SDK / 已有实板信息 |
| --- | --- | --- |
| CPU | PicoRV32，M/C 开启，`ENABLE_IRQ=0` | 官方说明为 PicoRV32；中断指令探测受阻 |
| Flash 启动地址 | `0x00000000` | 一致 |
| SRAM 基地址 | `0x30000000`，RAM 地址使用 `[16:2]` | 128 KiB SRAM 基地址一致 |
| 外部 PSRAM 基地址 | `0x40000000` | 一致 |
| SYS_UART | `0x10001000/0x10001004` | 一致 |
| Timer0/1 | `0x10002000/0x10003000`，偏移 0、4、8 | 一致，实板计数与配置读回已验证 |
| GPIO | 16 位输入、输出与方向 | 官方规格一致 |

地址和端口数量匹配只能建立候选关系，不能证明时钟、工艺宏、复位、总线时序及
后续流片修改均一致。该提交也未标为 C2/ICS55 的最终流片版本。

## 可直接阅读的 RTL

- [SoC 顶层 `rtl/mini/top/retrosoc.v`](https://github.com/retroSoC/retroSoC/blob/c96eb4c2f28f1f15a2694c7b65445b75e187ad9a/rtl/mini/top/retrosoc.v)：
  第 118 行将 native IRQ 接到 CPU 的 `[8:6]`；第 143 行为 `.ENABLE_IRQ(0)`。
- [native 外设封装 `rtl/mini/ip/natv_ip_wrapper.v`](https://github.com/retroSoC/retroSoC/blob/c96eb4c2f28f1f15a2694c7b65445b75e187ad9a/rtl/mini/ip/natv_ip_wrapper.v)：
  第 70～77 行译码串口和定时器；Timer0/1 分别输出至 `irq_o[1]`、`irq_o[2]`。
- [定时器 `rtl/mini/ip/counter_timer.v`](https://github.com/retroSoC/retroSoC/blob/c96eb4c2f28f1f15a2694c7b65445b75e187ad9a/rtl/mini/ip/counter_timer.v)：
  CONFIG bit 3 为 `irq_ena`，计数边界可产生 `irq_out`。
- [CPU `rtl/mini/core/picorv32.v`](https://github.com/retroSoC/retroSoC/blob/c96eb4c2f28f1f15a2694c7b65445b75e187ad9a/rtl/mini/core/picorv32.v)：
  第 1092 行的 `maskirq` 译码依赖 `ENABLE_IRQ`，第 1538 行的中断进入条件也依赖它。
- [地址宏 `rtl/mini/ip/mmap_define.v`](https://github.com/retroSoC/retroSoC/blob/c96eb4c2f28f1f15a2694c7b65445b75e187ad9a/rtl/mini/ip/mmap_define.v)。

这套 RTL 的连接为 Timer0/1 → native IRQ 1/2 → CPU IRQ 7/8，但 CPU IRQ 功能在
综合参数中关闭。因此**对这套候选 RTL 而言**，开启外设 IRQ 位不能启用 CPU 中断。
它与实板现象相符，是“CPU IRQ 被裁掉”假设的进一步证据，仍不是实片版本确认。

## 后续版本及排查范围

- `b820040cb1ab1fd517cea5eb8c12aae2b75477ab`（2025-04-15）抽出 `core_wrapper.sv`，
  保留 `.ENABLE_IRQ(0)`。
- `7efd5baa19cd199cdbe40ef3e5b4619fe48cccaa`（2025-08-15）将 GPIO 从 16 路减为 8 路；
  其后的旧候选 `b196dea` 不能据 GPIO 规格直接认作 C2。
- 本轮检查上游 `main/dev/feat/apu` 的公开历史、官方板卡资料和组织仓库列表，
  并查看 `tapeout`、`mini-ver-mpw`、`document`、`artifact`、`ecc-ci-designs`。
  没有取得 C2 的最终流片 commit/filelist。`document` 的 gen1 文件为空；
  `tapeout` 是外设集合，不能仅凭仓库名当作 C2 流片工程。

本地克隆位于仓库根目录的 `build/verification/rtthread/rtl-source/retroSoC`。
`candidate-c96eb4c/` 保存同一提交的 10 个原始源码/文件清单文件及 SHA256 manifest；
`irq-config-history.patch` 保存关闭 IRQ 的历史 diff。这些路径均为忽略的调查产物。
本轮仅进行了源码及文档核验，没有运行新的 RTL 仿真、修改固件或烧录。

要最终确认实片，需要 C2 流片工程的 commit、实际综合文件清单和宏配置，或维护者
对等效源码版本的明确确认。当前公开资料尚不能补齐这一环。
