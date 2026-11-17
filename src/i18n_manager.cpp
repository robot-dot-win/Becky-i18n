//  Implements locale loading, validation, and indexing, along with text matching and access-key application.
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
#include "i18n_manager.hpp"
#include "config_reg.hpp"
#include "limits.hpp"
#include "logger.hpp"

#include <fstream>
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <string>

namespace bki18n {

namespace {

inline wchar_t ToUpperAsciiW(wchar_t ch) noexcept {
    if (ch >= L'a' && ch <= L'z') {
        return static_cast<wchar_t>(ch - L'a' + L'A');
    }
    return ch;
}

inline bool EqualsIgnoreCaseAsciiW(wchar_t a, wchar_t b) noexcept {
    return ToUpperAsciiW(a) == ToUpperAsciiW(b);
}

inline bool IsAccessKeyFormatAsIs(std::wstring_view fmt) noexcept {
    if (fmt.empty()) return true;
    return _wcsicmp(std::wstring(fmt).c_str(), L"as_is") == 0 ||
           _wcsicmp(std::wstring(fmt).c_str(), L"as is") == 0;
}

// 从原文提取热键字母 (&2) 与尾部是否是三个点 ($3)
void ExtractSrcAccessKeyAndEllipsis(std::wstring_view rawSrc, wchar_t& outKey, bool& outHasEllipsis) noexcept {
    outKey = L'\0';
    outHasEllipsis = rawSrc.ends_with(L"...");

    for (size_t i = 0; i + 1 < rawSrc.size(); ++i) {
        if (rawSrc[i] == L'&') {
            if (rawSrc[i + 1] == L'&') {
                ++i; // 跳过转义的 &&
                continue;
            }
            wchar_t nextCh = rawSrc[i + 1];
            if (nextCh != L' ' && nextCh != L'\t' && nextCh != L'\r' && nextCh != L'\n') {
                outKey = ToUpperAsciiW(nextCh);
                break;
            }
        }
    }
}

// 存储/匹配规范化（与 inspector normalize_caption_text 完全对齐）：
// 剥离尾部 "..."、单 & 助记符删除、"&&" 折叠为字面 &，保留大小写与其余空白。
// 对已规范化文本幂等，可安全用于实时文本归一与数据 src 两侧。
std::wstring NormalizeCaptionW(std::wstring_view input) {
    std::wstring_view sv = input;
    if (sv.ends_with(L"...")) {
        sv.remove_suffix(3);
    }

    std::wstring out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == L'&') {
            if (i + 1 < sv.size() && sv[i + 1] == L'&') {
                out.push_back(L'&'); // "&&" -> 字面 &
                ++i;
            }
            // 单个 &（助记符）直接丢弃
        } else {
            out.push_back(sv[i]);
        }
    }
    return out;
}

