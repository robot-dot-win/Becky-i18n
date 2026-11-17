//  Declares the core I18nManager class and the locale data structures for menus, controls, and rules.
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

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <regex>
#include <mutex>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace bki18n {

// 防递归重入哨兵（保护插件内部调用的 Win32 API 不被自身 Hook 二次拦截）
class ScopedHookGuard {
public:
    ScopedHookGuard() noexcept { ++s_depth_; }
    ~ScopedHookGuard() noexcept { --s_depth_; }

    ScopedHookGuard(const ScopedHookGuard&) = delete;
    ScopedHookGuard& operator=(const ScopedHookGuard&) = delete;

    static bool in_hook() noexcept { return s_depth_ > 0; }

private:
    static inline thread_local int s_depth_ = 0;
};

// 语言包元信息（用于 Tools -> UI Language 菜单及 About 窗口展示）
struct LocaleMeta {
    std::string id;             // 例如 "zh_CN"
    std::wstring display_name;  // 例如 L"简体中文 (zh_CN)"
    std::wstring version;       // 例如 L"1.0.0"
    std::wstring author;        // 例如 L"Martin Young"
};

// 正则替换规则
struct CompiledRegexRule {
    std::wregex pattern;
    std::wstring replace;
};

// 精确文本映射对
struct TextPair {
    std::string src;
    std::wstring dst;
};

// 菜单命令映射项（以 cmd_id 为唯一索引，支持普通模式、多规则变体模式与正则模式）
struct MenuCommandRule {
    UINT cmd_id{0};
    bool is_regex{false};
    std::wstring raw_dst_utf16;
    std::optional<std::wregex> regex_pattern;
    // 多规则固定命令（同 cmd_id 不同措辞，如 57604 Save As... / Save To Folder...）：
    // 按去 & 后的实时 src 在 variants 中选 dst；为空表示单 dst，直接用 raw_dst_utf16。
    std::vector<TextPair> variants;
};

// 状态栏分格规则
struct StatusPaneRule {
    int index{0};
    bool translate{false};
    bool is_regex{false};
    std::string src;
    std::wstring dst;
    std::vector<TextPair> texts;
    std::vector<CompiledRegexRule> regex_rules;
};

// ComboBox 列表项规则
struct ComboItemRule {
    int index{-1};
    std::string src;
    std::wstring dst;
};

// 工具栏按钮规则（支持按钮表面文字 src/dst 与悬浮提示 tooltip_src/tooltip_dst）
struct ToolbarButtonRule {
    int cmd_id{0};
    bool translate{false};
    std::string src;
    std::wstring dst;
    std::string tooltip_src;
    std::wstring tooltip_dst;
};

// 菜单树节点规则（支持菜单项文字 src/dst 与状态栏提示 hint_src/hint_dst）
struct MenuNodeRule {
    bool is_popup{false};
    bool is_regex{false};
    bool is_ownerdraw{false};
    UINT cmd_id{0};
    // 动态命令范围：cmd_id_end > cmd_id 时本节点代表一个动态 ID 区间（如字符集/拨号/插件），
    // 具体子项的翻译由 dynamic_texts 按文本匹配；未在表中的文本保持原文。
    UINT cmd_id_end{0};
    bool translate{false};
    std::string src;
    std::string dst;
    std::wstring raw_dst_utf16;
    std::wstring dst_utf16;
    std::optional<std::wregex> regex_pattern;
    std::string hint_src;
    std::wstring hint_dst_utf16;
    std::string tooltip_src;
    std::wstring tooltip_dst_utf16;
    // 动态区间或自绘菜单内的文本映射（对应 JSON 的 rules）；存原始译文，access key 在
    // translate_menu_command 命中后按调用上下文（菜单/tooltip）再决定是否套用
    std::vector<TextPair> dynamic_texts;
    // 动态区间 rules 内各子项独立的状态栏提示（hint_src -> hint_dst）；区间节点顶层不再有 hint
    std::vector<TextPair> rule_hints;
    std::vector<MenuNodeRule> children;

    bool is_dynamic_range() const noexcept { return cmd_id_end > cmd_id; }
};

