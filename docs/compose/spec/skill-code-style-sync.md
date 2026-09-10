---
feature: skill-code-style-sync
status: delivered
updated: 2026-02-14
branch: main
commits: 943d48290f422e4f3cb6ec4767c803e5707f2ed9..working-tree # 文档变更尚未提交；提交时请 path-limit 到 skills/ + README.md + docs/compose/spec/
---

# 同步 mcu-code-style 与 mcu-universal-driver 编码风格

## Report

**What was built** — 两个 skill 的编码风格条文对齐为同一套 S1–S5（措辞以 `mcu-universal-driver` 为裁决基准），但**各自自包含、正文互不引用**。`mcu-code-style` 将 R1–R5 改写为 S1–S5（含 S5 可读性、中文注释细则、驱动命名例外），并保留检查/修复流程、保护清单与 dry-run；通用架构/安全项直接写规则，不引用 C 编号或另一 skill。`mcu-universal-driver` 去掉跨 skill 关系声明，`references/code-style.md` 补齐 dry-run 与 S5 索引。assets 与 README 描述各自独立。

**Verification** — `grep`：skills 下无 R1–R5 权威编号、无跨 skill 引用（「见 mcu-universal-driver / mcu-code-style」）、无「唯一规范源」关系句；S1/S2/S4 表两边一致；`code-style.md` CRLF 已转 LF。独立 review：CRITICAL 0。无编译/测试（纯文档）。

**Journey log** — 条文以 universal-driver S1–S5 为裁决基准对齐，但 skill 正文必须自包含，禁止写「以另一 skill 为唯一规范源 / 见另一 skill」。原「R5 ↔ S4」并不等价（安全规则拆在 C5/C9）——两边各自写全规则，不靠交叉引用补齐。main 上有无关 staged 的 driver examples/tests 删除，提交必须 path-limit。

## [S1] Problem

`mcu-universal-driver` 正文宣称「S1–S4 与 `mcu-code-style` skill 的对应规则一致」，但两边条文已漂移：

| 维度 | mcu-code-style | mcu-universal-driver | 问题 |
|------|----------------|----------------------|------|
| 命名 | R1 基础表 | S1 基础表 + 驱动命名例外 + 驱动目录命名 | code-style 缺驱动例外 |
| 格式 | R2 | S2 | 基本一致，措辞略简 |
| 注释 | R3 摘要 | S3 中文优先策略 + 文件头模板 | code-style 缺细则 |
| 架构 | R4 分层/static | C1/C2/C7 | 拆分位置不同 |
| 嵌入式安全 | R5 完整清单 | S4（volatile/重命名）+ C5/C9 | 「R5↔S4」并不等价 |
| 可读性 | 无 | S5 | 仅 driver skill 有 |
| 资产 | clang-format/editorconfig | 同左 | 正文规则号注释不一致 |

用户裁决：条文措辞以 `mcu-universal-driver` 的 S1–S5 为裁决基准对齐；随后补充要求：**skill 正文不得引用另一 skill / 外部文件作为规范源**，两边各自写全、自包含。

## [S2] Design

### 角色划分

- **条文基准**：措辞以 `mcu-universal-driver` 的 S1–S5 为准做对齐，但两边 SKILL 正文均不写「唯一规范源 / 见另一 skill」。
- **mcu-code-style**：自包含 S1–S5 + 通用架构与安全检查项；保留范围解析、保护清单、检查+修复流程、配置生成与 dry-run。
- **mcu-universal-driver**：自包含 C1–C10 + S1–S5；不引用 mcu-code-style。
- **驱动命名例外**等条文在 code-style 中直接写入，不通过外部链接补齐。

### 两边关系表述（统一口径）

> 编码风格（S1–S5）以 `mcu-universal-driver` 为唯一规范源。`mcu-code-style` 提供非驱动 / 通用 MCU 场景的检查与自动修复流程，风格条文与规范源对齐；驱动 / 可移植驱动场景以 `mcu-universal-driver` 为准（一并覆盖架构约束 C1–C10）。

### 明确不改

- 不修改任何 `driver/**` 业务代码（index 上已有无关 staged 删除，不得纳入本次提交语义）。
- 不改变 S1–S5 的规则实质（命名例外、中文优先、可读性等保持 universal-driver 现文）。
- 不删除 code-style 的流程与保护清单。

## [S3] Out of Scope

- 重构通用驱动框架 C1–C10
- 修改仓库内各 `*_Driver` 实现
- 新增第三个 skill 或抽公共 shared markdown 包
- 处理 main 上已 staged 的 examples/tests 删除

## Tasks

- [x] T1: 更新 mcu-universal-driver 关系表述与 code-style.md dry-run — acceptance: S 章前声明「唯一规范源」，code-style.md 含 dry-run 验证要求 (covers: S2)
- [x] T2: 重写 mcu-code-style SKILL.md 风格章 — acceptance: 规则条文与 universal-driver S1–S5 同源，含 S5；description 与交叉引用正确 (covers: S2)
- [x] T3: 同步 rule-examples.md 与 assets 注释头 — acceptance: 示例规则号/标题与 SKILL 一致，无互相矛盾的 R/S 编号权威声明 (covers: S2; depends: T2)
- [x] T4: 一致性核验 — acceptance: 两边重叠条文无冲突；grep 无「S1–S4 与 mcu-code-style 一致」等过时表述 (covers: S1, S2)
- [x] T5: 评审与收尾 — acceptance: 独立 review 无 critical；feature 文档 status=delivered (covers: S2)
