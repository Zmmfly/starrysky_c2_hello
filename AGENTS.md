# Git 提交规范

本规范适用于 `c2_hello` 工程。提交信息必须准确描述本次变更，便于独立阅读、审查和追溯。

## 1. 语言与基本结构

- **首行 summary 必须使用英文**，不使用中文，也不添加 Markdown 标题标记。
- **commit 详情必须分为两个独立部分：先完整英文，后完整中文**。不得逐句或逐条交替翻译。代码符号、文件路径、命令及日志原文保持原样。
- 正文使用 Markdown：以 `## English` 和 `## 中文` 划分语言，以三级标题 `###` 划分小节，以 `-` 组织条目，以反引号标记路径、命令和代码符号。
- 首行与正文之间空一行；小节标题与条目之间、小节之间均空一行。
- 说明“为什么修改、修改了什么、如何验证”，避免 `update code`、`fix issues` 等无法辨识具体内容的描述。

## 2. 首行 Summary

采用 Conventional Commits 格式：

```text
<type>(<scope>): <imperative summary>
```

- `type` 使用小写，例如 `feat`、`fix`、`docs`、`refactor`、`test`、`build`、`ci`、`chore`、`revert`。
- `scope` 指明主要影响范围，例如 `uart`、`startup`、`flash`、`build`、`agent`；没有合适范围时可以省略。
- summary 使用英文祈使句，通常以 `add`、`fix`、`remove`、`document` 等动词开头。
- **整个首行不超过 72 个字符**，结尾不加句号。
- 标题描述实际结果，不使用含糊的修改数量或主观评价。

示例：

```text
feat(uart): add periodic Hello World output
build(project): generate compile commands in .vscode
docs(agent): define bilingual commit conventions
```

## 3. 双语分段格式

- `## English` 下完成所有英文小节和条目，再开始 `## 中文`；每个语言部分均可独立阅读。
- 英文部分使用英文小节标题和说明，中文部分使用中文小节标题和说明，不使用混合语言标题或逐条语言标签。
- 两个部分的小节顺序、条目顺序和事实内容必须对应，不能只在其中一种语言中补充结论或遗漏重要事项。
- 一条只说明一件事；多项变更拆成多个条目，避免把整段 diff 塞进一个长句。
- 中英文中的参数、数值、命令和验证结论必须一致。技术术语可以保留英文名称。
- 不只翻译小节标题而省略正文翻译。完整排版参见第 6 节示例。

## 4. 正文小节

两个语言部分分别使用以下小节。前三节必须保留，最后一节按需添加；若添加，两个语言部分都应包含。

### 概述小节

- 英文标题为 `### Summary`，中文标题为 `### 概述`。
- 说明本次提交的目的、必要背景和最终行为。
- 保持简短，不重复列出所有实现细节。

### 变更小节

- 英文标题为 `### Changes`，中文标题为 `### 变更`。
- 按逻辑列出主要代码、配置或文档变化。
- 必要时说明选择该实现的原因，以及涉及的路径或符号。
- 每次提交只围绕一个完整主题，无关变更应拆分。

### 验证小节

- 英文标题为 `### Validation`，中文标题为 `### 验证`。
- 列出实际执行的命令、检查方式及其结果；必要时注明工具链、串口参数或硬件条件。
- 未执行的检查必须明确标注，并说明原因；失败或尚未完成的检查不能写成通过。
- 区分交叉编译、软件模拟、BIN 复制与同步、Flash 回读和实板运行，不得互相替代。
- 串口实板验证应说明实际观察到的输出；仅生成 BIN 或复制到 HFP-LINK 不等于程序已运行成功。
- 文档变更可说明未运行构建和运行测试，但仍应检查 Markdown 结构和 diff 格式。

### 说明小节（可选）

- 英文标题为 `### Notes`，中文标题为 `### 说明`。
- 记录兼容性影响、迁移步骤、已知限制或尚待验证的事项。
- 没有相关内容时省略本节，不添加空标题或占位条目。
- 不兼容变更在标题中使用 `!`，并在两个语言部分分别提供迁移说明。若额外使用 `BREAKING CHANGE:` footer，保留标准英文标记及英文摘要，对应中文内容写入中文说明小节。

## 5. 提交前检查

- 仅在用户明确要求提交时创建 commit；不自动推送，不擅自改写已有提交历史。
- 检查 `git status`、工作区 diff 和暂存区 diff，确认本次提交的范围。
- 按明确路径暂存文件，避免混入用户已有的无关修改。
- 不提交 `build/`、`.xmake/`、`.vscode/compile_commands.json` 等生成产物。
- 提交前执行 `git diff --cached --check`，并按变更风险执行必要验证。
- 对照暂存区 diff 核对提交信息：不能漏掉主要行为变化，也不能描述本次提交未包含的工作。
- 提交完成后报告 commit 哈希、英文首行，以及仍留在工作区的未提交修改。

## 6. 完整示例

以下为格式示例。实际提交时必须按真实 diff 和已经执行的检查填写，不能照抄未经验证的结果。

```markdown
build(project): generate compile commands in .vscode

## English

### Summary

- Keep the editor compilation database synchronized with the build configuration.

### Changes

- Add `plugin.compile_commands.autoupdate` to `xmake.lua` with `.vscode` as the output directory.

### Validation

- Run `xmake build c2_hello`: passed; `.vscode/compile_commands.json` was generated.
- Confirm that the generated database contains the compilation command for `src/main.c`.
- Run `git diff --cached --check`: passed.

### Notes

- Hardware runtime tests were not run because this change only affects editor metadata generation.

## 中文

### 概述

- 使编辑器使用的编译数据库与构建配置保持同步。

### 变更

- 在 `xmake.lua` 中添加 `plugin.compile_commands.autoupdate`，并将输出目录设为 `.vscode`。

### 验证

- 执行 `xmake build c2_hello`：通过，已生成 `.vscode/compile_commands.json`。
- 确认生成的数据库包含 `src/main.c` 的编译命令。
- 执行 `git diff --cached --check`：通过。

### 说明

- 未执行实板运行测试，因为本次变更仅影响编辑器元数据的生成。
```