// 控件树节点规则
struct ControlRule {
    std::string class_name;       // 归一化后的类名
    int ctrl_id{0};
    std::string control_subtype;
    std::string active_tab_page;
    bool translate{false};
    bool is_regex{false};

    // 普通控件（Button / Static / SysTreeView32 / AfxWnd 12562 等）精确匹配或多文本映射
    std::vector<TextPair> texts;

    // 正则匹配规则（动态文本等）
    std::vector<CompiledRegexRule> regex_rules;

    // SysTreeView32 图标索引白名单
    std::unordered_set<int> image_indices;

    // SysTreeView32 根级动态图标节点文本映射（JSON root_nodes，与 image_indices 并列）。
    // 仅对根级（无父节点）且图标不是通用文件夹图标(5)的节点生效，用于 Mail Query
    // 运行时动态分配图像索引、无法纳入静态 image_indices 的特殊节点（如 Query Result）。
    std::vector<TextPair> root_nodes;

    // SysHeader32 列名映射 (src_utf8 -> dst_wide)
    std::unordered_map<std::string, std::wstring> header_columns;

    // SysTabControl32 页签名映射 (src_utf8 -> dst_wide)
    std::unordered_map<std::string, std::wstring> tab_items;

    // ToolbarWindow32 按钮映射
    std::vector<ToolbarButtonRule> toolbar_buttons;

    // ComboBox 选项映射
    std::vector<ComboItemRule> combo_items;

    // msctls_statusbar32 分格映射 (pane_index -> StatusPaneRule)
    std::unordered_map<int, StatusPaneRule> status_panes;

    // 子控件树
    std::vector<ControlRule> children;
};

// 顶级窗口/对话框规则
struct WindowRule {
    std::string window_key;
    std::string class_name;
    std::string sdk_role;

    bool translate_title{false};
    bool title_is_regex{false};
    std::string title_src;
    std::wstring title_dst;
    std::vector<CompiledRegexRule> title_regex_rules;

    // 顶级窗口的子控件树
    std::vector<ControlRule> children;
};

class I18nManager {
public:
    static I18nManager& instance();

    // 初始化：扫描 PlugIns\locale\*.json 并加载当前语言包
    void init(HINSTANCE hDll);

    // 当前是否需要执行界面翻译（当启动语言为非 en_US 且成功加载 JSON 时返回 true）
    bool is_active() const noexcept { return active_; }

    // 启动时生效的语言标识（用于 About 窗口展示）
    const std::string& startup_locale_id() const noexcept { return startup_locale_; }

    // 启动时生效的语言元信息（用于 About 窗口展示）
    const LocaleMeta& active_locale_meta() const noexcept { return active_meta_; }

    // 所有扫描到的可用语言包列表（含 en_US，用于构建 Tools -> UI Language 菜单）
    const std::vector<LocaleMeta>& available_locales() const noexcept { return available_locales_; }

    // 根据 meta.access_key 模板（如 "as_is", "$1(&2)$3", "[&2]$1$3"）格式化译文
    std::wstring apply_access_key_format(std::wstring_view rawSrcWide, std::wstring_view rawDstWide) const;

    // 根据菜单所属窗口句柄，返回该窗口对应的 popup 菜单骨架（rev12 三窗口分桶）；
    // 窗口类无法识别时返回 nullptr（调用方回退到全局 popup 匹配）
    const std::vector<MenuNodeRule>* find_window_popup_tree(HWND hOwner) const;

    // 根据菜单命令 ID 查找翻译（以 cmd_id 为唯一索引，保留原快捷键后缀 \t...）
    std::optional<std::wstring> translate_menu_command(UINT cmdId, std::string_view rawAnsiOrUtf8Text, bool isUtf8 = false) const;

    // 翻译邮件头自绘弹出菜单 (FromToSubjDate_popup_menu, cmd_id: 1..100) 的左栏标签或前缀行。
    // 入参为 UTF-8 明文（由图元文件离屏重放提取），命中返回完整宽字符译文，未命中返回 nullopt。
    std::optional<std::wstring> translate_ownerdraw_menu_text(std::string_view utf8Text) const;

