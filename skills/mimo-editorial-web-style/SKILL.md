---
name: mimo-editorial-web-style
description: "Create share-ready single-file HTML pages for news flashes, announcements, market or industry updates, research snippets, and structured summaries, using a Xiaomi MiMo website-inspired warm-neutral editorial design with a responsive long-form layout. Use only when users ask to turn concrete source content into 网页、分享页、信息页、快讯页、摘要页 or an HTML share page. Do not use for marketing landing pages, product websites, web apps, admin dashboards, decorative posters, or pages without substantive information."
---

# MiMo Editorial Web Pages

## 目标

把一段有明确事实内容的快讯、公告、新闻摘要或研究结论，整理成一个可直接在浏览器打开、便于微信、社群和社交平台转发的单文件 HTML 长页面。页面采用自上而下的编辑排版，视觉统一使用 MiMo 式暖白、黑灰、克制橙色；不复制财联社或 MiMo 的标志与版式。

## 工作流

1. 完整读取 [references/style-system.md](references/style-system.md)。
2. 从用户材料中提取类别、标题、正文或要点、关键数字、来源、时间和可选尾注。
3. 只使用可追溯内容；不得补造数字、引语、来源、日期或结论。未知字段直接省略。
4. 按内容长度选择快讯页、要点页、数据页或长文页；默认优先单一长页面。
5. 压缩文字但保持事实含义。标题不超过 30 个中文字符；每节正文控制在 90–300 字；要点保持 3–6 条。
6. 生成单文件 HTML：内联 CSS、`<meta charset="UTF-8">` 与 viewport、系统字体栈、语义化标签（header/main/section/footer）、响应式布局；不引用任何外部资源，不使用 JavaScript。
7. 默认交付一个 `.html` 文件；内容过长时拆成编号分节（01/02/03）自上而下排列，不缩小字号。用户额外要求图片时，用浏览器打开页面并整页截图导出 PNG。
8. 按交付检查核对事实、文字、层级和视觉后再交付。

## 允许的页面类型

- **快讯页**：一个标题、一段 90–180 字正文、来源与时间。
- **要点页**：一个标题、3–6 条要点、可选结论或风险提示。
- **数据页**：一个关键数字或指标作为主视觉，辅以短解释和来源。
- **长文页**：将较长公告、文章或研究拆成编号分节，自上而下滚动阅读。

不要生成营销落地页、产品官网、后台管理页、带交互的 Web 应用、装饰海报，或只有口号没有信息正文的页面。

## 内容规则

- 保留专有名词、数值、单位、正负号、时间范围和限定条件。
- 把冗长背景压缩为读者理解结论所需的最少上下文。
- 不把推测改写为事实；保留“预计、可能、据称、拟”等语气。
- 不擅自添加投资建议、因果判断或情绪化标题。
- 来源或时间由用户提供时照录；未提供时不要伪造。
- 仅在用户提供真实链接或二维码资产时放入二维码；不得生成看似可扫描的假二维码。

## 版式规则

- 页头放小型类别标签、时间和大标题；正文区居中、最大宽度 640–720 px，自上而下分节；页脚放来源、尾注或真实链接信息。
- 手机优先的响应式布局，桌面端自动放大留白，任何屏宽都无需横向滚动。
- 只设置一个橙色焦点，可用于类别点、关键数字、关键词下划线或短分隔线。
- 允许页脚使用纯黑信息带，但整页必须以暖白为主。
- 不使用导航栏、按钮、表单、交互控件、复杂图表或无关插画。

## 交付检查

- 标题、正文、数字、单位、来源和时间是否与输入一致。
- 断网双击打开是否正常显示：无外部依赖、无加载失败、UTF-8 无乱码。
- 手机和桌面宽度下是否都无需放大即可阅读，页头、正文、页脚层级一眼可辨。
- 暖白、黑、暖灰是否占绝大多数，橙色是否低于约 5%。
- 是否无明显阴影、蓝紫霓虹、玻璃拟态和装饰性科技元素。
- 是否没有财联社、Xiaomi、MiMo 或其他未经提供的品牌标志。
- 内容过长时是否已经分节，而不是压缩字号或塞满一屏。

## 失败修正

- 文字太密：删去重复背景，改成 3–6 条要点或拆分节，仍过长则精简素材。
- 像后台或营销落地页：去掉导航、按钮和表单，改为单一自上而下阅读页。
- 像产品官网：缩小装饰主体，恢复标题、正文、来源三层信息结构。
- 中文乱码：确认 `<meta charset="UTF-8">` 与文件实际编码一致后重新保存。
- 缺少 MiMo 气质：改用 `#FCFAF8` 暖白底、`#F0EBE5` 模块、纯黑层级和一个小面积橙色焦点。
