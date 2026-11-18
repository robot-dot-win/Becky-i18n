//  Declares the UiTranslator class responsible for menus, window trees, and per-control translation.
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
#include <vector>
#include <string_view>
#include "i18n_manager.hpp"

namespace bki18n {

// 本次插入上下文：TVM_INSERTITEM 同步调用期间，向插入未完成时的 TVN_GETDISPINFOA 提供
// 根身份/图标（此时 map 尚未记录，且严禁发消息查树），保证节点宽度按译文计算
struct PendingInsertContext {
    bool is_root = false;
    int image_index = -1;
};

class UiTranslator {
public:
    // RAII：在 TVM_INSERTITEMA 调用期间发布本次插入的根身份/图标，供插入期宽度回调读取
    class ScopedPendingInsert {
    public:
        ScopedPendingInsert(bool isRoot, int imageIndex) noexcept;
        ~ScopedPendingInsert();
        ScopedPendingInsert(const ScopedPendingInsert&) = delete;
        ScopedPendingInsert& operator=(const ScopedPendingInsert&) = delete;
    private:
        PendingInsertContext ctx_;
        const PendingInsertContext* prev_;
    };

    static void install_window_hook(HINSTANCE hDll);
    static void uninstall_window_hook();

    // BKC_OnExit 时在窗口仍存在的前提下还原树/父窗口原窗口过程（首选卸载路径）
    static void restore_window_subclasses();

    // 递归翻译 HMENU；levelNodes 为当前窗口 popup 骨架（nullptr 时全局回退）
    static void translate_menu(HMENU hMenu, const std::vector<MenuNodeRule>* levelNodes = nullptr);

    // 按 JSON 控件树翻译指定窗口及其所有受管子控件
    static void translate_window_tree(HWND hWnd);

    // 单独翻译特定类型的控件
    static void translate_single_control(HWND hWnd);
    static void translate_header_control(HWND hHeader, const std::vector<const ControlRule*>& rules);
    static void translate_treeview_control(HWND hTree, const std::vector<const ControlRule*>& rules);
    static void translate_statusbar_control(HWND hStatus, const std::vector<const ControlRule*>& rules);
    static void translate_combobox_control(HWND hCombo, const std::vector<const ControlRule*>& rules);
    static void translate_tab_control(HWND hTab, const std::vector<const ControlRule*>& rules);
    static void translate_toolbar_control(HWND hToolbar, const std::vector<const ControlRule*>& rules);

    // 处理 TTN_NEEDTEXTA / TTN_NEEDTEXTW 悬浮提示通知翻译
    static void translate_tooltip_notify_a(NMTTDISPINFOA* pTTTA);
    static void translate_tooltip_notify_w(NMTTDISPINFOW* pTTTW);

private:
    static void translate_tree_item_recursive(HWND hTree, HTREEITEM hItem, const std::vector<const ControlRule*>& rules);
    static BOOL CALLBACK enum_child_proc(HWND hWndChild, LPARAM lParam);
    static LRESULT CALLBACK call_wnd_ret_proc(int nCode, WPARAM wParam, LPARAM lParam);

    static inline HHOOK s_hWndHook_{nullptr};
};

} // namespace bki18n