    // 翻译邮件内容面板底部自绘页签条 (AfxWnd, ctrl_id: 12562) 的页签标题，自动保留序号、MIME 前缀与首尾空格
    std::optional<std::wstring> translate_mail_tab_text(std::string_view rawAnsiText) const;

    // 根据弹出菜单标题查找匹配的菜单树节点
    const MenuNodeRule* find_popup_menu_node(std::string_view baseUtf8Text,
                                             const std::vector<MenuNodeRule>* currentLevelNodes = nullptr) const;

    // 查找工具栏按钮或控件的悬浮提示（Tooltip）翻译
    std::optional<std::wstring> translate_tooltip(UINT cmdId,
                                                  std::string_view rawText,
                                                  bool isUtf8 = false,
                                                  bool allowAlreadyTranslated = false) const;

    // 匹配并绑定顶级窗口规则
    const WindowRule* find_or_bind_window_rule(HWND hTop);

    // 按控件树路径精确查找指定 HWND 对应的控件规则集合（带 HWND 缓存，含空结果负缓存）
    std::vector<const ControlRule*> find_control_rules(HWND hWnd);

    // 窗口销毁通知：清除该 HWND 的顶级/控件缓存；顶级窗口销毁时整棵子树缓存一并失效，
    // 防止 HWND/HTREEITEM 句柄被系统复用后命中陈旧映射
    void on_window_destroyed(HWND hWnd);

    // 使用控件规则集合翻译普通控件文本（Button / Static / GroupBox 等）
    std::optional<std::wstring> translate_control_text(const std::vector<const ControlRule*>& rules,
                                                       std::string_view utf8Text) const;

    // 使用控件规则翻译表头列名（SysHeader32）
    std::optional<std::wstring> translate_header_column(const std::vector<const ControlRule*>& rules,
                                                        std::string_view utf8ColText) const;

    // 使用控件规则翻译选项卡标题（SysTabControl32）
    std::optional<std::wstring> translate_tab_item(const std::vector<const ControlRule*>& rules,
                                                   std::string_view utf8TabText) const;

    // 使用控件规则翻译树视图节点（SysTreeView32，含图标索引校验，支持非正则精确匹配与正则匹配）
    std::optional<std::wstring> translate_tree_item(const std::vector<const ControlRule*>& rules,
                                                    int imageIndex,
                                                    std::wstring_view wideItemText) const;

    // 翻译根级动态图标节点（SysTreeView32）：调用方已确认该节点为根级且图像非通用文件夹
    // 图标(5)，本方法仅在 root_nodes 中做精确文本匹配，不做图像白名单与正则处理
    std::optional<std::wstring> translate_root_item(const std::vector<const ControlRule*>& rules,
                                                    std::wstring_view wideItemText) const;

    // 记录与查询树节点原始英文文本（供 TVM_GETITEMA 还原给 B2.exe）
    void record_tree_item_orig_text(HTREEITEM hItem, std::string_view origText);
    std::optional<std::string> get_tree_item_orig_text(HTREEITEM hItem) const;

    // 使用控件规则翻译状态栏分格文本（msctls_statusbar32，含 Pane 0 菜单 Hint 匹配）
    std::optional<std::wstring> translate_status_pane(const std::vector<const ControlRule*>& rules,
                                                      int paneIndex,
                                                      std::wstring_view widePaneText) const;

    // 使用控件规则翻译下拉框选项（ComboBox）
    std::optional<std::wstring> translate_combo_item(const std::vector<const ControlRule*>& rules,
                                                     std::string_view utf8ItemText) const;

    // 翻译顶级窗口标题
    std::optional<std::wstring> translate_window_title(const WindowRule& winRule,
                                                       std::wstring_view wideTitle) const;

