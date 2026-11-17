[English](README.md) ｜ [简体中文](README.zh-CN.md) ｜ [日本語](README.ja.md)

<div align="center">

# ✉️ Becky-i18n

**Becky! Internet Mail 非侵入式多语言国际化（i18n）与本地化插件**

[![Release: Pre-release](https://img.shields.io/badge/Release-Pre--release-orange.svg)](https://github.com/robot-dot-win/Becky-i18n/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-806000.svg?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Win32%20(x86)-404040.svg?style=flat-square)](https://www.rimarts.jp/becky.htm)
[![C++20](https://img.shields.io/badge/std-C%2B%2B20-2b5b84.svg?style=flat-square)](https://en.cppreference.com/w/cpp/20)
[![Sponsor](https://img.shields.io/badge/Sponsor-555555?style=flat-square&logo=githubsponsors)](https://github.com/sponsors/robot-dot-win)
[![Patron](https://img.shields.io/badge/❤%EF%B8%8F-Patron-8b2635?style=flat-square)](patron/PATRON.zh-CN.md)

</div>

**Becky-i18n**（`locale.dll`）是一款专为 **Becky! Internet Mail** 打造的现代化、非侵入式、数据驱动的国际化（i18n）与本地化插件。本项目采用 C++20 编写，基于 Becky! 官方插件 SDK、Win32 窗口子类化（Subclassing）以及窄范围 API 钩子（Detour）技术，在运行期通过外部 UTF-8 JSON 语言包将 Becky! 的用户界面动态翻译为任意语言——**全程无需修改或硬改补丁 `B2.exe` 可执行文件**。

---

## 关于本项目

### 宿主软件：Becky! Internet Mail

由 **RimArts, Inc.** 开发的 [**Becky! Internet Mail**](https://www.rimarts.jp/becky.htm) 是 Windows 平台上一款极具传奇色彩的经典邮件客户端，历经二十余年持续演进，至今依然深受全球资深用户青睐：

* **历经时间检验，持续维护演进**：二十多年久经考验的可靠性，完美兼容最新版本的 Windows 系统。
* **纯正 Windows 原生应用**：基于原生 Win32/MFC 构建，拒绝臃肿的 Web 框架套壳。
* **资源占用极低，响应迅速**：毫秒级启动，内存占用仅为现代邮件客户端的零头，轻松管理数十万封海量邮件。
* **极强的稳定性**：几乎从不出现崩溃的情况。
* **界面经典、简洁、直观**：专注高效办公的无干扰设计与全键盘快捷操作体验。

### 为什么要做 Becky-i18n？

Becky! 内部采用 **ANSI MBCS 编码**，界面字符串全部硬编码在可执行文件中，且官方仅提供**英文版**与**日文版**。长期以来，网络上流传的中文版、法文版、德文版等第三方本地化版本，均通过直接二进制硬改 `B2.exe` 实现——这种方式不仅会破坏原版数字签名、难以跟进官方版本更新，还容易埋下稳定性隐患。

**Becky-i18n** 从根本上解决了这一痛点：

1. **零二进制修改**：严格遵循官方插件机制，以进程内插件（`locale.dll`）形式运行，让你始终使用原汁原味的官方原版 `B2.exe`。
2. **100% 数据驱动**：新增语种或润色译文只需编辑人类可读的 UTF-8 `.json` 语言文件，无需重新编译代码。
3. **深度界面覆盖**：不仅支持标准菜单与常规控件，还攻克了复杂的自绘菜单（`ETO_GLYPH_INDEX` 字形索引绘制）、双缓冲自绘 MIME 页签条（`AfxWnd`）、动态状态栏分格以及工具栏悬停提示（Tooltip）。

---

## 项目进度与路线图（`pre-release1`）

当前仓库处于 **`pre-release1`** 阶段。核心翻译引擎、内存安全防护、主窗口、菜单系统、界面语言选择对话框以及 14 个国际语种语言包已开发并测试完毕。

### `pre-release1` 已完成内容

* ✅ **主窗口（Main Window）**：
  * 系统文件夹目录树（保留未读邮件数 `(n)`）及动态根节点（如“查询结果”）。
  * 邮件列表视图表头列（ListView Column Headers）。
  * 邮件预览区信头标签与待办任务（Agent/Task）控制栏。
  * 邮件内容底部自绘 MIME 页签条。
  * 底部状态栏动态分格信息。
* ✅ **菜单系统与界面语言选择对话框**：
  * 主窗口、撰写窗口（Compose）与地址簿（Address Book）的顶层及多级弹出菜单树。
  * 全局命令池：覆盖固定命令、多变体命令、动态 ID 区间、工具栏悬停提示与状态栏帮助说明。
  * 邮件头字段自绘弹出菜单。
  * 模态**界面语言选择对话框（UI Language Picker Dialog）**。
* ✅ **14 种国际语言数据文件（`locale/*.json`）**：
  * `en_US.json` — 英语（English，基线模板）
  * `de_DE.json` — 德语（Deutsch）
  * `es_ES.json` — 西班牙语（Español）
  * `fr_FR.json` — 法语（Français）
  * `ja_JP.json` — 日语（日本語）
  * `ko_KR.json` — 韩语/朝鲜语（한국어）
  * `ms_MY.json` — 马来语（Bahasa Melayu）
  * `pt_PT.json` — 葡萄牙语（Português）
  * `ru_RU.json` — 俄语（Русский）
  * `th_TH.json` — 泰语（ไทย）
  * `uk_UA.json` — 乌克兰语（Українська）
  * `vi_VN.json` — 越南语（Tiếng Việt）
  * `zh_CN.json` — 简体中文
  * `zh_TW.json` — 繁体中文

### 后续计划（To-Do）

* [ ] **撰写邮件窗口（`Compose` Window）**：全面本地化窗口内部子控件与相关对话框。
* [ ] **地址簿窗口（`Address Book` Window）**：全面本地化内部子控件、属性页及 LDAP/CardDAV 视图。
* [ ] **常规设置对话框（`General Setup` Dialog）**：本地化所有全局首选项标签页与配置控件。
* [ ] **邮箱设置对话框（`Mailbox Setup` Dialog）**：本地化账户、POP3/IMAP4/SMTP 及配置方案对话框。
* [ ] **其余对话框与界面元素**：过滤规则管理器、邮件列表管理器、邮件查询对话框、模板编辑器及各类提示弹窗。
* [ ] **帮助文档（Help Documentation）**：本地化用户手册与上下文帮助文档。
* [ ] **更多国际语言数据文件**：持续扩充社区贡献的新语种并完善现有译文。

---

## 快速上手与安装部署（面向最终用户）

### 1. 前提条件：安装 `Becky! Internet Mail`

如果你尚未安装 Becky! Internet Mail，请先从 RimArts 官方网站下载并安装**英文原版（English edition）**：

* **官方网站下载页**：<https://www.rimarts.jp/becky.htm#download>

### 2. 部署 `Becky-i18n` 插件

1. 从本仓库的 [GitHub Releases](https://github.com/robot-dot-win/Becky-i18n/releases) 页面下载最新的 `Becky-i18n` 发布压缩包。
2. 找到 Becky! 的 **`PlugIns`** 插件目录。根据 Becky! 官方规范，该目录通常位于以下两处之一：
   * `[Becky! 程序安装目录]\PlugIns\`（例如 `C:\Program Files (x86)\RimArts\B2\PlugIns\`），**或**
   * `[Becky! 数据存储目录]\PlugIns\`
3. 将压缩包内的 `locale.dll` 以及包含 `.json` 语言文件的 `locale\` 子文件夹复制到 `PlugIns\` 目录下，最终目录结构如下：

```text
PlugIns\
├── locale.dll          # Becky-i18n 插件二进制文件（32 位 Win32，静态链接 CRT）
└── locale\             # 语言包目录
    ├── en_US.json      # 英语（默认 / 基线模板）
    ├── de_DE.json      # 德语 (Deutsch)
    ├── es_ES.json      # 西班牙语 (Español)
    ├── fr_FR.json      # 法语 (Français)
    ├── ja_JP.json      # 日语 (日本語)
    ├── ko_KR.json      # 韩语 (한국어)
    ├── ms_MY.json      # 马来语 (Bahasa Melayu)
    ├── pt_PT.json      # 葡萄牙语 (Português)
    ├── ru_RU.json      # 俄语 (Русский)
    ├── th_TH.json      # 泰语 (ไทย)
    ├── uk_UA.json      # 乌克兰语 (Українська)
    ├── vi_VN.json      # 越南语 (Tiếng Việt)
    ├── zh_CN.json      # 简体中文
    └── zh_TW.json      # 繁体中文
```

> **提示**：首次放入 `locale.dll` 并启动 Becky! 时，软件可能会弹窗询问是否加载新发现的插件 `locale.dll`，请点击 **Yes（是）**。

### 3. 切换界面语言

1. 点击顶部菜单栏的 **Tools → UI Language**（或 **工具 → 界面语言**），在弹出的对话框中选择你希望使用的语言。
2. **重启 Becky!** 使新语言生效。
   * *技术说明*：点击 **OK** 后，插件会将所选语言标识（如 `zh_CN`）保存在 Windows 注册表 `HKEY_CURRENT_USER\Software\RimArts\B2\Settings` 下的 `Locale` 键值中（`REG_SZ` 类型，默认值为 `en_US`）。为确保界面状态绝对干净稳定，`Becky-i18n` 采用启动时一次性初始化翻译的策略，不在运行中途强行热替换已创建的窗口句柄。

> **⚠️ 关于 Windows 非 Unicode 程序语言（系统区域设置）的重要提示（若菜单或目录树显示为 `"?"`）**：
> 由于 Becky!（`B2.exe`）内部是 ANSI/MBCS 应用程序，部分原生 Win32/MFC 控件依赖当前 Windows 的**非 Unicode 程序的语言**（即系统 ANSI 代码页）。切换语言后，如果你发现菜单或左侧文件夹树变成问号（`"?"`）：
> 1. 打开 Windows **控制面板 → 区域 → 管理** 选项卡。
> 2. 点击 **更改系统区域设置...**，选择与你所选界面语言相匹配的区域/语言。
> 3. 重启计算机即可正常显示。

---

## 语言包编写规范（面向语言翻译贡献者）

为 Becky! 添加一门新语言**不需要任何编程基础**。你只需复制 `PlugIns\locale\` 目录下的 `en_US.json`，将其重命名为目标语言代码（例如 `it_IT.json`、`pl_PL.json`），然后翻译其中的 `dst`、`replace` 和 `hint_dst` 字段即可。

### 1. 文件命名、编码与顶层结构

* **文件命名**：`<locale_id>.json`。其中 `<locale_id>` 必须符合安全白名单正则 `^[A-Za-z0-9_-]{1,32}$`（如 `en_US`、`zh_CN`、`fr_FR`）。
* **文件编码**：必须保存为 **无 BOM 的 UTF-8（UTF-8 without BOM）**。
* **顶层键结构**：每个语言 JSON 文件固定由三个顶层对象组成：
  1. `"meta"`：语言包元数据及快捷键助记符（`&`）排版规则。
  2. `"menus"`：顶层菜单定义，分为 `"popups"`（弹出子菜单树）和 `"commands"`（全局命令池）。
  3. `"windows"`：窗口与子控件树，以 Win32 窗口类名（如 `"Becky2MainFrame"`）作为键名。

```json
{
  "meta": { ... },
  "menus": {
    "popups": { ... },
    "commands": [ ... ]
  },
  "windows": {
    "Becky2MainFrame": { ... }
  }
}
```

### 2. 元数据（`meta`）与快捷键助记符（`&`）契约

```json
"meta": {
  "locale": "zh_CN",
  "name": "简体中文 (zh_CN)",
  "access_key": "$1(&2)$3",
  "version": "1.0.7",
  "author": "Martin Young"
}
```

| 字段名 | 类型 | 说明 |
| :--- | :--- | :--- |
| `locale` | `string` | 与文件名一致的唯一语言标识（如 `"zh_CN"`）。 |
| `name` | `string` | 在 **UI Language** 语言选择下拉框及插件 **About** 窗口中展示的名称（如 `"简体中文 (zh_CN)"`）。 |
| `access_key` | `string` | 快捷键助记符（`&`）处理模式：可设为 `"as_is"`，或使用包含 `$1`、`&2`、`$3` 的匹配模式模板。 |
| `version` | `string` | 语言文件版本号（如 `"1.0.7"`）。 |
| `author` | `string` | 语言包作者或维护者署名。 |

#### 原文 `src` 字符串的规范化

在语言文件中，所有菜单项及支持助记符控件的 `src` 原文均经过**规范化处理**：

1. 剥离了尾部的省略号 `...`（例如 `"&Import..."` 规范化存储为 `"Import"`）。
2. 剥离了单个助记符 `&` 以及尾部的括号助记符（如 `(&Z)`）。
3. 将 Win32 字符串中用于转义字面量 `&` 的双和号 `&&` 折叠还原为单个 `"&"`。

#### 运行期对译文 `dst` 的处理规则（`as_is` 模式 vs. 模板匹配模式）

`locale.dll` 根据 `meta.access_key` 的配置支持两种截然不同的译文处理模式：

##### 模式 A：`"as_is"` 原样输出模式（`en_US.json` 使用）

* **运行期行为**：插件对 `dst` **不做任何加工处理**，直接原样输出。
* **作者职责**：语言包作者对 `dst` 的最终呈现格式负全责，需自行在对应字母前添加助记符 `&`、将字面量 `&` 转义为 `&&`，并自行保留尾部三点 `...`（例如 `"src": "Delete Folder"`, `"dst": "De&lete Folder..."`）。

##### 模式 B：模板匹配模式（例如 `"$1(&2)$3"`）

* **运行期行为**：`dst` 统一填写**规范化纯文本**。语言作者**无需也不应在 `dst` 中手写 `&` 助记符或尾部 `...`**（例如在 `zh_CN.json` 中，只需写 `"src": "Delete Folder"`, `"dst": "删除文件夹"`），由插件在运行期自动完成“`&` 转义 + 助记符插入 + 尾部三点恢复”。
* **模板占位符定义**：
  * **`$1`**：代表主文本字符串（对应剥离了助记符和尾部三点后的 `src` / `dst` 正文）。
  * **`&2`**：代表从实时 Win32 `src` 原文中提取出的**快捷键助记符**（如 `&F`）。
  * **`$3`**：代表**尾部的三个点（`...`）**——仅当实时 Win32 `src` 原文带有尾部三点时取值为 `"..."`，否则为空 `""`。
* **常用模板格式举例**（以实时 `src` = `"Save &As..."`、`dst` = `"另存为"` 为例）：

| 模板格式（`meta.access_key`） | 助记符排版风格 | `"Save &As..."` 的最终渲染结果 |
| :--- | :--- | :--- |
| **`"$1(&2)$3"`** | 尾部圆括号助记符（中日韩标准风格） | `"另存为(&A)..."` |
| **`"[&2]$1$3"`** | 首部方括号助记符 | `"[&A]另存为..."` |
| **`"&2-$1$3"`** | 首部连字符助记符 | `"&A-另存为..."` |
| **`"&2. $1$3"`** | 首部序号点助记符 | `"&A. 另存为..."` |

### 3. 菜单系统（`menus`）

菜单定义与具体窗口解耦，统一置于顶层 `"menus"` 下，划分为 `"popups"` 和 `"commands"` 两部分。

#### 3.1 `menus.popups`（弹出子菜单树）

没有固定 `cmd_id` 的弹出式容器菜单按所属窗口分为三个桶：`"main_frame"`（主窗口）、`"compose"`（撰写窗口）和 `"address"`（地址簿）。每个节点包含 `translate`、`src`、`dst` 以及可选的嵌套 `children` 数组。

#### 3.2 `menus.commands`（全局命令池）

Becky! 中所有可执行菜单项按 `cmd_id` 升序存放在同一个扁平数组中。由于 Win32 命令 ID 具有全局语义，该命令池同时驱动**菜单项文本**、**工具栏按钮悬停提示（Tooltip）**以及**状态栏帮助提示（`hint_src` / `hint_dst`）**。

`menus.commands` 支持三种节点形态：

1. **固定单条命令（`cmd_id` + `src`/`dst`）**
2. **多变体命令（`cmd_id` + `rules`）**：用于同一个 `cmd_id` 在不同窗口下显示不同文本的场景（如 `57604` 或 `57636`）。
3. **动态命令 ID 区间（`cmd_id` + `cmd_id_end` + `rules`）**：用于 Becky! 在运行期动态分配 ID 的连续区间。凡是不在 `rules` 列表中的运行期文本（例如用户自建的拨号连接名称），插件均会安全放行、保持原文不变：
   * `1`–`100`（`"ownerdraw": true`）：邮件头字段自绘弹出菜单。
   * `10000`–`10099`：编辑器、解码、段落格式化、字符集与工具栏专属命令。
   * `20000`–`31999`：插件命令区间（如 PGP 插件、语音附件插件、Becky-i18n 插件等）。
   * `55001`–`55099`：查看 → 字符集语言（View → Language）动态菜单项。
   * `56001`–`56099`：拨号连接（Dialup）动态菜单项（`(no dialup)`）。
   * `56100`–`56199`：工具 → 插件设置（Tools → Plug-Ins Setup）动态菜单项。

### 4. 窗口与控件树（`windows`）

`"windows"` 下的每个键对应一个顶层窗口类名（如 `"Becky2MainFrame"`），并通过嵌套的 `"children"` 递归映射其 Win32 子控件树。控件通过 `class_name` 与 `ctrl_id` 精确定位。

| 控件类名（`class_name`） | `ctrl_id` | JSON 翻译配置说明 |
| :--- | :--- | :--- |
| **`SysTreeView32`** | `1000` | 系统文件夹树。通过 `"image_indices": [12..23]` 精准识别系统内置文件夹，通过 `"root_nodes"` 识别动态根节点；用户自建文件夹绝不会被误翻。 |
| **`SysHeader32`** | `0` | 邮件列表 `SysListView32` 内的表头列。 |
| **`AfxWnd`（页签条）** | `12562` | 邮件阅读区底部的自绘 MIME 页签条。 |
| **`Static`** | 多个 | 标准静态文本标签。 |
| **`Button`** | 多个 | 标准按钮控件。 |
| **`Edit`** | 多个 | 标准编辑框控件。 |
| **`ComboBox`** | `1532` | 下拉列表框，按从 0 开始的索引（`index`）与文本匹配。 |
| **`msctls_statusbar32`** | `59393` | 底部状态栏，按分格索引 `index` 划分为多个 `"panes"`。 |

### 5. 译文匹配规则

在菜单节点、窗口控件和状态栏分格中，翻译规则沿着两个正交维度组织：

1. **规则数量维度（单条匹配 vs. 多条匹配）**：
   * **单条匹配（Single-Rule）**：当一个节点只需匹配单一文本模式时，匹配字段直接作为该 JSON 节点的属性书写。
   * **多条匹配（Multi-Rule，`"rules"` 数组）**：当单个节点（如动态命令区间、多变体命令、自绘页签条或状态栏分格）需要匹配多个候选字符串或正则模式时，将多条规则统一放入 `"rules": [ ... ]` 数组中。
2. **匹配模式维度（精确映射 vs. 正则匹配）**：
   * **精确映射（`"src"` → `"dst"`）**：默认模式。执行高速字符串精确匹配（或规范化命令查找），将 `"src"` 替换为 `"dst"`。
   * **正则表达式模式（`"mode": "regex"` + `"pattern"` / `"replace"`）**：通过设置 `"mode": "regex"` 启用。使用 ECMAScript 正则表达式（`"pattern"`），并在 `"replace"` 中支持捕获组替换（`$1`、`$2` 等）。

| 数量维度 \ 模式维度 | 精确映射（`src` → `dst`） | 正则匹配（`mode: "regex"` + `pattern` / `replace`） |
| :--- | :--- | :--- |
| **单条匹配** *（节点内联属性）* | 在节点上直接定义 `"src"` 与 `"dst"`。 | 在节点上直接定义 `"mode": "regex"`、`"pattern"` 与 `"replace"`。 |
| **多条匹配** *（`"rules"` 数组）* | `"rules": [ { "src": "...", "dst": "..." }, ... ]` | `"mode": "regex"` 配合 `"rules": [ { "pattern": "...", "replace": "..." }, ... ]` |

#### 5.1 单条匹配 + 精确映射

适用于具有单一静态文本标签的普通控件或固定命令：

```json
{
  "class_name": "Button",
  "ctrl_id": 1068,
  "control_subtype": "PushButton",
  "translate": true,
  "src": "Edit Task",
  "dst": "编辑任务"
}
```

#### 5.2 多条匹配 + 精确映射

适用于同一个命令 ID、动态命令区间或自定义控件在运行期可能显示多种固定文本之一的场景：

```json
{
  "cmd_id": 57636,
  "translate": true,
  "rules": [
    { "src": "Find", "dst": "查找" },
    { "src": "Search Address", "dst": "查找地址" },
    { "src": "Find Address", "dst": "查找地址" }
  ],
  "hint_src": "Find the specified text",
  "hint_dst": "查找指定文本"
}
```

#### 5.3 单条匹配 + 正则匹配

适用于控件或状态栏分格显示带有运行期动态变量（如行列号或内联抄送前缀）的单一格式文本：

```json
{
  "index": 2,
  "translate": true,
  "mode": "regex",
  "pattern": "^R:(\\d+)(\\s+)C:(\\d+)(\\s+)Lines:(\\d+)(\\s*)$",
  "replace": "行:$1$2列:$3$4总行:$5$6"
}
```

#### 5.4 多条匹配 + 正则匹配

适用于同一个控件或状态栏分格可能显示多种不同格式的动态文本、且每种都需要正则捕获组替换的场景：

```json
{
  "index": 0,
  "translate": true,
  "mode": "regex",
  "rules": [
    {
      "pattern": "^Switch language to (.*)$",
      "replace": "切换界面语言为 $1"
    },
    {
      "pattern": "^Receiving message \\((\\d+)/(\\d+)\\)\\.\\.\\.$",
      "replace": "正在接收邮件 ($1/$2)..."
    }
  ]
}
```

### 6. 运行期安全限额

为保障宿主进程（`B2.exe`）的绝对稳定，`locale.dll` 在解析任何 `.json` 语言包时均会强制执行严格的安全限额校验（定义于 `src/limits.hpp`）。一旦触发任一限额，插件将拒绝该异常语言包（或单条规则）并安全回退至原生 `en_US` 界面：

| 限额常量 | 上限值 | 含义说明 |
| :--- | :--- | :--- |
| `kMaxJsonFileBytes` | `2 MB` | 单个语言 `.json` 文件的最大字节数（按 UTF-8 计）。 |
| `kMaxNestingDepth` | `20` | JSON 对象/数组的最大嵌套深度。 |
| `kMaxStringBytes` | `4096` | 单条 JSON 字符串值的最大 UTF-8 字节长度。 |
| `kMaxRegexPatternBytes` | `256` | 正则表达式 `pattern` 的最大 UTF-8 字节长度。 |
| `kMaxRegexInputChars` | `1024` | 运行期参与正则匹配的字符串最大 UTF-16 字符数。 |
| `kMaxArrayElements` | `256` | 单个 JSON 数组（`commands`、`rules`、`children` 等）允许的最大元素个数。 |
| `kMaxLocaleFiles` | `32` | `locale\` 目录下最多扫描的 `.json` 语言包文件数量。 |
| `kMaxLocaleIdLength` | `32` | 语言标识符的最大字符长度（`^[A-Za-z0-9_-]{1,32}$`）。 |
| `kMaxLogFileBytes` | `1 MB` | 诊断日志文件的大小上限（超出后自动截断重建）。 |

---

## 技术架构与源码编译（面向开发者）

### 1. 核心翻译引擎

由于 Becky! 将界面字符串硬编码在二进制中，且混合使用了标准 Win32 控件、MFC 视图以及自定义 GDI 自绘表面，`locale.dll` 采用了双通道互补拦截架构：

1. **SDK 回调 + 窗口子类化（`UiTranslator`）**：
   * 挂载 Becky! 导出的 `BKC_On*` 生命周期回调。
   * 递归遍历窗口 HWND 树并对目标窗口过程进行子类化（Subclassing），实时拦截并翻译各类窗口与控件通知消息。
2. **窄范围 Win32/GDI API 钩子（基于 MinHook 的 `ApiHooks`）**：
   * 自绘邮件头弹出菜单拦截（`ExtTextOutW` + 增强图元文件 `EMF` 离屏预重放提取明文）。
   * 自绘 MIME 页签条拦截（子类化作用域 + 双缓冲内存 DC `DrawTextA` 拦截）。
   * 未命中路径零开销直通与内存安全防护（`win32_guard.hpp`）：所有钩子与回调中解引用的指针均经过用户态地址范围校验与 SEH 内存可读性探针保护，确保任何异常绝不逃逸至宿主进程 `B2.exe`。

### 2. 开发环境与第三方依赖

#### 编译器与平台要求

* **操作系统**：Windows 10（或更高版本）。
* **编译器**：**Visual Studio 2022**（或更高版本），需安装 **使用 C++ 的桌面开发** 工作负载（MSVC 工具链）。
* **C++ 标准**：**C++20**（`CMAKE_CXX_STANDARD 20`、`CMAKE_CXX_EXTENSIONS OFF`、`/utf-8 /W3 /permissive- /EHsc`）。
* **目标架构**：**仅限 32 位 x86（`Win32`）**。由于 Becky!（`B2.exe`）是 32 位程序，CMake 会强制检查 `CMAKE_SIZEOF_VOID_P`，若以 64 位配置会直接报错中止。
* **CRT 链接方式**：**静态链接 CRT**（Release 下为 `/MT`，Debug 下为 `/MTd`），确保编译出的 `locale.dll` 在任何 Windows 机器上均可开箱即用，无需额外安装 VC++ 运行库。
* **构建工具**：**CMake ≥ 3.15**

#### 官方 SDK 与第三方库（纯本地离线构建）

`CMakeLists.txt` 构建过程**不会**从网络拉取代码，需提前在本地准备好 **Becky! 官方插件 SDK** 及两个开源库：

1. **Becky! 官方插件 SDK（`Becky-SDK`）**
   * **下载地址**：<https://www.rimarts.jp/becky.htm#download>
   * **所需文件**：`BeckyAPI.cpp` 与 `BeckyApi.h`
2. **nlohmann/json**（现代 C++ 单头文件 JSON 库）
   * **仓库地址**：<https://github.com/nlohmann/json>
3. **MinHook**（轻量级 Windows x86/x64 API 钩子库）
   * **仓库地址**：<https://github.com/TsudaKageyu/minhook>

#### 推荐目录布局

建议将 `Becky-SDK`、`nlohmann/json` 和 `minhook` 放置在与本工程 `Becky-i18n` **同级**的父目录下：

```text
<workspace>/
├── Becky-i18n/          # 本仓库目录
│   ├── CMakeLists.txt   # 顶层 CMake 构建脚本
│   ├── locale.def       # 模块导出符号定义
│   └── src/             # C++ 源代码目录
├── Becky-SDK/           # Becky! 官方插件 SDK（含 BeckyAPI.cpp 和 BeckyApi.h）
├── nlohmann/
│   └── json/            # git clone https://github.com/nlohmann/json.git
└── minhook/             # git clone https://github.com/TsudaKageyu/minhook.git
```

### 3. 编译 `locale.dll`

#### 方式 A：使用默认的同级目录布局

打开 PowerShell 或命令提示符执行：

```powershell
# 1. 将依赖库克隆到同级目录
git clone https://github.com/nlohmann/json.git nlohmann/json
git clone https://github.com/TsudaKageyu/minhook.git minhook
# （请确保包含 BeckyAPI.cpp 与 BeckyApi.h 的 Becky-SDK/ 也放在同一父目录下）

# 2. 配置并编译 Becky-i18n（必须指定 -A Win32）
cd Becky-i18n
cmake -S . -B build -A Win32
cmake --build build --config Release
```

#### 方式 B：显式指定自定义依赖路径

如果你的 SDK 或第三方库存放在其他位置，可通过 `-D` 参数传入绝对路径：

```powershell
cd Becky-i18n
cmake -S . -B build -A Win32 `
  -DBECKY_SDK_DIR="D:/sdk/Becky-SDK" `
  -DNLOHMANN_JSON_DIR="D:/third_party/json" `
  -DMINHOOK_DIR="D:/third_party/minhook"

cmake --build build --config Release
```

编译完成后，生成的插件文件位于 `build/Release/locale.dll`。

---

## 知识产权、致谢与第三方声明

本项目严格尊重所有原作者及第三方开源项目的知识产权：

1. **Becky! Internet Mail & Becky! Plug-In SDK**
   * **Copyright © RimArts, Inc. All rights reserved** (<https://www.rimarts.jp/>)
   * *Becky! Internet Mail* 是 RimArts, Inc. 的注册商标与专有软件，由乘松知博（Tomohiro Norimatsu）先生开发。
   * `Becky-i18n` 是由社区独立开发的开源插件，与 RimArts, Inc. 不存在任何隶属、赞助或官方背书关系。
2. **JSON for Modern C++ (`nlohmann/json`)**
   * **Copyright © 2013–2026 Niels Lohmann** (<https://github.com/nlohmann/json>)
   * 基于 **MIT License** 授权发布。
3. **MinHook — The Minimalistic x86/x64 API Hooking Library for Windows**
   * **Copyright © 2009–2017 Tsuda Kageyu. All rights reserved** (<https://github.com/TsudaKageyu/minhook>)
   * 基于 **BSD 2-Clause License** 授权发布。
4. **Becky-i18n (`locale.dll` & Language Packs)**
   * **Copyright © 2026 Martin Young & Becky-i18n Contributors**
   * 基于 **MIT License** 授权发布。

---

## 赞赏与支持（Patronage & Sponsorship）

🙏你的赞赏与支持将直接化作推进本项目的动力：[![Patron](https://img.shields.io/badge/❤%EF%B8%8F-Patron-8b2635?style=flat-square)](patron/PATRON.zh-CN.md)

---

## 开源协议（License）

本项目采用 [MIT License](LICENSE) 开源协议授权。