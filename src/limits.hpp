//  Defines the centralized hardening limits used when loading and validating locale packages.
//
//  Copyright (C) 2026, Martin Young <martin_young@live.cn>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the MIT License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  MIT License for more details.
//
//  You should have received a copy of the MIT License
//  along with this program. If not, see <https://opensource.org/licenses/MIT>.
//------------------------------------------------------------------------------
#pragma once

// 产品级加固限额集中定义。
// 所有数值均为当前开发阶段的保守默认值，正式发行前按实际语言包规模再调整。
//
// 约定：
// - 字符串类限额（JSON 字节串）按 UTF-8 字节数计；
// - 正则运行期输入按 UTF-16 字符数计；
// - 任一限额触发即拒绝整个语言包（或单条规则），并回退到 en_US 原生界面。

#include <cstddef>

namespace bki18n::limits {

// 单个 locale JSON 文件最大字节数（当前语言包约 100 KB 量级）
inline constexpr std::size_t kMaxJsonFileBytes = 2u * 1024u * 1024u;  // 2 MB

// JSON 结构最大嵌套深度（菜单/控件树实际深度小于 15）
inline constexpr unsigned kMaxNestingDepth = 20;

// 单条 JSON 字符串最大 UTF-8 字节数
inline constexpr std::size_t kMaxStringBytes = 4096;

// 正则 pattern 最大 UTF-8 字节数
inline constexpr std::size_t kMaxRegexPatternBytes = 256;

// 运行期送入正则引擎的最大 UTF-16 字符数，超出则跳过正则匹配
inline constexpr std::size_t kMaxRegexInputChars = 1024;

// 单个数组允许的最大元素数（rules/texts/children/buttons/commands 等）
inline constexpr std::size_t kMaxArrayElements = 256;

// locale 目录允许扫描的最大语言包数量
inline constexpr std::size_t kMaxLocaleFiles = 32;

// locale 标识最大长度（白名单 ^[A-Za-z0-9_-]{1,32}$）
inline constexpr std::size_t kMaxLocaleIdLength = 32;

// 日志文件大小上限，超过则在下一次插件启动时截断重建
inline constexpr std::size_t kMaxLogFileBytes = 1u * 1024u * 1024u;  // 1 MB

// 正则冒烟测试合成输入的字符数
inline constexpr std::size_t kRegexSmokeTestLength = 256;

}  // namespace bki18n::limits
