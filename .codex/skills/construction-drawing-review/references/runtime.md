# 本地工具与可选审核引擎

默认由 Codex 直接查看图像、关联证据并判断，提取、换算和排版使用本机已验证工具。没有额外模型 API 不影响这种工作方式；没有视觉能力或图面不可读时，仅完成可核实部分，其余保留未完成。不要把只建索引或成功返回 JSON 称为技术审核完成。

## 已安装引擎的使用

先找到实际 Python 环境，核实命令；不要为读取图纸自动覆盖已有引擎安装。

```powershell
# $taskPython 是本机已核实的 Python 解释器路径。
& $taskPython -X utf8 -B -m drawing_review.cli --help
& $taskPython -X utf8 -B -m drawing_review.cli checks
& $taskPython -X utf8 -B -m drawing_review.cli extract --help
& $taskPython -X utf8 -B -m drawing_review.cli review --help
& $taskPython -X utf8 -B -m drawing_review.cli report --help
```

有同环境命令入口时 `drawing-review` 与 `python -m drawing_review.cli` 等价。扫描图 OCR 可使用 Tesseract 及 chi_sim/eng，缺少 OCR 时仍可视觉读取可辨图像；未识别部分必须保留，不以文字为空判缺项。

已验证的本地提取与报告命令：

```powershell
# extract 的输入是单个 PDF；多文件逐个提取到不同目录。
& $taskPython -X utf8 -B -m drawing_review.cli extract $inputPdf --output $extractOutput
# report 的输入须符合当前引擎结果结构；输出分别为简版和详版。
& $taskPython -X utf8 -B -m drawing_review.cli report $engineResultJson
```

`extract` 可加 `--no-ocr`，但要另行读取未提取到的图面。`report` 不会自动实现本 Skill 的“仅确认问题”筛选或简详合并；按 [交付约定](delivery.md) 生成派生报告并验收，保留未筛选原始结果。若人工底稿不符合当前引擎结构，使用现有本地排版工具，不伪造字段来强行通过。

## 范围与引擎检查项不等价

本技能的三种模式没有新增 CLI 参数。引擎 `review` 在省略 `--checks` 时可能选择全部项目，因此只有已选范围与实际检查项匹配时才调用。

当前可见的图纸检查项包括 `architecture.identity`、`architecture.geometry`、`architecture.stair`、`coordination.levels` 和 `coordination.openings`；它们只能辅助部分协调审核，不能代替 C01—C09 全部清单，也不能保证仅检查建结或仅检查图面一致性。

“仅错漏碰缺”默认采用 `extract`＋Codex 按协调清单审核。不要为了核对图上两处荷载或配筋标注而启用 `structure.loads`、`structure.beams` 等专项；若旧引擎的提示词或检查项无法表达用户范围，就由 Codex 完成该部分，不宣称 `--checks` 已解决范围约束。

结构计算或综合模式也要显式传入本次选择且 `checks` 命令确实列出的 ID；不能将本技能 C01 等台账编号传给 CLI。图页阅读、引擎候选及运行状态均须回到证据核实，再写入正式台账。

## 安装与版本兼容

技能包保留 `scripts/install.py` 及 `wheels/villa_drawing_review-*.whl`，只在需要且决定安装引擎时使用；从技能目录执行 `python scripts/install.py`。先检查现有版本，随包 wheel 不保证比本机开发环境新，不能直接覆盖较新功能。开发环境可在真实引擎仓库执行 `python -m pip install -e review-engine`。

不同安装版本的预算、缓存、软件识别等命令能力以 `--help` 和实际实现为准；旧 wheel 不提供新参数时不要照抄。CAD/PDF 转换是单独的本机能力，见 [CAD/PDF 准备](cad-pdf.md)。

## 仅在选择外部模型运行时

沿用用户本次指定的模型与费用范围；没有该需求时不配置或调用付费 API。配置将密钥留在环境变量，项目资料、Skill 和报告中不写密钥。下列是结构示例，端点、模型及项目条件必须替换并核实：

```json
{
  "project": {"name": "项目名称", "location": "实际所在地", "usage": "实际用途", "structure": "结构体系", "designDate": "实际日期", "applicabilityConfirmed": false},
  "model": {"baseUrl": "https://api.example.invalid/v1", "model": "实际模型名", "apiKeyEnv": "DRAWING_REVIEW_API_KEY", "vision": true, "jsonMode": false, "maxOutputTokens": 3000},
  "norms": []
}
```

端点需满足实际客户端的 `chat/completions` 接口。代表性中文与钢筋局部图验证通过后再承担对应任务，不根据名称推定支持视觉或严格 JSON。示例中的 `vision: true` 是配置声明，不是能力证明。

```powershell
# $selectedCheckIds 是当前检查目录中、符合用户范围的非空字符串数组。
& $taskPython -X utf8 -B -m drawing_review.cli review $pdfFolder --output $workOutput --checks $selectedCheckIds --config $configJson
```

输出目录不得覆盖原件。已支持的版本可用 `--budget-tokens`、`--budget-cost`、`--cache-dir`，金额预算需核实实际价格。记录输入、输出、缓存及失败/在途请求用量；无返回或计费未知时保留记录并暂停新增收费请求，不盲目重复提交。不要固化历史费率、排名或保证固定节省比例。

中止后按当前版本规则从 checkpoint 恢复；支持 `CANCEL` 标记的版本先检查是否仍处于取消状态。恢复前核对资料、选项和预算，只有用户要求恢复时才移除取消标记。新版本资料另建结果批次；缓存以文件内容、提取器、规则、项目条件、模型及证据为边界，变更后复核关联影响。

已有网站 Worker 配置可继续使用 `DRAWING_REVIEW_BASE_URL`、`DRAWING_REVIEW_WORKER_TOKEN`、`DRAWING_REVIEW_PROFILES`、`DRAWING_REVIEW_DATA`；它们不是本地直接校审的前置条件。本次技能范围选择和交付规则不会自动改造网站或旧引擎行为。
