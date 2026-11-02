# AutoCAD 2020 ObjectARX 开发技能

供 Codex 开发、审查和维护 Windows x64 / AutoCAD 2020 原生 C++ 插件，也适用于 C# 数据库操作迁移、ARX/CRX 加载诊断及已有证据判读。业务命令、图层、比例、配筋数值和发布约定由使用项目定义。

## 安装

使用 [Skills CLI](https://github.com/vercel-labs/skills#install-a-skill)，其当前版本要求 Node.js 22.20 或更高版本。准备好 npm、Git；私有仓库还需已配置相应的 Git 访问权限。

从 GitHub 安装到当前用户的 Codex，全局可用：

```powershell
npx skills add xieyioo1991-web/autocad2020-objectarx --skill autocad2020-objectarx -a codex -g -y
```

已有本地源码时，在包含 `SKILL.md` 的仓库根目录运行：

```powershell
npx skills add . --skill autocad2020-objectarx -a codex -g -y
```

`-a codex` 指定 Codex，`-g` 指定用户级安装，`-y` 跳过安装交互。安装技能不会安装 AutoCAD、SDK 或编译器。参数说明见 [Skills CLI 安装文档](https://github.com/vercel-labs/skills/blob/main/docs/commands.md#skills-add)。

## 使用

示例请求：

```text
使用 $autocad2020-objectarx，修复当前命令在重复导入时的样式错误，
保持用户原有图形，沿用现有入口，完成相关验证后给出结果。
```

```text
使用 $autocad2020-objectarx，只读检查当前插件加载失败的原因，
区分实际模块、宿主依赖与旧日志，不附带构建或运行 CAD。
```

```text
使用 $autocad2020-objectarx，分析本次生成的重复读取和保存耗时，
按同条件结果优化，保持已确认图面和必要的数据保护。
```

任务已有输入、规则和授权时直接使用；只询问真正缺少且影响结果的信息。完整执行指导维护在 [SKILL.md](SKILL.md)，这里不重复列另一套流程。

## 资料导航

| 文件 | 何时读取 |
| --- | --- |
| [SKILL.md](SKILL.md) | 开发范围、完成边界、ObjectARX技术约束与参考路由 |
| [开发效率与复用实践](references/development-efficiency.md) | 过度验证、私设门槛、范围扩大、重复计算、协作及交付 |
| [验证与证据](references/verification.md) | 测试选择、结果复用、真实加载、日志误判及检查器参数 |
| [可复现工作流](references/reproducible-workflows.md) | 等价几何、生命周期、样式恢复、目标范围及证据维护案例 |
| [构建与加载](references/build-and-loading.md) | v141/x64、实际构建配置、依赖和宿主加载 |
| [数据库生命周期](references/database-lifecycle.md) | 对象所有权、事务、失败清理与C#迁移 |
| [原生出图模式](references/native-drawing-patterns.md) | JSON消费、跨库克隆、模板与Hatch关系 |
| [图纸兼容与交付一致性](references/drawing-compatibility.md) | 同名样式、坐标、可见性及资源配套 |
| [来源与SDK索引](references/sources.md) | ObjectARX 2020头文件、示例、CHM及官方入口 |
| [只读检查器](scripts/Inspect-Arx2020.ps1) | 需要读取显式工程路径、配置声明或已有JSON证据时使用 |

只读检查器示例：

```powershell
& .\scripts\Inspect-Arx2020.ps1 -Workspace 'C:\work\MyArxProject' -ProjectFile 'MyPlugin.vcxproj' -BinaryPath 'artifacts\Release\MyPlugin.arx'
```

仅 `Workspace` 必需，其余路径可省略；相对路径按该工作区解析。它输出JSON，不构建、不执行MSBuild导入、不启动CAD、不写文件。`Assessment=InspectionOnly`、退出码0或空`Findings`都不能证明插件运行通过；未知`Match`不是匹配。无需先运行检查器才能使用技能。

## 适用范围与验证说明

目标为 AutoCAD 2020 / R23.1 / Windows x64；工具链以 VS2017/v141 为基线，保留项目已确认配置。普通绘图、纯C#/.NET、Python COM及其他CAD版本不在默认范围。`cpp-coding-standards`、`cpp-testing`可按需配合，不是必需依赖。

技能结构与文档检查只验证技能本身；编译、CoreConsole业务宿主、生产GUI插件加载和实际交互各有范围。根据任务选择必要验证，不因列出这些层级就要求每次全部执行。源码、资源与已有结果的对应关系按实际依赖核对，历史通过不自动代表当前版本通过。

技能不分发业务工程、DWG、宿主、SDK或个人配置。参考中的案例路径与数字是历史依据，不是使用技能的前置条件、性能承诺或通用验收阈值。安装副本、项目副本和本仓库分别按用户指定范围维护。

## 更新

通过 Skills CLI 从 GitHub 安装后，只更新此全局技能：

```powershell
npx skills update autocad2020-objectarx -g
```

命令按技能名和全局范围更新，详见 [Skills CLI 更新文档](https://github.com/vercel-labs/skills/blob/main/docs/commands.md#skills-update)。若此前是手工复制安装、没有可用的更新来源，可重跑上面的 GitHub 安装命令。

从本地目录安装时，先更新本地源码，再重跑本地安装命令。仅修改或拉取 Git 仓库不会自动同步安装副本；安装副本中需要保留的定制应先合回源码。技能更新不自动改变业务项目和验收基线。

## 2026-11-02 更新

- 将近期复盘整理为[开发范围与完成边界](SKILL.md#开发范围与完成边界)，防止无依据的防御、确认门槛和范围扩大；分开产品运行、开发验证与正式发布。
- [验证规则](references/verification.md)按实际影响选择检查、复用有效结果，必要检查通过后交付；保留对象生命周期、事务、源图保护与真实执行证据等技术要求。
- [优化经验](references/development-efficiency.md)集中记录重复读取与计算复用、报告装配与编码、CAD 会话复用、轻量模板和同条件性能对照。
- 清理重复清单、过时项目阶段和无条件全量验证要求，历史案例只作追溯，不作为其他项目的准入条件。

仓库分发 `SKILL.md`、`references/`、`scripts/` 及 README、Git 忽略配置；不包含业务工程、运行日志、截图、DWG 或临时缓存。历史版本与变更可从 Git 查看。