// 匹配模式下，dst 规范化正文里的所有 & 都是字面 &（助记符已在数据端删除），
// 统一转义为 "&&"，避免被控件/菜单解释成助记符。
std::wstring EscapeLiteralAmpersandsW(std::wstring_view input) {
    if (input.find(L'&') == std::wstring_view::npos) {
        return std::wstring(input);
    }
    std::wstring out;
    out.reserve(input.size() + 1);
    for (wchar_t ch : input) {
        if (ch == L'&') {
            out.push_back(L'&');
            out.push_back(L'&');
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

// 语言包加载期的内部错误：触发即拒绝整个语言包并回退 en_US
struct LoadError : std::runtime_error {
    explicit LoadError(const std::string& msg) : std::runtime_error(msg) {}
};

// 校验 UTF-8 字节串长度并禁止内嵌 NUL
void CheckString(std::string_view s, const char* what) {
    if (s.size() > limits::kMaxStringBytes) {
        throw LoadError(std::string("string exceeds limit at ") + what);
    }
    if (s.find('\0') != std::string_view::npos) {
        throw LoadError(std::string("embedded NUL at ") + what);
    }
}

// 校验 JSON 数组元素数量
void CheckArraySize(std::size_t count, const char* what) {
    if (count > limits::kMaxArrayElements) {
        throw LoadError(std::string("array elements exceed limit at ") + what);
    }
}

// 编译 UTF-8 pattern 并执行加载期冒烟测试。
// 校验 pattern 长度；用合成输入试匹配，提前触发引擎 error_complexity/error_stack。
// 任何失败返回 std::nullopt，调用方跳过该规则。std::regex 无代码执行能力，不存在注入。
std::optional<std::wregex> CompileRegexChecked(std::string_view patUtf8) {
    if (patUtf8.empty() || patUtf8.size() > limits::kMaxRegexPatternBytes) {
        return std::nullopt;
    }
    try {
        std::wregex re(I18nManager::utf8_to_wide(patUtf8),
                       std::regex_constants::ECMAScript | std::regex_constants::optimize);

        const std::wstring probe(limits::kRegexSmokeTestLength, L'a');
        (void)std::regex_match(probe, re);
        (void)std::regex_search(probe, re);
        return re;
    } catch (...) {
        return std::nullopt;
    }
}

// 运行期正则安全包装：输入超 kMaxRegexInputChars 直接不匹配；
// 引擎异常（complexity/stack/bad_alloc）吞掉按不匹配处理，绝不逃逸到系统回调。
bool SafeRegexMatch(const std::wregex& re, const std::wstring& input) noexcept {
    if (input.size() > limits::kMaxRegexInputChars) return false;
    try {
        return std::regex_match(input, re);
    } catch (...) {
        return false;
    }
}

bool SafeRegexSearch(const std::wregex& re, const std::wstring& input) noexcept {
    if (input.size() > limits::kMaxRegexInputChars) return false;
    try {
        return std::regex_search(input, re);
    } catch (...) {
        return false;
    }
}

// 替换失败（含输入超限/异常）时原样返回输入，调用方据此跳过写入
std::wstring SafeRegexReplace(const std::wregex& re,
                              const std::wstring& input,
                              const std::wstring& fmt) noexcept {
    if (input.size() > limits::kMaxRegexInputChars) return input;
    try {
        return std::regex_replace(input, re, fmt);
    } catch (...) {
        return input;
    }
}

} // namespace

I18nManager& I18nManager::instance() {
    static I18nManager inst;
    return inst;
}

bool I18nManager::is_valid_locale_id(std::string_view id) noexcept {
    if (id.empty() || id.size() > limits::kMaxLocaleIdLength) {
        return false;
    }
    for (const char ch : id) {
        const bool ok =
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-';
        if (!ok) return false;
    }
    return true;
}

std::string I18nManager::wide_to_utf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int len = ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                    nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                          out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring I18nManager::utf8_to_wide(std::string_view u8str) {
    if (u8str.empty()) return {};
    int len = ::MultiByteToWideChar(CP_UTF8, 0, u8str.data(), static_cast<int>(u8str.size()),
                                    nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, u8str.data(), static_cast<int>(u8str.size()),
                          out.data(), len);
    return out;
}

std::wstring I18nManager::ansi_to_wide(std::string_view astr) {
    if (astr.empty()) return {};
    int len = ::MultiByteToWideChar(CP_ACP, 0, astr.data(), static_cast<int>(astr.size()),
                                    nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, astr.data(), static_cast<int>(astr.size()),
                          out.data(), len);
    return out;
}

std::string I18nManager::wide_to_ansi(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int len = ::WideCharToMultiByte(CP_ACP, 0, wstr.data(), static_cast<int>(wstr.size()),
                                    nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    ::WideCharToMultiByte(CP_ACP, 0, wstr.data(), static_cast<int>(wstr.size()),
                          out.data(), len, nullptr, nullptr);
    return out;
}

std::string I18nManager::ansi_to_utf8(std::string_view astr) {
    return wide_to_utf8(ansi_to_wide(astr));
}

std::string I18nManager::normalize_newlines(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r') {
            if (i + 1 < s.size() && s[i + 1] == '\n') {
                continue;
            }
            out.push_back('\n');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string I18nManager::trim_right_copy(std::string_view s) {
    size_t end = s.size();
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return std::string(s.substr(0, end));
}

std::wstring I18nManager::apply_access_key_format(std::wstring_view rawSrcWide, std::wstring_view rawDstWide) const {
    if (rawDstWide.empty()) return {};

    // as_is 模式：dst 原样输出，作者全权负责，不做任何助记符/三点处理
    if (access_key_as_is_) {
        return std::wstring(rawDstWide);
    }

    // 1. 从实时 src 提取助记符字母与尾部三点（实时文本带 &/...）
    wchar_t accessKey = L'\0';
    bool srcHasEllipsis = false;
    ExtractSrcAccessKeyAndEllipsis(rawSrcWide, accessKey, srcHasEllipsis);
    const std::wstring suffix3 = srcHasEllipsis ? L"..." : L"";

    // 2. dst 是规范化纯文本：防御性去掉残留尾部三点，字面 & 稍后统一转义
    std::wstring_view bodyView = rawDstWide;
    if (bodyView.ends_with(L"...")) {
        bodyView.remove_suffix(3);
    }

    // 3. 实时 src 无热键：仅转义字面 &，不插助记符，再恢复三点
    if (accessKey == L'\0') {
        return EscapeLiteralAmpersandsW(bodyView) + suffix3;
    }

    // 4. 在规范化正文（未转义）里定位 key 字母位置，忽略大小写，取第一个匹配
    size_t keyPos = std::wstring_view::npos;
    for (size_t i = 0; i < bodyView.size(); ++i) {
        if (EqualsIgnoreCaseAsciiW(bodyView[i], accessKey)) {
            keyPos = i;
            break;
        }
    }

    // 5. 正文命中 key：一次遍历同时完成"字面 & 转义 &&"与"key 前插助记符 &"
    if (keyPos != std::wstring_view::npos) {
        std::wstring out;
        out.reserve(bodyView.size() + 2 + suffix3.size());
        for (size_t i = 0; i < bodyView.size(); ++i) {
            if (i == keyPos) {
                out.push_back(L'&'); // 助记符
            }
            if (bodyView[i] == L'&') {
                out.push_back(L'&');
                out.push_back(L'&');
            } else {
                out.push_back(bodyView[i]);
            }
        }
        return out + suffix3;
    }

    // 6. 正文找不到 key：按 access_key 模板（$1=正文、&2=助记符、$3=三点）
    const std::wstring escapedBody = EscapeLiteralAmpersandsW(bodyView);
    std::wstring out;
    out.reserve(escapedBody.size() + access_key_pattern_.size() + suffix3.size() + 4);

    const std::wstring& tpl = access_key_pattern_;
    for (size_t i = 0; i < tpl.size(); ++i) {
        if (tpl[i] == L'$' && i + 1 < tpl.size()) {
            if (tpl[i + 1] == L'1') {
                out += escapedBody;
                ++i;
                continue;
            }
            if (tpl[i + 1] == L'2') {
                out.push_back(accessKey);
                ++i;
                continue;
            }
            if (tpl[i + 1] == L'3') {
                out += suffix3;
                ++i;
                continue;
            }
        } else if (tpl[i] == L'&' && i + 1 < tpl.size() && tpl[i + 1] == L'2') {
            out.push_back(L'&');
            out.push_back(accessKey);
            ++i;
            continue;
        }
        out.push_back(tpl[i]);
    }

    return out;
}

std::string I18nManager::normalize_class_name(std::string_view rawClass) {
    if (rawClass.starts_with("Afx:")) {
        return "AfxWnd";
    }
    if (rawClass.starts_with("AfxWnd")) {
        return "AfxWnd";
    }
    if (rawClass.starts_with("AfxControlBar")) {
        return "AfxControlBar";
    }
    if (rawClass.starts_with("AfxFrameOrView")) {
        return "AfxFrameOrView";
    }
    return std::string(rawClass);
}

void I18nManager::init(HINSTANCE hDll) {
    startup_locale_ = read_reg_locale();
    if (startup_locale_.empty() || !is_valid_locale_id(startup_locale_)) {
        if (!startup_locale_.empty() && startup_locale_ != "en_US") {
            BKI_LOG_WARN("invalid locale id in registry, fallback to en_US: " << startup_locale_);
        }
        startup_locale_ = "en_US";
    }

    const auto localeDir = get_locale_dir(hDll);
    scan_available_locales(localeDir);

    // 匹配当前启动语言的 Meta 信息（用于 About 窗口显示）
    for (const auto& meta : available_locales_) {
        if (meta.id == startup_locale_) {
            active_meta_ = meta;
            break;
        }
    }

    if (startup_locale_ == "en_US") {
        active_ = false;
        return;
    }

    const auto targetJson = localeDir / (startup_locale_ + ".json");
    active_ = load_locale_json(targetJson);
}

void I18nManager::scan_available_locales(const std::filesystem::path& localeDir) {
    available_locales_.clear();

    LocaleMeta defaultEn{
        "en_US",
        L"English (en_US)",
        L"(Becky! native)",
        L"(Becky! native)"
    };

    std::error_code ec;
    if (!std::filesystem::exists(localeDir, ec) || !std::filesystem::is_directory(localeDir, ec)) {
        available_locales_.push_back(std::move(defaultEn));
        return;
    }

    std::vector<LocaleMeta> others;

    std::size_t scanned = 0;
    for (const auto& entry : std::filesystem::directory_iterator(localeDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const auto& path = entry.path();
        if (path.extension() != L".json") continue;

        std::string stem = path.stem().string();
        // 彻底忽略 en_US.json 模板文件，不加载也不解析
        if (stem.empty() || stem == "en_US") continue;

        // 文件名即 locale id：白名单外的文件直接跳过，防路径穿越与非法字符
        if (!is_valid_locale_id(stem)) {
            BKI_LOG_WARN("skip locale file with invalid id: " << stem);
            continue;
        }
        // 语言包数量上限，防止目录塞满导致启动卡死
        if (scanned >= limits::kMaxLocaleFiles) {
            BKI_LOG_WARN("locale file count exceeds limit, ignore remaining files");
            break;
        }
        // 前置大小检查：超限文件不打开不解析
        if (const auto fileSize = entry.file_size(ec); !ec && fileSize > limits::kMaxJsonFileBytes) {
            BKI_LOG_WARN("locale file exceeds size limit, skip: " << stem);
            continue;
        }
        ++scanned;

        LocaleMeta meta;
        meta.id = stem;
        meta.display_name = utf8_to_wide(stem);
        meta.version = L"1.0.0";
        meta.author = L"Unknown";

        try {
            std::ifstream ifs(path, std::ios::binary);
            if (ifs) {
                nlohmann::json j = nlohmann::json::parse(ifs, nullptr, false);
                if (!j.is_discarded() && j.contains("meta") && j["meta"].is_object()) {
                    const auto& jm = j["meta"];
                    if (jm.contains("name") && jm["name"].is_string()) {
                        std::string nameStr = jm["name"].get<std::string>();
                        if (!nameStr.empty()) {
                            meta.display_name = utf8_to_wide(nameStr);
                        }
                    }
                    if (jm.contains("version") && jm["version"].is_string()) {
                        meta.version = utf8_to_wide(jm["version"].get<std::string>());
                    }
                    if (jm.contains("author") && jm["author"].is_string()) {
                        meta.author = utf8_to_wide(jm["author"].get<std::string>());
                    }
                }
            }
        } catch (...) {
        }

        others.push_back(std::move(meta));
    }

    std::sort(others.begin(), others.end(), [](const LocaleMeta& a, const LocaleMeta& b) {
        return a.id < b.id;
    });

    available_locales_.push_back(std::move(defaultEn));
    for (auto& item : others) {
        available_locales_.push_back(std::move(item));
    }
}

void I18nManager::parse_regex_rules(const nlohmann::json& j, std::vector<CompiledRegexRule>& outRules) {
    if (j.contains("rules") && j["rules"].is_array()) {
        const auto& ruleArray = j["rules"];
        CheckArraySize(ruleArray.size(), "regex.rules");
        for (const auto& r : ruleArray) {
            if (!r.is_object()) continue;
            std::string pat = r.value("pattern", "");
            std::string rep = r.value("replace", "");
            CheckString(pat, "regex.pattern");
            CheckString(rep, "regex.replace");
            if (pat.empty()) continue;

            auto compiled = CompileRegexChecked(pat);
            if (!compiled) {
                BKI_LOG_WARN("regex rule rejected: " << pat);
                continue;
            }
            CompiledRegexRule cr;
            cr.pattern = std::move(*compiled);
            cr.replace = utf8_to_wide(rep);
            outRules.push_back(std::move(cr));
        }
    } else if (j.contains("pattern") && j["pattern"].is_string()) {
        std::string pat = j.value("pattern", "");
        std::string rep = j.value("replace", "");
        CheckString(pat, "regex.pattern");
        CheckString(rep, "regex.replace");
        if (pat.empty()) return;

        auto compiled = CompileRegexChecked(pat);
        if (!compiled) {
            BKI_LOG_WARN("regex rule rejected: " << pat);
            return;
        }
        CompiledRegexRule cr;
        cr.pattern = std::move(*compiled);
        cr.replace = utf8_to_wide(rep);
        outRules.push_back(std::move(cr));
    }
}

MenuNodeRule I18nManager::parse_menu_node(const nlohmann::json& j, unsigned depth, bool defaultIsPopup) const {
    if (depth > limits::kMaxNestingDepth) {
        throw LoadError("menu node nesting too deep");
    }

    MenuNodeRule node;
    if (j.contains("type") && j["type"].is_string()) {
        std::string type = j.value("type", "");
        CheckString(type, "menu.type");
        node.is_popup = (type == "popup");
    } else {
        node.is_popup = defaultIsPopup || (!j.contains("cmd_id") && j.contains("src"));
    }
    node.is_regex = (j.value("mode", "") == "regex");
    node.is_ownerdraw = j.value("ownerdraw", false);
    node.cmd_id = static_cast<UINT>(j.value("cmd_id", 0));
    node.cmd_id_end = static_cast<UINT>(j.value("cmd_id_end", 0));
    if (node.cmd_id_end != 0 && node.cmd_id_end < node.cmd_id) {
        throw LoadError("menu cmd_id_end is less than cmd_id");
    }
    node.translate = j.value("translate", false);
    node.src = j.value("src", "");
    node.dst = j.value("dst", "");
    CheckString(node.src, "menu.src");
    CheckString(node.dst, "menu.dst");
    node.raw_dst_utf16 = utf8_to_wide(node.dst);

    if (j.contains("rules") && j["rules"].is_array()) {
        // 含 rules 的节点（动态区间 / ownerdraw / 多规则固定命令）：子项文本按 src 匹配，
        // 统一存原始译文（数据已去 &），并收集各 rule 独立 hint。
        const auto& rulesArray = j["rules"];
        CheckArraySize(rulesArray.size(), "menu.rules");
        for (const auto& r : rulesArray) {
            if (!r.is_object()) continue;
            if (!r.value("translate", true)) continue;
            std::string rSrc = r.value("src", "");
            std::string rDst = r.value("dst", "");
            CheckString(rSrc, "menu.rules.src");
            CheckString(rDst, "menu.rules.dst");
            if (!rSrc.empty() && !rDst.empty()) {
                node.dynamic_texts.push_back(TextPair{rSrc, utf8_to_wide(rDst)});
            }
            // rule 级状态栏提示（可选，成对出现）
            std::string rHintSrc = r.value("hint_src", "");
            std::string rHintDst = r.value("hint_dst", "");
            CheckString(rHintSrc, "menu.rules.hint_src");
            CheckString(rHintDst, "menu.rules.hint_dst");
            if (!rHintSrc.empty() && !rHintDst.empty()) {
                node.rule_hints.push_back(
                    TextPair{rHintSrc, utf8_to_wide(rHintDst)});
            }
        }
    }

    if (node.is_regex) {
        std::string pat = j.value("pattern", node.src);
        CheckString(pat, "menu.pattern");
        if (auto compiled = CompileRegexChecked(pat)) {
            node.regex_pattern = std::move(*compiled);
        } else if (!pat.empty()) {
            BKI_LOG_WARN("menu regex pattern rejected: " << pat);
        }
        node.dst_utf16 = node.raw_dst_utf16;
    } else if (!node.is_dynamic_range() && !node.is_ownerdraw) {
        // 普通固定命令（单节点）：数据已去 &，dst_utf16 仅存无 & 原始译文；
        // access key 无法在加载期得知（数据 src 无 &），延后到运行时用实时文本应用。
        node.dst_utf16 = node.raw_dst_utf16;
    }

    // 解析菜单项的状态栏提示 (hint_src / hint_dst)：不受 node.translate == false 限制
    if (j.value("hint_translate", true)) {
        node.hint_src = j.value("hint_src", "");
        std::string hintDst = j.value("hint_dst", "");
        CheckString(node.hint_src, "menu.hint_src");
        CheckString(hintDst, "menu.hint_dst");
        if (!node.hint_src.empty() && !hintDst.empty()) {
            node.hint_dst_utf16 = utf8_to_wide(hintDst);
        }
    }

    // 兼容在菜单节点上直接附加 tooltip_src / tooltip_dst 的情况：不受 node.translate == false 限制
    if (j.value("tooltip_translate", true)) {
        node.tooltip_src = j.value("tooltip_src", "");
        std::string tipDst = j.value("tooltip_dst", "");
        CheckString(node.tooltip_src, "menu.tooltip_src");
        CheckString(tipDst, "menu.tooltip_dst");
        if (!node.tooltip_src.empty() && !tipDst.empty()) {
            node.tooltip_dst_utf16 = utf8_to_wide(tipDst);
        }
    }

    if (j.contains("children") && j["children"].is_array()) {
        const auto& childArray = j["children"];
        CheckArraySize(childArray.size(), "menu.children");
        for (const auto& child : childArray) {
            if (child.is_object()) {
                node.children.push_back(parse_menu_node(child, depth + 1, node.is_popup));
            }
        }
    }
    return node;
}

ControlRule I18nManager::parse_control_node(const nlohmann::json& j, unsigned depth) const {
    if (depth > limits::kMaxNestingDepth) {
        throw LoadError("control node nesting too deep");
    }

    ControlRule ctrl;
    ctrl.class_name = normalize_class_name(j.value("class_name", ""));
    CheckString(ctrl.class_name, "control.class_name");
    ctrl.ctrl_id = j.value("ctrl_id", 0);
    ctrl.control_subtype = j.value("control_subtype", "");
    ctrl.active_tab_page = j.value("active_tab_page", "");
    CheckString(ctrl.control_subtype, "control.control_subtype");
    CheckString(ctrl.active_tab_page, "control.active_tab_page");
    ctrl.translate = j.value("translate", false);
    ctrl.is_regex = (j.value("mode", "") == "regex");

    if (ctrl.is_regex) {
        parse_regex_rules(j, ctrl.regex_rules);
    } else if (j.contains("rules") && j["rules"].is_array()) {
        // 支持非正则模式下在 rules 数组中直接配置 {"src": "...", "dst": "..."} 或 {"pattern": "...", "replace": "..."}
        const auto& ruleArray = j["rules"];
        CheckArraySize(ruleArray.size(), "control.rules");
        for (const auto& r : ruleArray) {
            if (!r.is_object()) continue;
            if (!r.value("translate", true)) continue;
            std::string src = r.value("src", r.value("pattern", ""));
            std::string dst = r.value("dst", r.value("replace", ""));
            CheckString(src, "control.rules.src");
            CheckString(dst, "control.rules.dst");
            if (!src.empty() && !dst.empty()) {
                ctrl.texts.push_back(TextPair{src, utf8_to_wide(dst)});
            }
        }
    }

    if (j.contains("image_indices") && j["image_indices"].is_array()) {
        const auto& imgArray = j["image_indices"];
        CheckArraySize(imgArray.size(), "control.image_indices");
        for (const auto& idx : imgArray) {
            if (idx.is_number_integer()) {
                ctrl.image_indices.insert(idx.get<int>());
            }
        }
    }

    // SysTreeView32 根级动态图标节点（与 image_indices 并列）；仅做精确 src -> dst 映射
    if (j.contains("root_nodes") && j["root_nodes"].is_array()) {
        const auto& rootArray = j["root_nodes"];
        CheckArraySize(rootArray.size(), "control.root_nodes");
        for (const auto& rn : rootArray) {
            if (!rn.is_object()) continue;
            if (!rn.value("translate", true)) continue;
            std::string src = rn.value("src", "");
            std::string dst = rn.value("dst", "");
            CheckString(src, "control.root_nodes.src");
            CheckString(dst, "control.root_nodes.dst");
            if (!src.empty() && !dst.empty()) {
                ctrl.root_nodes.push_back(TextPair{src, utf8_to_wide(dst)});
            }
        }
    }

    // 单文本与多文本 (texts / nodes)：对话框控件经实测不设 access key，dst 原样使用，
    // 避免字面 '&' 与尾部 '...' 被误剥离（access key 模板仅保留给菜单与工具栏按钮）
    if (j.contains("src") && j["src"].is_string() && j.contains("dst") && j["dst"].is_string()) {
        std::string src = j["src"].get<std::string>();
        std::string dst = j["dst"].get<std::string>();
        CheckString(src, "control.src");
        CheckString(dst, "control.dst");
        if (!src.empty() && !dst.empty()) {
            ctrl.texts.push_back(TextPair{src, utf8_to_wide(dst)});
        }
    }
    for (const char* arrKey : {"texts", "nodes"}) {
        if (j.contains(arrKey) && j[arrKey].is_array()) {
            const auto& textArray = j[arrKey];
            CheckArraySize(textArray.size(), "control.texts");
            for (const auto& t : textArray) {
                if (!t.is_object()) continue;
                if (!t.value("translate", true)) continue;
                std::string src = t.value("src", "");
                std::string dst = t.value("dst", "");
                CheckString(src, "control.texts.src");
                CheckString(dst, "control.texts.dst");
                if (!src.empty() && !dst.empty()) {
                    ctrl.texts.push_back(TextPair{src, utf8_to_wide(dst)});
                }
            }
        }
    }

    // SysHeader32 表头列
    if (j.contains("columns") && j["columns"].is_array()) {
        const auto& colArray = j["columns"];
        CheckArraySize(colArray.size(), "control.columns");
        for (const auto& col : colArray) {
            if (!col.is_object()) continue;
            if (!col.value("translate", false)) continue;
            std::string src = col.value("src", "");
            std::string dst = col.value("dst", "");
            CheckString(src, "control.columns.src");
            CheckString(dst, "control.columns.dst");
            if (!src.empty() && !dst.empty()) {
                ctrl.header_columns[src] = utf8_to_wide(dst);
            }
        }
    }

    // SysTabControl32 页签
    if (j.contains("tabs") && j["tabs"].is_array()) {
        const auto& tabArray = j["tabs"];
        CheckArraySize(tabArray.size(), "control.tabs");
        for (const auto& tab : tabArray) {
            if (!tab.is_object()) continue;
            if (!tab.value("translate", false)) continue;
            std::string src = tab.value("src", "");
            std::string dst = tab.value("dst", "");
            CheckString(src, "control.tabs.src");
            CheckString(dst, "control.tabs.dst");
            if (!src.empty() && !dst.empty()) {
                ctrl.tab_items[src] = utf8_to_wide(dst);
            }
        }
    }

    // ToolbarWindow32 工具栏按钮：忽略 translate: false 对 tooltip_src/tooltip_dst 的限制
    if (j.contains("buttons") && j["buttons"].is_array()) {
        const auto& btnArray = j["buttons"];
        CheckArraySize(btnArray.size(), "control.buttons");
        bool anySurfaceTextTranslate = false;
        for (const auto& btn : btnArray) {
            if (!btn.is_object()) continue;
            ToolbarButtonRule bRule;
            bRule.cmd_id = btn.value("cmd_id", 0);
            bRule.translate = btn.value("translate", false);
            bRule.src = btn.value("src", "");
            CheckString(bRule.src, "control.buttons.src");
            std::string rawBtnDst = btn.value("dst", "");
            CheckString(rawBtnDst, "control.buttons.dst");
            if (!rawBtnDst.empty()) {
                bRule.dst = apply_access_key_format(utf8_to_wide(bRule.src), utf8_to_wide(rawBtnDst));
            }
            if (btn.value("tooltip_translate", true)) {
                bRule.tooltip_src = btn.value("tooltip_src", "");
                std::string tipDst = btn.value("tooltip_dst", "");
                CheckString(bRule.tooltip_src, "control.buttons.tooltip_src");
                CheckString(tipDst, "control.buttons.tooltip_dst");
                bRule.tooltip_dst = utf8_to_wide(tipDst);
            }

            if (bRule.translate && !bRule.dst.empty()) {
                anySurfaceTextTranslate = true;
            }
            if ((bRule.translate && !bRule.dst.empty()) || !bRule.tooltip_dst.empty()) {
                ctrl.toolbar_buttons.push_back(std::move(bRule));
            }
        }
        if (anySurfaceTextTranslate) {
            ctrl.translate = true;
        }
    }

    // ComboBox / SysTreeView32 列表项 (items)
    if (j.contains("items") && j["items"].is_array()) {
        const auto& itemArray = j["items"];
        CheckArraySize(itemArray.size(), "control.items");
        for (const auto& item : itemArray) {
            if (!item.is_object()) continue;
            if (!item.value("translate", true)) continue;
            std::string src = item.value("src", "");
            std::string dst = item.value("dst", "");
            CheckString(src, "control.items.src");
            CheckString(dst, "control.items.dst");
            if (!src.empty() && !dst.empty()) {
                ComboItemRule cRule;
                cRule.index = item.value("index", -1);
                cRule.src = src;
                cRule.dst = utf8_to_wide(dst);
                ctrl.combo_items.push_back(std::move(cRule));
                if (ctrl.class_name == "SysTreeView32") {
                    ctrl.texts.push_back(TextPair{src, utf8_to_wide(dst)});
                }
            }
        }
    }

    // msctls_statusbar32 状态栏分格
    if (j.contains("panes") && j["panes"].is_array()) {
        const auto& paneArray = j["panes"];
        CheckArraySize(paneArray.size(), "control.panes");
        for (const auto& pane : paneArray) {
            if (!pane.is_object()) continue;
            StatusPaneRule pRule;
            pRule.index = pane.value("index", -1);
            pRule.translate = pane.value("translate", false);
            pRule.is_regex = (pane.value("mode", "") == "regex");
            if (pRule.index < 0 || !pRule.translate) continue;

            if (pRule.is_regex) {
                parse_regex_rules(pane, pRule.regex_rules);
            } else {
                pRule.src = pane.value("src", "");
                std::string paneDst = pane.value("dst", "");
                CheckString(pRule.src, "control.panes.src");
                CheckString(paneDst, "control.panes.dst");
                pRule.dst = utf8_to_wide(paneDst);
            }
            if (pane.contains("texts") && pane["texts"].is_array()) {
                const auto& paneTexts = pane["texts"];
                CheckArraySize(paneTexts.size(), "control.panes.texts");
                for (const auto& t : paneTexts) {
                    if (!t.is_object() || !t.value("translate", true)) continue;
                    std::string s = t.value("src", "");
                    std::string d = t.value("dst", "");
                    CheckString(s, "control.panes.texts.src");
                    CheckString(d, "control.panes.texts.dst");
                    if (!s.empty() && !d.empty()) {
                        pRule.texts.push_back(TextPair{s, utf8_to_wide(d)});
                    }
                }
            }
            ctrl.status_panes[pRule.index] = std::move(pRule);
        }
    }

    // 递归解析子控件树
    if (j.contains("children") && j["children"].is_array()) {
        const auto& childArray = j["children"];
        CheckArraySize(childArray.size(), "control.children");
        for (const auto& child : childArray) {
            if (child.is_object()) {
                ctrl.children.push_back(parse_control_node(child, depth + 1));
            }
        }
    }

    return ctrl;
}

void I18nManager::index_menu_tree_recursive(const std::vector<MenuNodeRule>& nodes) {
    for (const auto& node : nodes) {
        // 无论 node.translate 是否为 true，只要存在 hint_src / hint_dst 均建立索引
        if (!node.hint_src.empty() && !node.hint_dst_utf16.empty()) {
            menu_hint_map_[node.hint_src] = node.hint_dst_utf16;
            std::string trimmed = trim_right_copy(node.hint_src);
            if (!trimmed.empty() && trimmed != node.hint_src) {
                menu_hint_map_[trimmed] = node.hint_dst_utf16;
            }
        }

        // 动态区间 rules 内各子项独立的 hint：同样登记进统一 hint 索引，供状态栏第 0 格查询
        for (const auto& hp : node.rule_hints) {
            menu_hint_map_[hp.src] = hp.dst;
            std::string trimmed = trim_right_copy(hp.src);
            if (!trimmed.empty() && trimmed != hp.src) {
                menu_hint_map_[trimmed] = hp.dst;
            }
        }

        // 无论 node.translate 是否为 true，只要存在 tooltip_src / tooltip_dst 均建立索引
        if (!node.tooltip_src.empty() && !node.tooltip_dst_utf16.empty()) {
            tooltip_by_src_[node.tooltip_src] = node.tooltip_dst_utf16;
            tooltip_by_dst_u8_[wide_to_utf8(node.tooltip_dst_utf16)] = node.tooltip_dst_utf16;
            if (node.cmd_id != 0) {
                tooltip_by_cmd_id_[node.cmd_id].push_back(TextPair{node.tooltip_src, node.tooltip_dst_utf16});
            }
        }

        if (node.is_popup) {
            all_popup_nodes_.push_back(&node);
        } else {
            // 区间归属只看 ID 范围，与绘制方式解耦：任何动态区间节点（含 ownerdraw 的 1..100）
            // 都登记进范围表，translate_menu_command 才能按 cmd_id 回退到该区间内的标准文本项
            // （如 statusbar Q 菜单里 cmd_id=42 的 Clear All，MFT_STRING 而非自绘）。
            if (node.is_dynamic_range()) {
                if (node.translate) {
                    dynamic_cmd_ranges_.push_back(&node);
                }
            }
            // ownerdraw 仅决定自绘项的文本获取：区间内自绘项的 rules 额外登记进自绘规则表，
            // 供 GDI 拦截（ExtTextOutW）翻译；标准文本项不依赖此路径，二者可同时存在。
            if (node.is_ownerdraw) {
                if (node.translate && !node.dynamic_texts.empty()) {
                    for (const auto& pair : node.dynamic_texts) {
                        ownerdraw_menu_rules_.push_back(pair);
                    }
                }
            }
            // 固定命令（非区间、非 popup）：按 cmd_id 精确索引
            if (!node.is_dynamic_range() && node.translate && node.cmd_id != 0) {
                const bool hasVariants = !node.dynamic_texts.empty();
                if (hasVariants || !node.raw_dst_utf16.empty()) {
                    MenuCommandRule cmdRule;
                    cmdRule.cmd_id = node.cmd_id;
                    cmdRule.is_regex = node.is_regex;
                    cmdRule.raw_dst_utf16 = node.raw_dst_utf16;
                    cmdRule.regex_pattern = node.regex_pattern;
                    if (hasVariants) {
                        // 多规则固定命令：按去 & 实时文本在 variants 中选 dst
                        cmdRule.variants = node.dynamic_texts;
                    }
                    menu_cmd_map_[node.cmd_id].push_back(std::move(cmdRule));
                }
            }
        }
        if (!node.children.empty()) {
            index_menu_tree_recursive(node.children);
        }
    }
}

void I18nManager::index_control_tree_recursive(const std::vector<ControlRule>& controls) {
    for (const auto& ctrl : controls) {
        if (ctrl.class_name == "ToolbarWindow32") {
            for (const auto& btn : ctrl.toolbar_buttons) {
                // 忽略 btn.translate == false 对 tooltip_dst 的限制
                if (btn.tooltip_dst.empty()) continue;
                if (!btn.tooltip_src.empty()) {
                    tooltip_by_src_[btn.tooltip_src] = btn.tooltip_dst;
                    std::string trimmed = trim_right_copy(btn.tooltip_src);
                    if (!trimmed.empty() && trimmed != btn.tooltip_src) {
                        tooltip_by_src_[trimmed] = btn.tooltip_dst;
                    }
                }
                tooltip_by_dst_u8_[wide_to_utf8(btn.tooltip_dst)] = btn.tooltip_dst;
                if (btn.cmd_id != 0) {
                    tooltip_by_cmd_id_[static_cast<UINT>(btn.cmd_id)].push_back(
                        TextPair{btn.tooltip_src, btn.tooltip_dst}
                    );
                }
            }
        } else if (ctrl.class_name == "AfxWnd" && ctrl.ctrl_id == 12562 && ctrl.translate) {
            for (const auto& pair : ctrl.texts) {
                mail_tab_rules_.push_back(pair);
            }
        }
        if (!ctrl.children.empty()) {
            index_control_tree_recursive(ctrl.children);
        }
    }
}

bool I18nManager::load_locale_json(const std::filesystem::path& jsonPath) {
    std::error_code ec;
    if (!std::filesystem::exists(jsonPath, ec)) {
        return false;
    }
    // 前置文件大小检查：超限直接拒绝，不打开不解析
    if (const auto fileSize = std::filesystem::file_size(jsonPath, ec);
        !ec && fileSize > limits::kMaxJsonFileBytes) {
        BKI_LOG_WARN("reject locale file, size exceeds limit: " << jsonPath.filename().string());
        return false;
    }

    nlohmann::json root;
    try {
        std::ifstream ifs(jsonPath, std::ios::binary);
        if (!ifs) return false;

        // 解析深度限制：超过 kMaxNestingDepth 时抛异常中止解析。
        // 注意：parse_event_t 必须通过 basic_json 的嵌套类型 json::parse_event_t 访问——
        // 3.12 起 nlohmann 顶层不再导出该名，而嵌套类型自 1.0 起稳定，3.11/3.12 通用。
        // 深度回调里 return false 只能丢弃元素、无法使 parse 失败，故超限必须 throw。
        auto depthCallback = [](int depth,
                                nlohmann::json::parse_event_t event,
                                nlohmann::json& /*parsed*/) -> bool {
            if ((event == nlohmann::json::parse_event_t::object_start ||
                 event == nlohmann::json::parse_event_t::array_start) &&
                static_cast<unsigned>(depth) > limits::kMaxNestingDepth) {
                throw std::runtime_error("JSON nesting depth exceeds limit");
            }
            return true;
        };

        root = nlohmann::json::parse(ifs, depthCallback, false);
        if (root.is_discarded() || !root.is_object()) return false;
    } catch (...) {
        return false;
    }

    // 规则构建期任何限额触发（LoadError）或异常都在此收口，拒绝整个语言包
    try {

    windows_.clear();
    window_popup_trees_.clear();
    menu_command_nodes_.clear();
    menu_cmd_map_.clear();
    all_popup_nodes_.clear();
    dynamic_cmd_ranges_.clear();
    ownerdraw_menu_rules_.clear();
    mail_tab_rules_.clear();
    menu_hint_map_.clear();
    tooltip_by_cmd_id_.clear();
    tooltip_by_src_.clear();
    tooltip_by_dst_u8_.clear();
    {
        std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
        top_window_cache_.clear();
        control_rule_cache_.clear();
        tree_item_orig_text_.clear();
    }

    access_key_pattern_ = L"as_is";
    access_key_as_is_ = true;

    // 读取 meta 中的 access_key 配置
    if (root.contains("meta") && root["meta"].is_object()) {
        const auto& jm = root["meta"];
        std::string ak = jm.value("access_key", "");
        CheckString(ak, "meta.access_key");
        if (!ak.empty()) {
            access_key_pattern_ = utf8_to_wide(ak);
            access_key_as_is_ = IsAccessKeyFormatAsIs(access_key_pattern_);
        }
    }

    if (!root.contains("windows") || !root["windows"].is_object()) {
        return false;
    }
    CheckArraySize(root["windows"].size(), "windows");
    CheckArraySize(root["meta"].is_object() ? root["meta"].size() : 0, "meta");

    for (auto it = root["windows"].begin(); it != root["windows"].end(); ++it) {
        const std::string winKey = it.key();
        const auto& jWin = it.value();
        if (!jWin.is_object()) continue;

        CheckString(winKey, "window.key");
        WindowRule wRule;
        wRule.window_key = winKey;
        wRule.class_name = normalize_class_name(jWin.value("class_name", ""));
        wRule.sdk_role = jWin.value("sdk_role", "");
        CheckString(wRule.class_name, "window.class_name");
        CheckString(wRule.sdk_role, "window.sdk_role");

        if (jWin.contains("properties") && jWin["properties"].is_object()) {
            const auto& jProps = jWin["properties"];
            if (jProps.contains("window_title") && jProps["window_title"].is_object()) {
                const auto& jTitle = jProps["window_title"];
                wRule.translate_title = jTitle.value("translate", false);
                wRule.title_is_regex = (jTitle.value("mode", "") == "regex");
                if (wRule.translate_title) {
                    if (wRule.title_is_regex) {
                        parse_regex_rules(jTitle, wRule.title_regex_rules);
                    } else {
                        wRule.title_src = jTitle.value("src", "");
                        std::string titleDst = jTitle.value("dst", "");
                        CheckString(wRule.title_src, "window.title_src");
                        CheckString(titleDst, "window.title_dst");
                        wRule.title_dst = utf8_to_wide(titleDst);
                    }
                }
            }
        }

        if (jWin.contains("children") && jWin["children"].is_array()) {
            const auto& winChildren = jWin["children"];
            CheckArraySize(winChildren.size(), "window.children");
            for (const auto& child : winChildren) {
                if (child.is_object()) {
                    wRule.children.push_back(parse_control_node(child, 1));
                }
            }
        }

        windows_[winKey] = std::move(wRule);
    }

    // ---- 顶层 menus（rev12 已从窗口上移）：popups 三窗口骨架 + commands 全局池 ----
    if (!root.contains("menus") || !root["menus"].is_object()) {
        return false;
    }
    const auto& jMenus = root["menus"];
    CheckArraySize(jMenus.size(), "menus");

    // 1) popups 三桶：每桶 parse 成该窗口的 popup 骨架，存入稳定存储后再索引
    if (jMenus.contains("popups") && jMenus["popups"].is_object()) {
        const auto& jPopups = jMenus["popups"];
        for (auto pIt = jPopups.begin(); pIt != jPopups.end(); ++pIt) {
            if (!pIt.value().is_array()) continue;
            const std::string bucket = pIt.key();
            CheckString(bucket, "menus.popups.bucket");
            CheckArraySize(pIt.value().size(), "menus.popups.tree");
            std::vector<MenuNodeRule> tree;
            for (const auto& item : pIt.value()) {
                if (item.is_object()) {
                    tree.push_back(parse_menu_node(item, 1, true));
                }
            }
            // 先移入稳定存储（节点地址固定），再建立索引，避免指针悬垂
            auto& stored = window_popup_trees_[bucket];
            stored = std::move(tree);
            index_menu_tree_recursive(stored);
        }
    }

    // 2) commands 全局池：parse 进稳定存储后统一索引（固定/区间/自绘/hint/tooltip）
    if (jMenus.contains("commands") && jMenus["commands"].is_array()) {
        const auto& jCommands = jMenus["commands"];
        CheckArraySize(jCommands.size(), "menus.commands");
        for (const auto& item : jCommands) {
            if (item.is_object()) {
                menu_command_nodes_.push_back(parse_menu_node(item, 1, false));
            }
        }
        index_menu_tree_recursive(menu_command_nodes_);
    }

    // 3) 各窗口控件树索引
    for (const auto& [winKey, wRule] : windows_) {
        index_control_tree_recursive(wRule.children);
    }

    return !windows_.empty();
    } catch (const LoadError& e) {
        BKI_LOG_ERROR("locale load rejected by limit: " << e.what());
        return false;
    } catch (const std::exception& e) {
        BKI_LOG_ERROR("locale load rejected: " << e.what());
        return false;
    } catch (...) {
        BKI_LOG_ERROR("locale load rejected: unknown error");
        return false;
    }
}

const std::vector<MenuNodeRule>* I18nManager::find_window_popup_tree(HWND hOwner) const {
    if (!hOwner || !::IsWindow(hOwner)) return nullptr;

    // 菜单所属窗口可能是子窗口，统一取顶级窗口类名做桶映射
    HWND hTop = ::GetAncestor(hOwner, GA_ROOT);
    if (!hTop) hTop = hOwner;

    wchar_t wClass[64]{};
    if (::GetClassNameW(hTop, wClass, static_cast<int>(std::size(wClass))) == 0) {
        return nullptr;
    }
    const std::string normClass = normalize_class_name(wide_to_utf8(wClass));

    std::string bucket;
    if (normClass == "Becky2MainFrame") {
        bucket = "main_frame";
    } else if (normClass == "Becky2ComposeFrame") {
        bucket = "compose";
    } else if (normClass == "Becky2AddressFrame") {
        bucket = "address";
    } else {
        return nullptr;
    }

    auto it = window_popup_trees_.find(bucket);
    if (it == window_popup_trees_.end()) return nullptr;
    return &it->second;
}

std::optional<std::wstring> I18nManager::translate_menu_command(UINT cmdId, std::string_view rawText, bool isUtf8) const {
    if (!active_ || cmdId == 0) return std::nullopt;

    std::string u8Full = isUtf8 ? std::string(rawText) : ansi_to_utf8(rawText);
    std::string baseText = u8Full;
    std::wstring accelSuffix;

    size_t tabPos = u8Full.find('\t');
    if (tabPos != std::string::npos) {
        baseText = u8Full.substr(0, tabPos);
        accelSuffix = utf8_to_wide(u8Full.substr(tabPos));
    }

    // 实时文本（菜单带 &/...）按新契约 normalize 后，再与规范化数据 src 匹配
    const std::wstring wBaseText = utf8_to_wide(baseText);
    const std::string baseNoAmp = wide_to_utf8(NormalizeCaptionW(wBaseText));

    // 1) 精确 cmd_id 索引：固定命令（普通单 dst / 多规则 variants / 正则）
    auto it = menu_cmd_map_.find(cmdId);
    if (it != menu_cmd_map_.end() && !it->second.empty()) {
        for (const auto& rule : it->second) {
            if (rule.is_regex && rule.regex_pattern.has_value()) {
                if (SafeRegexMatch(*rule.regex_pattern, wBaseText)) {
                    std::wstring replaced =
                        SafeRegexReplace(*rule.regex_pattern, wBaseText, rule.raw_dst_utf16);
                    if (replaced == wBaseText) continue;
                    // access key 从实时文本提取（菜单带 & 则插，tooltip 无 & 自然不插）
                    return apply_access_key_format(wBaseText, replaced) + accelSuffix;
                }
            } else if (!rule.variants.empty()) {
                // 多规则固定命令（如 57604/57636）：先按去 & 实时文本选 dst，再套 access key
                for (const auto& v : rule.variants) {
                    if (v.src == baseNoAmp) {
                        return apply_access_key_format(wBaseText, v.dst) + accelSuffix;
                    }
                }
            } else if (!rule.raw_dst_utf16.empty()) {
                // 普通固定命令：按原规范应用 access key
                return apply_access_key_format(wBaseText, rule.raw_dst_utf16) + accelSuffix;
            }
        }
    }

    // 2) 动态 ID 区间回退：cmdId 落入某区间，按去 & 文本在 dynamic_texts 匹配后套 access key。
    //    各区间在设计上互斥，命中某区间即不再考察其他区间。
    for (const auto* rangeNode : dynamic_cmd_ranges_) {
        if (!rangeNode || cmdId < rangeNode->cmd_id || cmdId > rangeNode->cmd_id_end) continue;

        for (const auto& pair : rangeNode->dynamic_texts) {
            if (pair.src == baseNoAmp) {
                return apply_access_key_format(wBaseText, pair.dst) + accelSuffix;
            }
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_ownerdraw_menu_text(std::string_view utf8Text) const {
    if (!active_ || utf8Text.empty() || ownerdraw_menu_rules_.empty()) {
        return std::nullopt;
    }

    for (const auto& pair : ownerdraw_menu_rules_) {
        // 1. 整词精确匹配（用于 From / Subject / Date 下拉菜单的独立左栏标签）
        if (utf8Text == pair.src) {
            return pair.dst;
        }
        // 2. 行首前缀 + 空格匹配（用于 To 下拉菜单中 "To: xxx" / "Cc: xxx" / "Bcc: xxx"
        //    的单行拼接文本，前缀译文后原样保留右栏值）
        if (utf8Text.size() > pair.src.size() &&
            utf8Text.starts_with(pair.src) &&
            utf8Text[pair.src.size()] == ' ') {
            return pair.dst + utf8_to_wide(utf8Text.substr(pair.src.size()));
        }
    }

    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_mail_tab_text(std::string_view rawAnsiText) const {
    if (!active_ || rawAnsiText.empty() || mail_tab_rules_.empty()) {
        return std::nullopt;
    }

    std::string u8Text = ansi_to_utf8(rawAnsiText);
    if (u8Text.empty()) return std::nullopt;

    std::string_view sv = u8Text;

    // 保留首尾空白（如 " Memo " 左右两侧的内边距空格）
    size_t leadLen = 0;
    while (leadLen < sv.size() && (sv[leadLen] == ' ' || sv[leadLen] == '\t')) {
        ++leadLen;
    }
    size_t trailLen = 0;
    while (trailLen < (sv.size() - leadLen) &&
           (sv[sv.size() - 1 - trailLen] == ' ' || sv[sv.size() - 1 - trailLen] == '\t')) {
        ++trailLen;
    }

    std::string_view trimmed = sv.substr(leadLen, sv.size() - leadLen - trailLen);
    if (trimmed.empty()) return std::nullopt;

    std::string_view prefixPart;
    std::string_view corePart = trimmed;

    // 1. 含 '/' 的 MIME 缩写页签（如 "2: >t/plain"、"5: >m/delivery-status"）
    size_t slashPos = trimmed.rfind('/');
    if (slashPos != std::string_view::npos && slashPos + 1 < trimmed.size()) {
        prefixPart = trimmed.substr(0, slashPos + 1);
        corePart   = trimmed.substr(slashPos + 1);
    } else {
        // 2. 含 ':' 的带序号页签（如 "1: Body"、"0: Headers"）
        size_t colonPos = trimmed.find(':');
        if (colonPos != std::string_view::npos && colonPos + 1 < trimmed.size()) {
            size_t afterColon = colonPos + 1;
            while (afterColon < trimmed.size() && trimmed[afterColon] == ' ') {
                ++afterColon;
            }
            prefixPart = trimmed.substr(0, afterColon);
            corePart   = trimmed.substr(afterColon);
        }
    }

    if (corePart.empty()) return std::nullopt;

    for (const auto& pair : mail_tab_rules_) {
        if (corePart == pair.src) {
            std::wstring out;
            if (leadLen > 0) {
                out.append(utf8_to_wide(sv.substr(0, leadLen)));
            }
            if (!prefixPart.empty()) {
                out.append(utf8_to_wide(prefixPart));
            }
            out.append(pair.dst);
            if (trailLen > 0) {
                out.append(utf8_to_wide(sv.substr(sv.size() - trailLen)));
            }
            return out;
        }
    }

    return std::nullopt;
}

const MenuNodeRule* I18nManager::find_popup_menu_node(std::string_view baseUtf8Text,
                                                     const std::vector<MenuNodeRule>* currentLevelNodes) const {
    if (!active_ || baseUtf8Text.empty()) return nullptr;

    std::wstring wBase = utf8_to_wide(baseUtf8Text);
    // 实时文本按新契约完整 normalize（去 &/...、&& 折叠），与数据 src 同口径
    const std::wstring wBaseNorm = NormalizeCaptionW(wBase);

    auto matches_popup = [&](const MenuNodeRule& node) -> bool {
        if (!node.is_popup) return false;
        // 1. 原始精确匹配（未翻译英文、as_is 译文等，实时文本未归一形态）
        if (node.src == baseUtf8Text || node.dst == baseUtf8Text ||
            node.raw_dst_utf16 == wBase) {
            return true;
        }
        // 数据 src/dst 均为规范化文本（normalize 幂等）
        const std::wstring srcNorm = NormalizeCaptionW(utf8_to_wide(node.src));
        const std::wstring dstNorm = NormalizeCaptionW(node.raw_dst_utf16);
        // 2. 归一骨架一致：实时英文带 & 对数据规范化 src，或已翻译的规范化 dst
        if (wBaseNorm == srcNorm) {
            return true;
        }
        if (dstNorm.empty()) return false;
        // 3. 译文形态：纯译文一致，或模板形态 dst(X)（dst 归一骨架 + 括号助记）
        if (wBaseNorm == dstNorm) return true;
        if (wBaseNorm.size() == dstNorm.size() + 3 &&
            wBaseNorm.starts_with(dstNorm) &&
            wBaseNorm[dstNorm.size()] == L'(' &&
            wBaseNorm[wBaseNorm.size() - 1] == L')') {
            return true;
        }
        return false;
    };

    if (currentLevelNodes) {
        for (const auto& node : *currentLevelNodes) {
            if (matches_popup(node)) {
                return &node;
            }
        }
    }

    for (const auto* node : all_popup_nodes_) {
        if (matches_popup(*node)) {
            return node;
        }
    }
    return nullptr;
}

std::optional<std::wstring> I18nManager::translate_tooltip(UINT cmdId,
                                                           std::string_view rawText,
                                                           bool isUtf8,
                                                           bool allowAlreadyTranslated) const {
    if (!active_) return std::nullopt;

    std::string u8Full = isUtf8 ? std::string(rawText) : ansi_to_utf8(rawText);
    std::string u8Trimmed = trim_right_copy(u8Full);

    // 拆分可能附带的快捷键后缀，例如 "Send And Receive (Ctrl+D)" 或 "Send And Receive\tCtrl+D"
    std::string baseText = u8Trimmed;
    std::wstring suffixWide;
    size_t tabPos = u8Trimmed.find('\t');
    if (tabPos != std::string::npos) {
        baseText = trim_right_copy(u8Trimmed.substr(0, tabPos));
        suffixWide = utf8_to_wide(u8Trimmed.substr(tabPos));
    } else if (u8Trimmed.ends_with(')')) {
        size_t parenPos = u8Trimmed.rfind(" (");
        if (parenPos != std::string::npos && parenPos > 0) {
            baseText = trim_right_copy(u8Trimmed.substr(0, parenPos));
            suffixWide = utf8_to_wide(u8Trimmed.substr(parenPos));
        }
    }

    // 1. 优先以按钮命令 ID (cmd_id) 作为唯一索引查找
    if (cmdId != 0) {
        auto it = tooltip_by_cmd_id_.find(cmdId);
        if (it != tooltip_by_cmd_id_.end() && !it->second.empty()) {
            return it->second.front().dst + suffixWide;
        }
        // rev11 起工具栏不再单独存储 buttons/tooltip，tooltip 已按 cmd_id 合并进 commands：
        // 固定命令（含 toolbar_only）用该命令 dst，动态区间按 tooltip 文本匹配区间 rules。
        // baseText 已在本函数开头统一转为 UTF-8 并剥离后缀，故按 UTF-8 传入再补回后缀。
        if (auto cmdTrans = translate_menu_command(cmdId, baseText, true)) {
            return *cmdTrans + suffixWide;
        }
    }

    // 2. 无 cmd_id 时按 Tooltip 原文匹配
    if (!u8Trimmed.empty()) {
        auto srcIt = tooltip_by_src_.find(u8Full);
        if (srcIt != tooltip_by_src_.end()) {
            return srcIt->second;
        }
        srcIt = tooltip_by_src_.find(u8Trimmed);
        if (srcIt != tooltip_by_src_.end()) {
            return srcIt->second;
        }
        if (!suffixWide.empty()) {
            srcIt = tooltip_by_src_.find(baseText);
            if (srcIt != tooltip_by_src_.end()) {
                return srcIt->second + suffixWide;
            }
        }
        if (allowAlreadyTranslated) {
            auto dstIt = tooltip_by_dst_u8_.find(u8Trimmed);
            if (dstIt != tooltip_by_dst_u8_.end()) {
                return dstIt->second;
            }
            if (!suffixWide.empty()) {
                dstIt = tooltip_by_dst_u8_.find(baseText);
                if (dstIt != tooltip_by_dst_u8_.end()) {
                    return dstIt->second + suffixWide;
                }
            }
        }
    }

    return std::nullopt;
}

const WindowRule* I18nManager::find_or_bind_window_rule(HWND hTop) {
    if (!active_ || !hTop || !::IsWindow(hTop)) return nullptr;

    {
        std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
        auto it = top_window_cache_.find(hTop);
        if (it != top_window_cache_.end()) {
            return it->second;
        }
    }

    char rawClass[128]{};
    ::GetClassNameA(hTop, rawClass, sizeof(rawClass));
    std::string normClass = normalize_class_name(rawClass);
    if (normClass.empty()) return nullptr;

    // 1. 直接按归一化类名精确匹配（如 Becky2MainFrame）
    auto directIt = windows_.find(normClass);
    if (directIt != windows_.end()) {
        std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
        top_window_cache_[hTop] = &directIt->second;
        return &directIt->second;
    }

    // 2. 读取窗口标题并按 "<ClassName>:<Title>" 或标题规则匹配
    wchar_t wTitleBuf[512]{};
    ::GetWindowTextW(hTop, wTitleBuf, 512);
    std::wstring wTitle(wTitleBuf);
    std::string u8Title = wide_to_utf8(wTitle);

    std::string compositeKey = normClass + ":" + u8Title;
    auto compIt = windows_.find(compositeKey);
    if (compIt != windows_.end()) {
        std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
        top_window_cache_[hTop] = &compIt->second;
        return &compIt->second;
    }

    for (const auto& [key, wRule] : windows_) {
        if (wRule.class_name != normClass) continue;
        if (!wRule.title_src.empty() && (wRule.title_src == u8Title || wRule.title_dst == wTitle)) {
            std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
            top_window_cache_[hTop] = &wRule;
            return &wRule;
        }
        if (wRule.title_is_regex && !wRule.title_regex_rules.empty()) {
            for (const auto& rx : wRule.title_regex_rules) {
                if (SafeRegexMatch(rx.pattern, wTitle)) {
                    std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
                    top_window_cache_[hTop] = &wRule;
                    return &wRule;
                }
            }
        }
    }

    return nullptr;
}

std::vector<const ControlRule*> I18nManager::find_control_rules(HWND hWnd) {
    if (!active_ || !hWnd || !::IsWindow(hWnd)) return {};

    // 缓存命中即返回（仅可能是非空结果，写入条件见下）
    {
        std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
        auto cached = control_rule_cache_.find(hWnd);
        if (cached != control_rule_cache_.end()) {
            return cached->second;
        }
    }

    std::vector<const ControlRule*> result = compute_control_rules(hWnd);

    // 仅缓存确定的非空结果。空结果可能只是“窗口规则尚未绑定/祖先链未就绪”的临时状态
    // （控件创建期的 SetWindowText 等 detour 会提前触发本函数）；若做负缓存会固化该空结果，
    // 导致后续 WM_INITDIALOG 命中陈旧空值、控件永远不翻译。
    if (!result.empty()) {
        std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
        control_rule_cache_[hWnd] = result;
    }
    return result;
}

std::vector<const ControlRule*> I18nManager::compute_control_rules(HWND hWnd) {
    if (!active_ || !hWnd || !::IsWindow(hWnd)) return {};

    HWND hTop = ::GetAncestor(hWnd, GA_ROOT);
    if (!hTop) hTop = hWnd;
    if (hWnd == hTop) return {};

    const WindowRule* winRule = find_or_bind_window_rule(hTop);
    if (!winRule) return {};

    // 构建从顶级窗口直接子级到目标控件 hWnd 的祖先链
    std::vector<HWND> chain;
    chain.reserve(8);
    HWND curr = hWnd;
    while (curr != nullptr && curr != hTop) {
        chain.push_back(curr);
        curr = ::GetParent(curr);
    }
    if (curr != hTop || chain.empty()) return {};
    std::reverse(chain.begin(), chain.end());

    // 沿 JSON 控件树逐层向下匹配
    std::vector<const ControlRule*> currentCandidates;
    for (const auto& c : winRule->children) {
        currentCandidates.push_back(&c);
    }

    for (size_t depth = 0; depth < chain.size(); ++depth) {
        HWND stepHwnd = chain[depth];
        char rawClass[128]{};
        ::GetClassNameA(stepHwnd, rawClass, sizeof(rawClass));
        std::string stepClass = normalize_class_name(rawClass);
        int stepCtrlId = ::GetDlgCtrlID(stepHwnd);

        std::vector<const ControlRule*> matched;
        for (const auto* node : currentCandidates) {
            if (node->class_name == stepClass && node->ctrl_id == stepCtrlId) {
                matched.push_back(node);
            }
        }

        // 兼容属性页/子对话框动态 ID 回退匹配 (#32770 ctrl_id == 0)
        if (matched.empty() && stepClass == "#32770") {
            for (const auto* node : currentCandidates) {
                if (node->class_name == "#32770" && node->ctrl_id == 0) {
                    matched.push_back(node);
                }
            }
        }

        if (matched.empty()) {
            return {};
        }

        if (depth + 1 == chain.size()) {
            return matched;
        }

        currentCandidates.clear();
        for (const auto* m : matched) {
            for (const auto& child : m->children) {
                currentCandidates.push_back(&child);
            }
        }
        if (currentCandidates.empty()) {
            return {};
        }
    }

    return {};
}

std::optional<std::wstring> I18nManager::translate_control_text(const std::vector<const ControlRule*>& rules,
                                                                std::string_view utf8Text) const {
    if (utf8Text.empty()) return std::nullopt;

    // 实时控件文本经与 inspector normalize_caption_text 完全相同的归一（剥离尾部 "..."、
    // 单 & 助记符删除、"&&" 折叠为字面 &，保留大小写与其余空白）。数据 src 本身即该
    // 归一的输出，故直接相等比较，不引入换行/空白等额外变换，保证两侧算法呼应一致。
    const std::wstring wNorm = NormalizeCaptionW(utf8_to_wide(utf8Text));
    const std::string normInput = wide_to_utf8(wNorm);

    for (const auto* rule : rules) {
        if (!rule || !rule->translate) continue;

        for (const auto& pair : rule->texts) {
            if (pair.src == normInput) {
                return pair.dst;
            }
        }

        if (rule->is_regex && !rule->regex_rules.empty()) {
            for (const auto& rx : rule->regex_rules) {
                if (SafeRegexSearch(rx.pattern, wNorm)) {
                    std::wstring replaced = SafeRegexReplace(rx.pattern, wNorm, rx.replace);
                    // 返回规范化结果；access key 由 ui_translator 按控件类白名单统一应用
                    if (replaced != wNorm) {
                        return replaced;
                    }
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_header_column(const std::vector<const ControlRule*>& rules,
                                                                 std::string_view utf8ColText) const {
    if (utf8ColText.empty()) return std::nullopt;
    std::string key(utf8ColText);
    for (const auto* rule : rules) {
        if (!rule || !rule->translate) continue;
        auto it = rule->header_columns.find(key);
        if (it != rule->header_columns.end()) {
            return it->second;
        }
    }
    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_tab_item(const std::vector<const ControlRule*>& rules,
                                                            std::string_view utf8TabText) const {
    if (utf8TabText.empty()) return std::nullopt;
    std::string key(utf8TabText);
    for (const auto* rule : rules) {
        if (!rule || !rule->translate) continue;
        auto it = rule->tab_items.find(key);
        if (it != rule->tab_items.end()) {
            return it->second;
        }
    }
    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_tree_item(const std::vector<const ControlRule*>& rules,
                                                             int imageIndex,
                                                             std::wstring_view wideItemText) const {
    if (wideItemText.empty()) return std::nullopt;
    std::wstring wInput(wideItemText);
    std::string u8Input = wide_to_utf8(wInput);

    for (const auto* rule : rules) {
        if (!rule || !rule->translate) continue;
        if (!rule->image_indices.empty() && !rule->image_indices.contains(imageIndex)) {
            continue;
        }

        // 1. 优先支持非正则精确匹配（items / texts / rules 中的 src -> dst）
        for (const auto& pair : rule->texts) {
            if (pair.src == u8Input && pair.dst != wInput) {
                return pair.dst;
            }
        }

        // 2. 兼容正则表达式匹配
        for (const auto& rx : rule->regex_rules) {
            if (SafeRegexMatch(rx.pattern, wInput)) {
                std::wstring replaced = SafeRegexReplace(rx.pattern, wInput, rx.replace);
                if (replaced != wInput) {
                    return replaced;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_root_item(
    const std::vector<const ControlRule*>& rules,
    std::wstring_view wideItemText) const {
    if (wideItemText.empty()) return std::nullopt;
    const std::string u8Input = wide_to_utf8(wideItemText);

    for (const auto* rule : rules) {
        if (!rule || !rule->translate) continue;
        for (const auto& pair : rule->root_nodes) {
            if (pair.src == u8Input && pair.dst != wideItemText) {
                return pair.dst;
            }
        }
    }
    return std::nullopt;
}

void I18nManager::record_tree_item_orig_text(HTREEITEM hItem, std::string_view origText) {
    if (!hItem || origText.empty()) return;
    std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
    tree_item_orig_text_[hItem] = std::string(origText);
}

std::optional<std::string> I18nManager::get_tree_item_orig_text(HTREEITEM hItem) const {
    if (!hItem) return std::nullopt;
    std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
    auto it = tree_item_orig_text_.find(hItem);
    if (it != tree_item_orig_text_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void I18nManager::on_window_destroyed(HWND hWnd) {
    if (!hWnd) return;
    std::lock_guard<std::mutex> lock(hwnd_cache_mutex_);
    const bool wasTop = top_window_cache_.find(hWnd) != top_window_cache_.end();
    top_window_cache_.erase(hWnd);
    control_rule_cache_.erase(hWnd);
    if (wasTop) {
        // 顶级窗口销毁：其整棵子树与树节点缓存全部失效。子窗口理论上先于父窗口收到
        // WM_NCDESTROY，这里做防御性全量清理，杜绝句柄复用导致的陈旧映射；
        // 代价仅是其余窗口下次访问时重新计算，无正确性影响。
        control_rule_cache_.clear();
        tree_item_orig_text_.clear();
    }
}

std::optional<std::wstring> I18nManager::translate_status_pane(const std::vector<const ControlRule*>& rules,
                                                               int paneIndex,
                                                               std::wstring_view widePaneText) const {
    if (widePaneText.empty()) return std::nullopt;
    std::wstring wInput(widePaneText);
    std::string u8Input = wide_to_utf8(wInput);

    // 1. 当状态栏更新第 0 格（或 Simple 模式 255）时，优先匹配菜单项的 Hint (hint_src -> hint_dst)
    if ((paneIndex == 0 || paneIndex == 255) && !menu_hint_map_.empty()) {
        auto hintIt = menu_hint_map_.find(u8Input);
        if (hintIt != menu_hint_map_.end()) {
            return hintIt->second;
        }
        std::string trimmedInput = trim_right_copy(u8Input);
        if (!trimmedInput.empty() && trimmedInput != u8Input) {
            auto trimIt = menu_hint_map_.find(trimmedInput);
            if (trimIt != menu_hint_map_.end()) {
                return trimIt->second;
            }
        }
    }

    // 2. 按 msctls_statusbar32 的 panes 规则匹配
    for (const auto* rule : rules) {
        if (!rule || !rule->translate) continue;
        auto it = rule->status_panes.find(paneIndex);
        if (it == rule->status_panes.end()) continue;

        const auto& paneRule = it->second;
        if (!paneRule.translate) continue;

        for (const auto& pair : paneRule.texts) {
            if (pair.src == u8Input) {
                return pair.dst;
            }
        }

        if (paneRule.is_regex) {
            for (const auto& rx : paneRule.regex_rules) {
                if (SafeRegexMatch(rx.pattern, wInput)) {
                    std::wstring replaced = SafeRegexReplace(rx.pattern, wInput, rx.replace);
                    if (replaced != wInput) {
                        return replaced;
                    }
                }
            }
        } else if (!paneRule.src.empty()) {
            if (u8Input == paneRule.src) {
                return paneRule.dst;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_combo_item(const std::vector<const ControlRule*>& rules,
                                                              std::string_view utf8ItemText) const {
    if (utf8ItemText.empty()) return std::nullopt;
    for (const auto* rule : rules) {
        if (!rule || !rule->translate) continue;
        for (const auto& item : rule->combo_items) {
            if (item.src == utf8ItemText) {
                return item.dst;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::wstring> I18nManager::translate_window_title(const WindowRule& winRule,
                                                                std::wstring_view wideTitle) const {
    if (!winRule.translate_title || wideTitle.empty()) return std::nullopt;

    std::wstring wInput(wideTitle);
    if (winRule.title_is_regex) {
        for (const auto& rx : winRule.title_regex_rules) {
            if (SafeRegexMatch(rx.pattern, wInput)) {
                std::wstring replaced = SafeRegexReplace(rx.pattern, wInput, rx.replace);
                if (replaced != wInput) {
                    return replaced;
                }
            }
        }
    } else if (!winRule.title_src.empty() && !winRule.title_dst.empty()) {
        if (wide_to_utf8(wInput) == winRule.title_src) {
            return winRule.title_dst;
        }
    }
    return std::nullopt;
}

} // namespace bki18n