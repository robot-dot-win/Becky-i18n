[English](README.md) ｜ [简体中文](README.zh-CN.md) ｜ [日本語](README.ja.md)

<div align="center">

# ✉️ Becky-i18n

**Non-Intrusive Internationalization & Localization Plug-In for Becky! Internet Mail**

[![Release: Pre-release](https://img.shields.io/badge/Release-Pre--release-orange.svg)](https://github.com/robot-dot-win/Becky-i18n/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-806000.svg?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Win32%20(x86)-404040.svg?style=flat-square)](https://www.rimarts.jp/becky.htm)
[![C++20](https://img.shields.io/badge/std-C%2B%2B20-2b5b84.svg?style=flat-square)](https://en.cppreference.com/w/cpp/20)
[![Sponsor](https://img.shields.io/badge/Sponsor-555555?style=flat-square&logo=githubsponsors)](https://github.com/sponsors/robot-dot-win)
[![Patron](https://img.shields.io/badge/❤%EF%B8%8F-Patron-8b2635?style=flat-square)](patron/PATRON.md)

</div>

**Becky-i18n** (`locale.dll`) is a modern, non-intrusive, data-driven internationalization (i18n) and localization plug-in for **Becky! Internet Mail**. Built with C++20 on top of the official Becky! Plug-In SDK, Win32 window subclassing, and narrow-scope API detours, it translates Becky!'s user interface into any language at runtime using external UTF-8 JSON language packs—**without modifying or binary-patching `B2.exe`**.

---

## About

### The Host Application: Becky! Internet Mail

[**Becky! Internet Mail**](https://www.rimarts.jp/becky.htm) (developed by **RimArts, Inc.**) is a legendary Windows email client with over 30 years of continuous evolution. Written natively in Visual C++ for Win32, it remains a favorite among power users worldwide thanks to its defining characteristics:

* **Time-Tested & Actively Maintained**: Over three decades of proven reliability, running seamlessly on the latest Windows releases.
* **Pure Native Windows Application**: Built directly on Win32/MFC without bloated web frameworks.
* **Minimal Resource Footprint & Blazing Speed**: Starts instantaneously, consumes a fraction of the memory of modern clients, and handles hundreds of thousands of emails effortlessly.
* **Rock-Solid Stability**: crash-free in nearly all cases.
* **Classic, Clean, and Intuitive UI**: A distraction-free, keyboard-friendly interface designed for maximum productivity.

### Why Becky-i18n?

Internally, Becky! uses **ANSI MBCS encoding** with UI strings hardcoded inside the executable, and RimArts officially provides only **English** and **Japanese** editions. Historically, localized editions (such as Chinese, French, or German) circulated on the internet were created by directly binary-patching `B2.exe`—an approach that breaks digital signatures, lags behind official updates, and risks stability issues.

**Becky-i18n** solves this permanently:

1. **Zero Binary Modification**: Works strictly as an in-process plug-in (`locale.dll`) loaded via Becky!'s official Plug-In SDK. You always run the untouched, official `B2.exe`.
2. **100% Data-Driven**: Adding a new language or refining translations requires only editing a human-readable UTF-8 `.json` file—no recompilation needed.
3. **Deep UI Coverage**: Translates not only standard menus and controls, but also complex owner-draw menus (`ETO_GLYPH_INDEX`), custom double-buffered MIME tab strips (`AfxWnd`), dynamic status bar panes, and toolbar tooltips; it also covers owner-drawn GDI text with no dedicated control handle (such as Task pane status lines and mail-list task labels) via data-driven `gdi_patterns` rules.

---

## Project Status & Roadmap

This repository is currently at the pre-release stage **`v1.0.0-rc.4`**. The core translation engine, memory-safety guards, Menu System, Main Window, Compose Window (mail and templates), UI Language Picker Dialog, and 16 international language packs are complete and tested.

### Completed

* ✅ **Main Window**:
  * System Folder TreeView (preserving unread message counts `(n)`) and dynamic root nodes such as "Query Result".
  * Mail ListView Column Headers.
  * Message View Header Labels and Agent/Task controls.
  * Custom Owner-Draw Message View Tab Strip for MIME parts.
  * Status Bar dynamic panes.
  * **Tasks View**-related UI elements.
* ✅ **Menu System & UI Language Picker Dialog**:
  * Top-level and nested popup menus across Main Frame, Compose Window, and Address Book.
  * Global command pool covering fixed commands, multi-variant commands, dynamic ID ranges, toolbar tooltips, and status bar help hints.
  * Custom Owner-Draw header fields popup menu.
  * Modal **UI Language Picker Dialog**.
* ✅ **Compose Window**:
  * Standard control and button labels, along with their tooltips, across the six Compose windows (new message, reply, forward, and the three template editor windows).
  * Data-driven regular-expression translation of the window title.
* ✅ **16 International Language Data Files (`locale/*.json`)**:
  * `en_US.json` — English (Baseline template)
  * `de_DE.json` — German (Deutsch)
  * `es_ES.json` — Spanish (Español)
  * `fr_FR.json` — French (Français)
  * `it_IT.json` — Italian (Italiano)
  * `ja_JP.json` — Japanese (日本語)
  * `ko_KR.json` — Korean (한국어)
  * `ms_MY.json` — Malay (Bahasa Melayu)
  * `pl_PL.json` — Polish (Polski)
  * `pt_PT.json` — Portuguese (Português)
  * `ru_RU.json` — Russian (Русский)
  * `th_TH.json` — Thai (ไทย)
  * `uk_UA.json` — Ukrainian (Українська)
  * `vi_VN.json` — Vietnamese (Tiếng Việt)
  * `zh_CN.json` — Simplified Chinese (简体中文)
  * `zh_TW.json` — Traditional Chinese (繁體中文)

### To-Do (Upcoming Releases)

* [ ] **`Address Book` Window**: Full localization of its child controls, property sheets, and LDAP/CardDAV views.
* [ ] **`General Setup` Dialog**: Localization of all global preference tabs and controls.
* [ ] **`Mailbox Setup` Dialog**: Localization of account, POP3/IMAP4/SMTP, and profile configuration dialogs.
* [ ] **Remaining Dialogs & UI Elements**: Filtering Manager, Mailing Lists Manager, Mail Query dialog, Template editor, and message boxes.
* [ ] **Help Documentation**: Localized user manuals and context help documentation.
* [ ] **More International Language Data Files**: Expand community-contributed locales and regional refinements.

---

## Quick Start & Deployment (For End Users)

### 1. Prerequisite: Install `Becky! Internet Mail`

If you do not already have Becky! Internet Mail installed, download and install the official **English edition** from RimArts:

* **Official Website Download Page**: <https://www.rimarts.jp/becky.htm#download>

### 2. Deploy `Becky-i18n`

1. Download the latest `Becky-i18n` release archive from the [GitHub Releases](https://github.com/robot-dot-win/Becky-i18n/releases) page.
2. Locate your Becky! **`PlugIns`** directory. According to the Becky! specification, this can be either:
   * `[Becky! Program Directory]\PlugIns\` (e.g., `C:\Program Files (x86)\RimArts\B2\PlugIns\`), **or**
   * `[Becky! Data Directory]\PlugIns\`
3. Copy `locale.dll` and the `locale\` subfolder (containing the `.json` language files) into the `PlugIns\` directory so that the layout looks like this:

```text
PlugIns\
├── locale.dll          # Becky-i18n plug-in binary (32-bit Win32, static CRT)
└── locale\             # Language pack directory
    ├── en_US.json      # English (Default / Baseline)
    ├── de_DE.json      # German (Deutsch)
    ├── es_ES.json      # Spanish (Español)
    ├── fr_FR.json      # French (Français)
    ├── it_IT.json      # Italian (Italiano)
    ├── ja_JP.json      # Japanese (日本語)
    ├── ko_KR.json      # Korean (한국어)
    ├── ms_MY.json      # Malay (Bahasa Melayu)
    ├── pl_PL.json      # Polish (Polski)
    ├── pt_PT.json      # Portuguese (Português)
    ├── ru_RU.json      # Russian (Русский)
    ├── th_TH.json      # Thai (ไทย)
    ├── uk_UA.json      # Ukrainian (Українська)
    ├── vi_VN.json      # Vietnamese (Tiếng Việt)
    ├── zh_CN.json      # Simplified Chinese (简体中文)
    └── zh_TW.json      # Traditional Chinese (繁體中文)
```

> **Note:** When you launch Becky! for the first time after copying `locale.dll`, Becky! may display a prompt asking whether to load the new plug-in `locale.dll`. Click **Yes**.

### 3. Switch UI Language

1. Click the menu **Tools → UI Language** to select your desired language.
2. **Restart Becky!** for the selected language to take effect.
   * *Technical note:* Clicking **OK** saves the selected locale identifier (e.g., `zh_CN`) to the Windows Registry under `HKEY_CURRENT_USER\Software\RimArts\B2\Settings` (value name: `Locale`, type: `REG_SZ`, default: `en_US`). By design, `Becky-i18n` performs a clean translation pass on startup rather than hot-swapping live window handles mid-session.

> **⚠️ Important Note on Windows System Locale (If Menus or Folder Tree Appear as `"?"`):**
> Because Becky! (`B2.exe`) is internally an ANSI/MBCS application, certain Win32/MFC paths rely on your Windows **Language for non-Unicode programs** (system ANSI code page). If you notice that menus or the folder tree display as question marks (`"?"`) after switching languages:
> 1. Open Windows **Control Panel → Region → Administrative** tab.
> 2. Click **Change system locale...** and select the region/language matching your chosen UI language.
> 3. Restart your computer.

---

## Language Pack Specification (For Localization Authors)

Adding a new language to Becky! requires **no programming knowledge**. You simply copy `en_US.json` from the `PlugIns\locale\` folder, rename it to your target locale code (for example, `nl_NL.json` or `sv_SE.json`), and translate fields such as `dst`, `replace`, `hint_dst`, and `tooltip_dst`: `dst` is used for menu and control text, `replace` for regex replacement strings, `hint_dst` for status-bar help text, and `tooltip_dst` for toolbar-button hover tooltips.

### 1. File Naming, Encoding & Top-Level Structure

* **File Naming**: `<locale_id>.json`. The `<locale_id>` must match the security whitelist regex `^[A-Za-z0-9_-]{1,32}$` (e.g., `en_US`, `zh_CN`, `fr_FR`).
* **File Encoding**: Must be saved as **UTF-8 without BOM**.
* **Top-Level Keys**: Every language JSON file consists of three top-level objects:
  1. `"meta"`: Metadata and the access key (mnemonic `&`) formatting rule.
  2. `"menus"`: Top-level menu definitions, divided into `"popups"` and `"commands"`.
  3. `"windows"`: Top-level window and child-control trees. Each key preferably uses a plain Win32 window class name (e.g., `"Becky2MainFrame"`, `"Becky2ComposeFrame"`), and the node binds to the top-level window of that class name through `class_name`; to configure rules separately for different top-level windows sharing the same class name, you may also use a `"ClassName:Window Title"` compound key, and bind the rule to the real window through `class_name` and the optional `properties.window_title` title-matching rule inside the node. At runtime the lookup follows the order "plain class-name key → compound key → general matching", and the first hit is bound.

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

### 2. Metadata (`meta`) & The Access Key (`&`) Contract

```json
"meta": {
  "locale": "zh_CN",
  "name": "简体中文 (zh_CN)",
  "access_key": "$1(&2)$3",
  "version": "1.0.8",
  "author": "Martin Young"
}
```

| Field | Type | Description |
| :--- | :--- | :--- |
| `locale` | `string` | Unique locale identifier matching the filename (e.g., `"zh_CN"`). |
| `name` | `string` | Display name shown in the **UI Language** dialog's drop-down list (`Tools → UI Language`) and the **About Becky! I18n** dialog (e.g., `"简体中文 (zh_CN)"`). |
| `access_key` | `string` | Mnemonic (`&`) formatting mode: either `"as_is"` or a pattern matching template using `$1`, `&2`, and `$3`. |
| `version` | `string` | Language file version (e.g., `"1.0.7"`). |
| `author` | `string` | Author or maintainer of the language pack. |

#### Normalization of `src` Strings

In the language file schema, all `src` strings in menus and mnemonic-capable controls are **normalized**:

1. A trailing ellipsis (`...`, three ASCII dots) or a half-width colon (`:`) at the end of `src` is stripped (e.g., `"&Import..."` is normalized to `"Import"`, and `"&Cc:"` to `"Cc"`).
2. Single mnemonic ampersands (`&`) and trailing parenthesized access keys like `(&Z)` are stripped from `src`.
3. Double ampersands (`&&`) in Win32 strings are collapsed to a single literal `"&"` in `src`.

#### How Runtime Processes `dst` (`as_is` vs. Pattern Matching Mode)

`locale.dll` supports two distinct modes configured via `meta.access_key`:

##### Mode A: `"as_is"` Mode (Used by `en_US.json`)

* **Behavior**: The runtime outputs `dst` **verbatim without any modification**.
* **Author Responsibility**: The language author has full control and responsibility over the final presentation format in `dst`, including placing `&` before the mnemonic character, escaping literal `&` as `&&`, and preserving original trailing punctuation such as `...` and `:` (e.g., `"src": "Delete Folder"`, `"dst": "De&lete Folder..."`).

##### Mode B: Pattern Matching Mode (e.g., `"$1(&2)$3"`)

* **Behavior**: `dst` is written as **normalized plain text**. Under pattern matching mode, authors **must not** manually write `&` access keys or trailing `...` / `:` in `dst` (e.g., in `zh_CN.json`, simply write `"src": "Delete Folder"`, `"dst": "删除文件夹"`); at runtime the plug-in automatically performs “`&` escaping + access-key insertion + trailing-punctuation (`...` / `:`) restoration”.
* **Placeholder Definitions in the Pattern**:
  * **`$1`**: Represents the main text string (the `src` / `dst` body after access keys and trailing punctuation (`...` / `:`) have been stripped).
  * **`&2`**: Represents the **access key** (e.g., `&F`, extracted from the live Win32 `src` string).
  * **`$3`**: Represents the trailing punctuation inherited from the end of the **live Win32 `src` string**: it is `"..."` when the source ends with three ASCII periods, or `":"` when it ends with a half-width colon `:` (the two are mutually exclusive; at most one is matched); otherwise it is the empty string `""`. The runtime automatically appends this punctuation back to the end of the translated text based on the live source, so `dst` does not need to include it. Only ASCII `...` and the half-width colon `:` are recognized; a full-width colon `：` or the single ellipsis character `…` is not restored automatically.
  * **Template ordering rule**: Always place `$3` at the **very end** of the `meta.access_key` template string (all official templates below follow this convention); do not put it before or between `$1` and `&2`.
* **Common Pattern Examples** (illustrated with live `src` = `"Save &As..."`, `dst` = `"另存为"`):

| Pattern (`meta.access_key`) | Placement Style | Rendered Output for `"Save &As..."` |
| :--- | :--- | :--- |
| **`"$1(&2)$3"`** | Trailing parenthesized access key (standard CJK style) | `"另存为(&A)..."` |
| **`"[&2]$1$3"`** | Leading bracketed access key | `"[&A]另存为..."` |
| **`"&2-$1$3"`** | Leading hyphenated access key | `"&A-另存为..."` |
| **`"&2. $1$3"`** | Leading numbered/dotted access key | `"&A. 另存为..."` |

* **Trailing-colon example** (using the Bcc/Cc buttons of the Compose window: the live Win32 `src` is `"&Bcc:"` / `"&Cc:"`, while the normalized entries in the language file are `"src": "Bcc", "dst": "密送"` and `"src": "Cc", "dst": "抄送"`; the template remains `"$1(&2)$3"`):

| Live Win32 `src` | Language-file entry (normalized `src` / `dst`) | Rendered result of `"$1(&2)$3"` |
| :--- | :--- | :--- |
| `"&Bcc:"` | `"src": "Bcc"`, `"dst": "密送"` | `"密送(&B):"` |
| `"&Cc:"` | `"src": "Cc"`, `"dst": "抄送"` | `"抄送(&C):"` |

* As with `...` in the previous example, the trailing half-width colon `:` is restored to the end of the translated text by `$3`, while the access key `B` / `C` is inserted by `&2`; `dst` contains only the plain translation (`密送` / `抄送`), with neither `&` nor a trailing colon.

#### Single-Rule `"as_is": true` (Per-Entry Verbatim Output)

Besides the **whole-pack** mode determined by `meta.access_key` (Modes A/B above), `"as_is": true` can be added to an individual menu, popup-menu, or control rule node. When that entry matches, the plug-in skips access-key template post-processing and outputs the entry's `dst` (or `replace` for regex rules) **verbatim**, leaving `&` escaping and trailing punctuation to the author, for example:

```json
{ "src": "Special Label", "dst": "特判(&S)", "as_is": true }
```

In a Mode B language pack, `dst` should normally be normalized plain text without `&`; a per-entry `"as_is": true` is the only exception channel (the other exception is the English master `en_US.json` with `"access_key": "as_is"`, which preserves the original Becky! presentation verbatim). Use it sparingly, only for special entries that the template cannot express.

### 3. Menu System (`menus`)

Menus are decoupled from individual windows and placed at the top level under `"menus"`, split into `"popups"` and `"commands"`.

#### 3.1 `menus.popups` (Popup Menu Trees)

Popup containers (submenus without a fixed `cmd_id`) are organized into three window buckets: `"main_frame"`, `"compose"`, and `"address"`. Each node contains `translate`, `src`, `dst`, and optional nested `children`.

#### 3.2 `menus.commands` (Global Command Pool)

All actionable menu items in Becky! are stored in one flat array sorted ascending by `cmd_id`. Because Win32 command IDs have global meaning, this pool simultaneously drives **menu-item text**, **toolbar-button hover tooltips**, and **status-bar help text**: `hint_src` / `hint_dst` carry the status-bar help text, while `tooltip_src` / `tooltip_dst` carry the small hover tooltips of toolbar buttons. Both menu/command nodes and toolbar controls (the `buttons` array of a `ToolbarWindow32`) may provide `tooltip_*` fields; a tooltip on a node can still take effect independently even when that node has `"translate": false`, and it can be explicitly disabled with `"tooltip_translate": false`.

`menus.commands` supports three node patterns:

1. **Fixed Single Command (`cmd_id` + `src`/`dst`)**
2. **Multi-Variant Command (`cmd_id` + `rules`)**: Used when the same `cmd_id` displays different text depending on the active window (e.g., `57604` or `57636`)
3. **Dynamic Command ID Range (`cmd_id` + `cmd_id_end` + `rules`)**: Used for dynamic ranges where Becky! assigns IDs at runtime. Any runtime item whose text is not in `rules` (such as user-created dialup connection names) is safely left untranslated:
   * `1`–`100` (`"ownerdraw": true`): Header fields popup menus.
   * `10000`–`10099`: Editor, decoding, formatting, charset, and toolbar-specific commands.
   * `20000`–`31999`: Plug-in command range (e.g., PGP plug-in, Voice Attachment plug-in, Becky-i18n plug-in).
   * `55001`–`55099`: View → Language character set menu items.
   * `56001`–`56099`: Dialup connection menu items (`(no dialup)`).
   * `56100`–`56199`: Tools → Plug-Ins Setup dynamic items.

### 4. Window & Control Tree (`windows`)

Each key under `"windows"` corresponds to one top-level window rule (a plain class-name key or a `"ClassName:identifier"` compound key), and the node recursively mirrors its Win32 child-control tree through nested `"children"`. Child controls are located precisely by `class_name` and `ctrl_id`, optionally supplemented by annotation fields such as `control_subtype` and `sdk_role` (currently `sdk_role` is descriptive metadata only and does not participate in runtime matching). Beyond child controls, the node can also configure two kinds of window-level rules in `properties`: `window_title` (window title translation) and `glyph_menu` (owner-drawn popup menu translation).

| Control Class (`class_name`) | `ctrl_id` | Translation Configuration in JSON |
| :--- | :--- | :--- |
| **`SysTreeView32`** | `1000` | System folder tree. `"image_indices"` lists the icon indices of built-in folders covered by the current language data (`12`–`23`, plus `38` and `39` used by task folders; the set may grow with host versions), and `"root_nodes"` recognizes dynamic root nodes at runtime (e.g., Query Result). User-created mail folders are never mistranslated. |
| **`SysHeader32`** | `0` | Mail list column headers inside `SysListView32`. |
| **`AfxWnd` (Tab Strip)** | `12562` | Owner-draw message view bottom tab bar. |
| **`Static`** | Various | Standard labels. |
| **`Button`** | Various | Standard push buttons. |
| **`Edit`** | Various | Standard edit fields. |
| **`ComboBox`** | `1532` | Drop-down list items matched by 0-based index and text. |
| **`msctls_statusbar32`** | `59393` | Bottom status bar divided into `"panes"` by `index`. |

> **Toolbar buttons and owner-drawn text**: a `ToolbarWindow32` control describes its buttons and their `tooltip_src` / `tooltip_dst` through a `"buttons"` array; owner-drawn GDI text without an independent window handle (such as Task pane status lines) does not go through the control matching of this table, but is handled by a node-level `"gdi_patterns"` regex array on the control node.

#### 4.1 Window-Level Properties (`properties`): Window Title and Owner-Drawn Menus

The window node's `properties` sits at the same level as `"children"` and describes window-level text that does not belong to a specific child control. It currently contains two optional fields.

**`window_title`: Top-level window title translation.** When a window of the same class name shows multiple titles at runtime, you can provide several regular expressions; the runtime takes the **first match** from top to bottom (first-match-wins) rather than chaining replacements:

```json
"window_title": {
  "translate": true,
  "mode": "regex",
  "rules": [
    { "pattern": "^Compose - From:\\s*(.*)$",   "replace": "撰写 - 发件人: $1" },
    { "pattern": "^Reply - From:\\s*(.*)$",     "replace": "回复 - 发件人: $1" },
    { "pattern": "^Template - \\[(.*)\\]\\s*$", "replace": "模板 - [$1]" }
  ]
}
```

**`glyph_menu`: Owner-drawn popup menu translation.** The text of some owner-drawn popup menus is not stored in the menu item strings, in which case it is handled uniformly by the window-level `glyph_menu`. The rules are likewise first-match-wins:

```json
"glyph_menu": {
  "translate": true,
  "mode": "regex",
  "rules": [
    { "pattern": "^(\\d+:\\s*)High$",   "replace": "$1高" },
    { "pattern": "^(\\d+:\\s*)Normal$", "replace": "$1普通" },
    { "pattern": "^(\\d+:\\s*)Low$",    "replace": "$1低" }
  ]
}
```

### 5. Translation Matching Rules

Across menu nodes, window controls, and status bar panes, translation rules are organized along two orthogonal dimensions:

1. **Rule Cardinality (Single vs. Multi-Rule)**:
   * **Single-Rule Matching**: When a node only needs to match a single text pattern, the matching fields are written directly as properties of that JSON node.
   * **Multi-Rule Matching (`"rules"` array)**: When a single node (such as a dynamic command range, a multi-variant command, a tab strip, or a status bar pane) needs to match multiple candidate strings or patterns, the rules are grouped inside a `"rules": [ ... ]` array.
2. **Matching Mode (Exact vs. Regex)**:
   * **Exact Mapping (`"src"` → `"dst"`)**: Default mode. Performs fast exact string matching (or normalized command lookup) and replaces `"src"` with `"dst"`.
   * **Regular Expression Mode (`"mode": "regex"` + `"pattern"` / `"replace"`)**: Enabled by setting `"mode": "regex"`. Uses ECMAScript regular expressions (`"pattern"`) and supports capture group substitutions (`$1`, `$2`, etc.) in `"replace"`.

| Cardinality \ Mode | Exact Mapping (`src` → `dst`) | Regex Matching (`mode: "regex"` + `pattern` / `replace`) |
| :--- | :--- | :--- |
| **Single-Rule** *(Inline fields)* | `"src"` and `"dst"` defined directly on the node. | `"mode": "regex"`, `"pattern"`, and `"replace"` defined directly on the node. |
| **Multi-Rule** *(`"rules"` array)* | `"rules": [ { "src": "...", "dst": "..." }, ... ]` | `"mode": "regex"` with `"rules": [ { "pattern": "...", "replace": "..." }, ... ]` |

#### 5.1 Single-Rule + Exact Mapping
Used when a control or fixed command has a single static text label:

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

#### 5.2 Multi-Rule + Exact Mapping
Used when a single command ID, dynamic command range, or custom control can display one of several fixed strings at runtime:

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

#### 5.3 Single-Rule + Regex Matching
Used when a control or status bar pane displays a single dynamic text format with runtime variables (such as line/column numbers or inline recipient prefixes):

```json
{
  "index": 2,
  "translate": true,
  "mode": "regex",
  "pattern": "^R:(\\d+)(\\s+)C:(\\d+)(\\s+)Lines:(\\d+)(\\s*)$",
  "replace": "行:$1$2列:$3$4总行:$5$6"
}
```

#### 5.4 Multi-Rule + Regex Matching
Used when a control or status bar pane can display multiple distinct dynamic string formats that each require regular expression capture groups:
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

#### 5.5 Owner-Drawn GDI Text Rules (`gdi_patterns`, Optional)

Some Becky! UI is drawn directly by the host with GDI `DrawTextA` and has no corresponding standard control text (for example the `Task:` / `Deadline:` / `Comment:` status lines in the Task pane, and task-type labels such as `{Need to Reply}` in the mail list). Such text cannot be matched by `src` / `dst` control rules; instead, add an optional `"gdi_patterns"` array on the container window or control node that hosts the drawing (as a sibling of `"children"`):

```json
"gdi_patterns": [
  { "pattern": "\\{Need to Reply\\}", "replace": "{待回复}" },
  { "pattern": "^Deadline:", "replace": "截止:" },
  { "pattern": "^Comment:",  "replace": "备注:" }
]
```

* Each element is a regex rule: `"pattern"` uses ECMAScript regex syntax, and `"replace"` is the replacement string with capture-group support (`$1`, `$2`, etc.). Rules run in array order as **substring replacements**, so you may replace only a fixed leading label while using loose patterns and capture groups to preserve dates, remaining days, message bodies, and other dynamic content.
* An individual rule may set `"translate": false` to temporarily disable it without deleting it.
* A non-empty `"gdi_patterns"` array takes effect even when the containing container node has top-level `"translate": false`—the host often marks such containers `false` and translates the real child controls only through their `children`.
* Its scope is limited to the drawing cycle of that window (including the double-buffered memory DC); owner-drawn surfaces without `"gdi_patterns"` configured keep their original host behavior.

### 6. Runtime Safety Limits

To guarantee host process stability (`B2.exe`), `locale.dll` enforces strict validation limits (`src/limits.hpp`) when parsing any `.json` language pack. Exceeding any limit causes the plug-in to reject the malformed file/rule and safely fall back to the native `en_US` UI:

| Limit Constant | Max Value | Description |
| :--- | :--- | :--- |
| `kMaxJsonFileBytes` | `2 MB` | Maximum size of a single locale `.json` file (in UTF-8 bytes). |
| `kMaxNestingDepth` | `20` | Maximum JSON object/array nesting depth. |
| `kMaxStringBytes` | `4096` | Maximum UTF-8 byte length of any single JSON string value. |
| `kMaxRegexPatternBytes` | `256` | Maximum UTF-8 byte length of a `regex` `pattern`. |
| `kMaxRegexInputChars` | `1024` | Maximum UTF-16 character length of a runtime string evaluated by regex. |
| `kMaxArrayElements` | `256` | Maximum elements allowed in any JSON array (`commands`, `rules`, `children`, etc.). |
| `kMaxLocaleFiles` | `32` | Maximum number of `.json` files scanned in the `locale\` directory. |
| `kMaxLocaleIdLength` | `32` | Maximum character length of a locale identifier (`^[A-Za-z0-9_-]{1,32}$`). |
| `kMaxLogFileBytes` | `1 MB` | Maximum diagnostic log file size before automatic truncation/recreation. |

---

## Technical Architecture & Building from Source (For Developers)

### 1. Core Translation Engine

Because Becky! hardcodes its UI strings and uses a mix of standard Win32 controls, MFC views, and custom GDI owner-drawn surfaces, `locale.dll` employs a dual-path interception architecture:

1. **SDK Callbacks + Window Subclassing (`UiTranslator`)**:
   * Hooks into Becky!'s exported `BKC_On*` lifecycle callbacks.
   * Recursively walks the HWND hierarchy of window and subclasses target window procedures to intercept notifications.
2. **Narrow-Scope Win32/GDI API Detours (`ApiHooks` via MinHook)**:
   * Owner-Draw Header Popup Menu (`ExtTextOutW` + EMF Pre-Replay).
   * Glyph-index owner-drawn popup menu interception (`ExtTextOutW` / `ETO_GLYPH_INDEX`): within the popup menu's drawing cycle it reverse-maps the glyphs using the current font via `GetGlyphIndicesW`, then applies the window-level `glyph_menu` rules.
   * Owner-drawn MIME tab strip interception (subclass scope plus `DrawTextA` interception on the double-buffered memory DC); the same `DrawTextA` channel also translates handle-less owner-drawn GDI text such as Task pane status lines and mail-list task labels according to control-node `gdi_patterns` regex rules.
   * Zero-Overhead Pass-Through & Memory Safety (`win32_guard.hpp`): Unmatched calls pass through with near-zero overhead. All pointers inspected in hooks/callbacks are validated via user-mode address range checks and SEH readability probes so exceptions never escape into `B2.exe`.

### 2. Development Environment & Third-Party Dependencies

#### Compiler & Platform Requirements

* **OS**: Windows 10 (or newer).
* **Compiler**: **Visual Studio 2022** (or newer) with the **Desktop development with C++** workload (MSVC toolchain).
* **C++ Standard**: **C++20** (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`, `/utf-8 /W3 /permissive- /EHsc`).
* **Target Architecture**: **32-bit x86 (`Win32`) ONLY**. Because Becky! (`B2.exe`) is a 32-bit executable, CMake explicitly checks `CMAKE_SIZEOF_VOID_P` and aborts with a `FATAL_ERROR` if configured for 64-bit.
* **CRT Linkage**: **Static CRT** (`MultiThreaded` `/MT` for Release, `MultiThreadedDebug` `/MTd` for Debug) so `locale.dll` runs on any Windows machine without requiring VC++ Redistributable installations.
* **Build System**: **CMake ≥ 3.15**

#### External SDK & Third-Party Libraries (Offline Local Build)

By design, `CMakeLists.txt` does **not** fetch code from the internet during build time. It requires the official **Becky! Plug-In SDK** and two open-source libraries cloned locally:

1. **Becky! Official Plug-In SDK (`Becky-SDK`)**
   * **Download**: Available from the official Becky! download page: <https://www.rimarts.jp/becky.htm#download>
   * **Required Files**: `BeckyAPI.cpp` and `BeckyApi.h`
2. **nlohmann/json** (Header-only JSON library for Modern C++)
   * **Repository**: <https://github.com/nlohmann/json>
3. **MinHook** (Minimalistic x86/x64 API Hooking Library for Windows)
   * **Repository**: <https://github.com/TsudaKageyu/minhook>

#### Recommended Directory Layout

Place `Becky-SDK`, `nlohmann/json`, and `minhook` as sibling directories next to `Becky-i18n`:

```text
<workspace>/
├── Becky-i18n/          # This repository
│   ├── CMakeLists.txt   # Top-level CMake build script
│   ├── locale.def       # Module definition
│   └── src/             # C++ source files
├── Becky-SDK/           # Becky! Plug-In SDK
├── nlohmann/
│   └── json/            # git clone https://github.com/nlohmann/json.git
└── minhook/             # git clone https://github.com/TsudaKageyu/minhook.git
```

### 3. Building `locale.dll`

#### Option A: Using the Default Sibling Directory Layout

Open PowerShell or Command Prompt and run:

```powershell
# 1. Clone dependencies into sibling directories
git clone https://github.com/nlohmann/json.git nlohmann/json
git clone https://github.com/TsudaKageyu/minhook.git minhook
# (Ensure Becky-SDK/ with BeckyAPI.cpp & BeckyApi.h is also placed in the same parent folder)

# 2. Configure and build Becky-i18n (MUST specify -A Win32)
cd Becky-i18n
cmake -S . -B build -A Win32
cmake --build build --config Release
```

#### Option B: Specifying Custom Dependency Paths

If your SDK or third-party repositories are stored elsewhere, pass their absolute paths via `-D`:

```powershell
cd Becky-i18n
cmake -S . -B build -A Win32 `
  -DBECKY_SDK_DIR="D:/sdk/Becky-SDK" `
  -DNLOHMANN_JSON_DIR="D:/third_party/json" `
  -DMINHOOK_DIR="D:/third_party/minhook"

cmake --build build --config Release
```

Once built, the compiled plug-in binary will be located at `build/Release/locale.dll`.

#### Optional: Appending a Pre-Release Suffix to the DLL Version Resource

At configure time, `-DBECKY_VERSION_SUFFIX=<identifier>` appends a SemVer pre-release suffix to the generated DLL file version and `version.hpp`; the major/minor/patch version comes from the top-level `project(... VERSION ...)`:

```powershell
cmake -S . -B build -A Win32 -DBECKY_VERSION_SUFFIX=rc.3
cmake --build build --config Release
```

The full version identifier produced by the example above is `1.0.0-rc.3`; release builds normally leave this option empty.

---

## Intellectual Property, Credits & Third-Party Notices

This project respects the intellectual property rights of all original authors and third-party open-source projects:

1. **Becky! Internet Mail & Becky! Plug-In SDK**
   * **Copyright © RimArts, Inc. All rights reserved** (<https://www.rimarts.jp/>)
   * *Becky! Internet Mail* is a registered trademark and proprietary software of RimArts, Inc., authored by Tomohiro Norimatsu
   * `Becky-i18n` is an independent, community-driven open-source plug-in and is not affiliated with, sponsored by, or officially endorsed by RimArts, Inc.
2. **JSON for Modern C++ (`nlohmann/json`)**
   * **Copyright © 2013–2026 Niels Lohmann** (<https://github.com/nlohmann/json>)
   * Licensed under the **MIT License**
3. **MinHook — The Minimalistic x86/x64 API Hooking Library for Windows**
   * **Copyright © 2009–2017 Tsuda Kageyu. All rights reserved** (<https://github.com/TsudaKageyu/minhook>)
   * Licensed under the **BSD 2-Clause License**
4. **Becky-i18n (`locale.dll` & Language Packs)**
   * **Copyright © 2026 Martin Young & Becky-i18n Contributors**
   * Licensed under the **MIT License**

---

## Patronage & Sponsorship

🙏 Your patronage would directly fuel this project: [![Patron](https://img.shields.io/badge/❤%EF%B8%8F-Patron-8b2635?style=flat-square)](patron/PATRON.md)

---

## License

This project is open-source software licensed under the [MIT License](LICENSE).