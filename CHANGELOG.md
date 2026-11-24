# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0-rc.4] - 2026-11-25

### Added

- Data-driven `glyph_menu` rules on the Compose window for translating the
  owner-drawn priority menu (five priority levels).
- Multi-rule regular-expression `window_title` rules covering all six Compose
  window title forms.

### Fixed

- The Compose window is now bound by its pure class-name key
  (`Becky2ComposeFrame`) instead of a title-dependent composite key, so all
  controls in all six Compose windows are translated.
- Added the wide-character (`W`) top-level window title translation branch,
  matching the existing ANSI branch, so runtime title updates are not reverted
  to English.

## [1.0.0-rc.3] - 2026-11-23

### Added

- Optional data-driven `gdi_patterns` rules for control nodes, allowing
  regular-expression translation of owner-drawn GDI text surfaces that are
  not standard controls. This enables translation of the Tasks pane status
  lines (`Task:`, `Deadline:`, `Comment:`) and the task tags shown in the
  message list; surfaces without these rules keep their previous behavior.
- Per-entry `as_is` flag for menu, popup and control rules, so a mnemonic
  ampersand explicitly written by the language author in a translation is
  preserved verbatim instead of being reformatted.
- Two additional bundled languages: Italian (`it_IT`) and Polish (`pl_PL`),
  bringing the total to 16 language packs under `locale/`.
- The bundled language packs now translate the Compose message window and
  the Tasks view, including the owner-drawn Task/Deadline/Comment lines, the
  system Tasks folder node, message-list task tags and related tooltips.

### Changed

- Refreshed the fourteen previously bundled language files with the latest
  translations and terminology.

## [1.0.0-rc.2] - 2026-11-18

### Changed

- Language packs are now scanned when the UI Language picker is opened
  instead of during startup, so adding or removing a language file takes
  effect the next time the picker is opened, without restarting.

### Fixed

- A crash that could terminate the host application when a new mailbox is
  created and a new root node is inserted into the folder tree.
- The UI Language picker now always lists English and the currently
  selected language, even when the active language pack file is missing.
- When the active language pack is unavailable, the plug-in safely falls
  back to English and records the fallback in the registry.

## [1.0.0-rc.1] - 2026-11-17

First public pre-release.

### Added

- Non-intrusive, data-driven localization plug-in (`locale.dll`) built with
  C++20 on the official Becky! Plug-In SDK. `B2.exe` is never modified or
  binary-patched.
- Dual-path interception engine:
  - SDK callbacks with Win32 window subclassing for menus and controls;
  - narrow-scope Win32/GDI API detours (MinHook) for owner-draw surfaces and
    internal text-update paths, with zero-overhead pass-through when no rule
    matches.
- Main window localization: system folder tree view with dynamic root nodes,
  mail list column headers, message view header labels and controls,
  owner-draw MIME tab strip, and dynamic status bar panes.
- Full menu system coverage: top-level and nested popup menus across the main
  frame, compose window and address book, a global command pool with dynamic
  ID ranges, toolbar tooltips, status bar help hints, and the owner-draw header
  fields popup menu.
- Modal UI Language picker dialog and a Tools → UI Language menu entry; the
  selection is stored in the registry and applied on restart.
- Access key (mnemonic) contract that preserves Alt shortcuts while keeping
  literal ampersands correctly escaped.
- Fourteen UTF-8 language packs under `locale/`: `en_US`, `de_DE`, `es_ES`,
  `fr_FR`, `ja_JP`, `ko_KR`, `ms_MY`, `pt_PT`, `ru_RU`, `th_TH`, `uk_UA`,
  `vi_VN`, `zh_CN`, and `zh_TW`.
- Runtime safety limits (file size, nesting depth, string length, array size,
  regex bounds) with safe fallback to the native English UI, plus user-mode
  pointer validation so exceptions never escape into the host process.
- Static CRT linkage and an embedded Windows version resource, so the
  distributed DLL depends only on Windows system libraries.

[1.0.0-rc.4]: https://github.com/robot-dot-win/Becky-i18n/compare/v1.0.0-rc.3...v1.0.0-rc.4
[1.0.0-rc.3]: https://github.com/robot-dot-win/Becky-i18n/compare/v1.0.0-rc.2...v1.0.0-rc.3
[1.0.0-rc.2]: https://github.com/robot-dot-win/Becky-i18n/compare/v1.0.0-rc.1...v1.0.0-rc.2
[1.0.0-rc.1]: https://github.com/robot-dot-win/Becky-i18n/releases/tag/v1.0.0-rc.1