    // 字符集与类名工具函数
    static std::string normalize_class_name(std::string_view rawClass);
    // 校验 locale 标识是否匹配白名单 ^[A-Za-z0-9_-]{1,32}$（零分配，防路径穿越）
    static bool is_valid_locale_id(std::string_view id) noexcept;
    static std::string wide_to_utf8(std::wstring_view wstr);
    static std::wstring utf8_to_wide(std::string_view u8str);
    static std::string ansi_to_utf8(std::string_view astr);
    static std::wstring ansi_to_wide(std::string_view astr);
    static std::string wide_to_ansi(std::wstring_view wstr);
    static std::string normalize_newlines(std::string_view s);
    static std::string trim_right_copy(std::string_view s);

private:
    I18nManager() = default;

    void scan_available_locales(const std::filesystem::path& localeDir);
    bool load_locale_json(const std::filesystem::path& jsonPath);

    MenuNodeRule parse_menu_node(const nlohmann::json& j, unsigned depth, bool defaultIsPopup = false) const;
    ControlRule parse_control_node(const nlohmann::json& j, unsigned depth) const;
    // find_control_rules 的实际计算部分（不含缓存）：重建祖先链并沿控件树匹配
    std::vector<const ControlRule*> compute_control_rules(HWND hWnd);
    static void parse_regex_rules(const nlohmann::json& j, std::vector<CompiledRegexRule>& outRules);
    void index_menu_tree_recursive(const std::vector<MenuNodeRule>& nodes);
    void index_control_tree_recursive(const std::vector<ControlRule>& controls);

    bool active_{false};
    std::string startup_locale_{"en_US"};
    LocaleMeta active_meta_{
        "en_US",
        L"English (en_US)",
        L"(Becky! native)",
        L"(Becky! native)"
    };

    // 全局热键格式化配置（如 "as_is", "$1(&2)$3", "[&2]$1$3"）
    bool access_key_as_is_{true};
    std::wstring access_key_pattern_{L"as_is"};

    std::vector<LocaleMeta> available_locales_;
    std::unordered_map<std::string, WindowRule> windows_;

    // rev12：三窗口各自的 popup 菜单骨架（顶层 menus.popups，键为 main_frame/compose/address）
    std::unordered_map<std::string, std::vector<MenuNodeRule>> window_popup_trees_;
    // rev12：全局命令池节点的稳定存储（顶层 menus.commands），dynamic_cmd_ranges_ 指针指向此处
    std::vector<MenuNodeRule> menu_command_nodes_;
    std::unordered_map<UINT, std::vector<MenuCommandRule>> menu_cmd_map_;
    std::vector<const MenuNodeRule*> all_popup_nodes_;
    // 动态命令 ID 区间节点表（数量极少，线性扫描即可）
    std::vector<const MenuNodeRule*> dynamic_cmd_ranges_;
    // 邮件头自绘弹出菜单 (FromToSubjDate_popup_menu) 文本映射表
    std::vector<TextPair> ownerdraw_menu_rules_;
    // 邮件内容面板底部自绘页签条 (AfxWnd, ctrl_id: 12562) 文本映射表
    std::vector<TextPair> mail_tab_rules_;

    // 菜单项状态栏提示 (Menu Hint: hint_src -> hint_dst_wide)
    std::unordered_map<std::string, std::wstring> menu_hint_map_;

    // 工具栏按钮悬浮提示 (Tooltip: cmd_id / tooltip_src -> tooltip_dst_wide)
    std::unordered_map<UINT, std::vector<TextPair>> tooltip_by_cmd_id_;
    std::unordered_map<std::string, std::wstring> tooltip_by_src_;
    std::unordered_map<std::string, std::wstring> tooltip_by_dst_u8_;

    // 已识别顶级窗口句柄缓存及树节点原始英文名缓存
    mutable std::mutex hwnd_cache_mutex_;
    mutable std::unordered_map<HWND, const WindowRule*> top_window_cache_;
    // HWND -> 控件规则集合缓存；仅缓存确定的非空结果（空结果可能是创建期规则未就绪的
    // 临时状态，不做负缓存以免固化）。指针指向 windows_ 内节点，运行期只读、地址稳定
    mutable std::unordered_map<HWND, std::vector<const ControlRule*>> control_rule_cache_;
    std::unordered_map<HTREEITEM, std::string> tree_item_orig_text_;
};

} // namespace bki18n