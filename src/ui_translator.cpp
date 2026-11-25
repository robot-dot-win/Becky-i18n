//  Implements window and control translation, subclassing, and tooltip notification handling.
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
#include "ui_translator.hpp"
#include "api_hooks.hpp"
#include "win32_guard.hpp"
#include "logger.hpp"
#include <commctrl.h>
#include <cstdint>
#include <cstring>

namespace bki18n {

namespace {

WNDPROC g_origTreeParentWndProc = nullptr;
WNDPROC g_origTreeWndProc = nullptr;
HWND g_hSubclassedTree = nullptr;
HWND g_hSubclassedTreeParent = nullptr;
// g_treeFirstPaintPrimed：已完成过一次“确实发生译文替换”的纠正周期（完成态）。
// g_treePrimingInProgress：当前正处于双周期内，用于阻止重入的第二帧再次触发纠正。
// g_treeTranslationObserved：本周期内父窗口过程确实写入过译文，由 TreeParentSubclassProc 置位。
bool g_treeFirstPaintPrimed = false;
bool g_treePrimingInProgress = false;
bool g_treeTranslationObserved = false;

// 本次插入上下文指针：仅在 TVM_INSERTITEM 同步调用期间非空，由 ScopedPendingInsert 管理，
// 支持嵌套插入（构造时保存、析构时恢复外层上下文）
static thread_local const PendingInsertContext* tls_pending_insert = nullptr;

// B2 通用文件夹/账号固定图标索引（账号、用户文件夹、保存的查询容器均使用此图标）。
// 根级动态图标节点（如 Mail Query 的 Query Result）使用运行时分配的高索引，不会是该值。
constexpr int kGenericFolderImage = 5;

// 控件 access-key 白名单判定、助记符 '&' 检测与 caption 最终化（record/as_is/全局模板/尾部标点
// 恢复）已下沉为 I18nManager 的静态方法与 finalize_caption_text，供本文件枚举路径与 api_hooks 的
// A 版 WM_SETTEXT detour 共用同一输出口径，避免两条写入路径再次发生格式化漂移（#131）。

// 文件夹树控件子类：仅负责修复启动首帧的未读数错位。
//
// 根因：B2 在 WM_PAINT 开头先按树内部缓存的节点矩形定位右侧蓝色未读数，默认绘制阶段
// 才触发 TVN_GETDISPINFO 回调并把缓存更新为译文宽度；首帧因此把未读数停在英文位置，
// 随后被更宽的译文标签覆盖。
//
// 修复：第一个 WM_PAINT 周期正常跑完后（缓存已更新为译文宽度），在同一次消息处理中
// 强制第二个同步绘制周期。两个周期都在 WM_PAINT 返回前完成，DWM 仅呈现最终正确画面。
// 仅当本周期确实发生过译文替换才把 g_treeFirstPaintPrimed 置位；若未发生替换（如邮箱
// 尚未就绪），标志保留，后续 WM_PAINT 会重试，避免唯一一次纠正机会被提前消耗。
LRESULT CALLBACK FolderTreeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    try {
        if (uMsg == WM_PAINT && !g_treeFirstPaintPrimed && !g_treePrimingInProgress) {
            g_treePrimingInProgress = true;
            g_treeTranslationObserved = false;

            LRESULT firstRet = ::CallWindowProcA(g_origTreeWndProc, hWnd, uMsg, wParam, lParam);
            // 重入的第二帧因 g_treePrimingInProgress 为真而走常规转发，不会递归触发纠正。
            ::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

            g_treePrimingInProgress = false;
            if (g_treeTranslationObserved) {
                g_treeFirstPaintPrimed = true;
            }
            return firstRet;
        }

        if (uMsg == WM_NCDESTROY) {
            // 树控件自身销毁：其 HTREEITEM 全部失效，精确清空树节点缓存。
            // 注意：其他顶级窗口（Compose/对话框）的 NCDESTROY 不经过本过程，不会误清主树。
            I18nManager::instance().on_folder_tree_destroyed();
            // 窗口销毁前恢复原窗口过程，避免任何指向已卸载代码的悬挂回调（兜底路径）
            WNDPROC orig = g_origTreeWndProc;
            if (orig) {
                ::SetWindowLongPtrA(hWnd, GWLP_WNDPROC,
                                    reinterpret_cast<LONG_PTR>(orig));
            }
            g_hSubclassedTree = nullptr;
            g_origTreeWndProc = nullptr;
            g_treeFirstPaintPrimed = false;
            g_treePrimingInProgress = false;
            g_treeTranslationObserved = false;
            if (orig) {
                return ::CallWindowProcA(orig, hWnd, uMsg, wParam, lParam);
            }
            return 0;
        }

        return ::CallWindowProcA(g_origTreeWndProc, hWnd, uMsg, wParam, lParam);
    } catch (...) {
        // 系统回调边界：异常绝不允许逃逸到 CRT，安全转发原窗口过程
        BKI_LOG_ERROR("unhandled exception in folder tree subclass proc, fallback");
        if (g_origTreeWndProc) {
            return ::CallWindowProcA(g_origTreeWndProc, hWnd, uMsg, wParam, lParam);
        }
        return ::DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }
}

// 文件夹树父窗口 (#32770) 子类：在 TVN_GETDISPINFOA 返回时动态替换系统文件夹显示名称。
// 节点始终保持 LPSTR_TEXTCALLBACK，既不破坏回调状态，又让 B2 自行计算并绘制蓝色未读数。
LRESULT CALLBACK TreeParentSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // ret 声明在 try 外：异常时返回原窗口过程已取得的结果，绝不重复调用造成副作用
    LRESULT ret = 0;
    try {
        ret = ::CallWindowProcA(g_origTreeParentWndProc, hWnd, uMsg, wParam, lParam);

        if (uMsg == WM_NOTIFY &&
            (!ScopedHookGuard::in_hook() || tls_pending_insert != nullptr) &&
            I18nManager::instance().is_active() &&
            is_valid_user_mode_ptr(reinterpret_cast<const void*>(lParam))) {
            // 解引用前先做 SEH 探针：先探测 NMHDR 头部，再探测整个 NMTVDISPINFOA
            if (!is_memory_readable(reinterpret_cast<const void*>(lParam), sizeof(NMHDR)) ||
                !is_memory_readable(reinterpret_cast<const void*>(lParam), sizeof(NMTVDISPINFOA))) {
                return ret;
            }
            const auto* pHdr = reinterpret_cast<const NMHDR*>(lParam);
            // 最窄条件：仅处理来自我们这棵文件夹树的 TVN_GETDISPINFOA，不影响父窗口内其他控件
            if (pHdr->hwndFrom == g_hSubclassedTree && pHdr->code == TVN_GETDISPINFOA) {
                auto* pDispA = reinterpret_cast<NMTVDISPINFOA*>(lParam);
                if ((pDispA->item.mask & TVIF_TEXT) &&
                    is_valid_user_mode_ptr(pDispA->item.pszText) &&
                    is_memory_readable(pDispA->item.pszText, 1) &&
                    pDispA->item.pszText[0] != '\0') {

                    ScopedHookGuard guard;
                    auto rules = I18nManager::instance().find_control_rules(g_hSubclassedTree);
                    if (!rules.empty()) {
                        // 重入红线：本回调内绝不再向这棵树发任何 TVM_*。
                        // 插入期 map 尚未记录，用本次插入上下文；否则查插入边界缓存。
                        // 两者都无则本次放弃翻译，节点稳定后的下次绘制会命中，记录仍在。
                        // 通知自带 TVIF_IMAGE 时这是最可靠的真实图标，先回写缓存
                        // （插入时 mask 常无 TVIF_IMAGE，record 的是 -1），供后续仅
                        // TVIF_TEXT 的回调使用；只操作内部表，不发任何 TVM_*，无重入
                        const bool notifyHasImage = (pDispA->item.mask & TVIF_IMAGE) != 0;
                        if (notifyHasImage && pDispA->item.hItem) {
                            I18nManager::instance().update_tree_item_image(
                                pDispA->item.hItem, pDispA->item.iImage);
                        }

                        int imgIdx = -1;
                        bool isRoot = false;
                        bool haveMeta = false;

                        if (const auto* pending = tls_pending_insert) {
                            // 插入期：通知有图标用通知，否则用插入上下文
                            imgIdx = notifyHasImage ? pDispA->item.iImage : pending->image_index;
                            isRoot = pending->is_root;
                            haveMeta = true;
                        } else if (const auto cached =
                                   I18nManager::instance().get_tree_item_state(pDispA->item.hItem)) {
                            // 正常期：通知有图标用通知，否则用缓存（回写后即为真实图标）
                            imgIdx = notifyHasImage ? pDispA->item.iImage : cached->image_index;
                            isRoot = cached->is_root;
                            haveMeta = true;
                        }

                        if (haveMeta) {
                            const std::wstring wOrig =
                                I18nManager::ansi_to_wide(pDispA->item.pszText);

                            // 根级且图标非通用文件夹(5)：走 root_nodes 精确匹配；
                            // 其余节点走常规 image_indices。账号名(图标5)不翻译。
                            std::optional<std::wstring> trans;
                            if (isRoot && imgIdx != kGenericFolderImage) {
                                trans = I18nManager::instance().translate_root_item(rules, wOrig);
                            }
                            if (!trans) {
                                trans = I18nManager::instance().translate_tree_item(
                                    rules, imgIdx, wOrig);
                            }
                            if (trans) {
                                const std::string transAnsi =
                                    I18nManager::wide_to_ansi(*trans);
                                if (!transAnsi.empty()) {
                                    static thread_local char s_treeTextBufA[256]{};
                                    strncpy_s(s_treeTextBufA, transAnsi.c_str(), _TRUNCATE);

                                    // 仅当调用方提供可写缓冲时写入；cchTextMax==0 表示
                                    // 不接收文本，跳过且不修改 pszText 指针（改了也不会被读取）
                                    if (pDispA->item.cchTextMax > 0) {
                                        strncpy_s(pDispA->item.pszText,
                                                  static_cast<size_t>(pDispA->item.cchTextMax),
                                                  s_treeTextBufA, _TRUNCATE);
                                    }
                                    // 清掉 TVIF_DI_SETITEM，使节点永久保持
                                    // LPSTR_TEXTCALLBACK 回调状态
                                    pDispA->item.mask &= ~TVIF_DI_SETITEM;
                                    // 记录本周期确实写入过译文，供树子类判断纠正是否生效
                                    g_treeTranslationObserved = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (uMsg == WM_NCDESTROY && g_origTreeParentWndProc) {
            ::SetWindowLongPtrA(hWnd, GWLP_WNDPROC,
                                reinterpret_cast<LONG_PTR>(g_origTreeParentWndProc));
            g_hSubclassedTreeParent = nullptr;
            g_origTreeParentWndProc = nullptr;
            g_treePrimingInProgress = false;
            g_treeTranslationObserved = false;
        }
    } catch (...) {
        // 系统回调边界：异常绝不允许逃逸到 CRT；返回原过程已取得的结果
        BKI_LOG_ERROR("unhandled exception in tree parent subclass proc, fallback");
    }

    return ret;
}

// 安装文件夹树及其父窗口子类。幂等：重复调用不会重复挂钩，且仅作用于传入的文件夹树 HWND。
void EnsureTreeParentSubclassed(HWND hTree) {
    if (!hTree || !::IsWindow(hTree)) return;

    HWND hParent = ::GetParent(hTree);
    if (!hParent) return;

    // 更换父窗口前，先把旧父窗口的原过程恢复，避免旧父仍挂着本子类、其原 proc 却被覆盖
    // 而造成错误转发。正常运行中父窗口固定，此分支为防御性处理。
    if (g_hSubclassedTreeParent && g_hSubclassedTreeParent != hParent &&
        ::IsWindow(g_hSubclassedTreeParent) && g_origTreeParentWndProc) {
        auto oldCur = reinterpret_cast<WNDPROC>(
            ::GetWindowLongPtrA(g_hSubclassedTreeParent, GWLP_WNDPROC));
        if (oldCur == TreeParentSubclassProc) {
            ::SetWindowLongPtrA(g_hSubclassedTreeParent, GWLP_WNDPROC,
                                reinterpret_cast<LONG_PTR>(g_origTreeParentWndProc));
        }
        g_hSubclassedTreeParent = nullptr;
        g_origTreeParentWndProc = nullptr;
    }

    if (g_hSubclassedTreeParent != hParent) {
        WNDPROC curParentProc =
            reinterpret_cast<WNDPROC>(::GetWindowLongPtrA(hParent, GWLP_WNDPROC));
        if (curParentProc && curParentProc != TreeParentSubclassProc) {
            // SetWindowLongPtr 返回 0 表示失败；成功才提交全局状态，失败仅记录、保持未挂钩
            LONG_PTR prev = ::SetWindowLongPtrA(
                hParent, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(TreeParentSubclassProc));
            if (prev == 0) {
                BKI_LOG_ERROR("SetWindowLongPtr failed subclassing tree parent");
            } else {
                g_origTreeParentWndProc = curParentProc;
                g_hSubclassedTreeParent = hParent;
            }
        }
    }

    // Becky 是 ANSI 程序、树为 ANSI 窗口，安装与转发统一使用 A 版，与父窗口子类保持一致。
    WNDPROC curTreeProc =
        reinterpret_cast<WNDPROC>(::GetWindowLongPtrA(hTree, GWLP_WNDPROC));
    if (curTreeProc && curTreeProc != FolderTreeSubclassProc) {
        LONG_PTR prev = ::SetWindowLongPtrA(
            hTree, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(FolderTreeSubclassProc));
        if (prev == 0) {
            // 挂钩失败：不更新 g_hSubclassedTree / g_origTreeWndProc，避免后续向无效 proc 转发
            BKI_LOG_ERROR("SetWindowLongPtr failed subclassing folder tree");
        } else {
            g_origTreeWndProc = curTreeProc;
            g_hSubclassedTree = hTree;
            g_treeFirstPaintPrimed = false;
            g_treePrimingInProgress = false;
            g_treeTranslationObserved = false;
        }
    } else if (g_hSubclassedTree != hTree) {
        g_hSubclassedTree = hTree;
    }
}

// 显式还原树与父窗口的原窗口过程并清空状态。在 BKC_OnExit（窗口仍存在）时调用，
// 是首选卸载路径；WM_NCDESTROY 仅作为窗口提前销毁时的兜底。
void RestoreTreeSubclasses() {
    if (g_hSubclassedTreeParent && ::IsWindow(g_hSubclassedTreeParent) &&
        g_origTreeParentWndProc) {
        auto cur = reinterpret_cast<WNDPROC>(
            ::GetWindowLongPtrA(g_hSubclassedTreeParent, GWLP_WNDPROC));
        if (cur == TreeParentSubclassProc) {
            ::SetWindowLongPtrA(g_hSubclassedTreeParent, GWLP_WNDPROC,
                                reinterpret_cast<LONG_PTR>(g_origTreeParentWndProc));
        }
    }
    if (g_hSubclassedTree && ::IsWindow(g_hSubclassedTree) && g_origTreeWndProc) {
        auto cur = reinterpret_cast<WNDPROC>(
            ::GetWindowLongPtrA(g_hSubclassedTree, GWLP_WNDPROC));
        if (cur == FolderTreeSubclassProc) {
            ::SetWindowLongPtrA(g_hSubclassedTree, GWLP_WNDPROC,
                                reinterpret_cast<LONG_PTR>(g_origTreeWndProc));
        }
    }

    g_hSubclassedTree = nullptr;
    g_hSubclassedTreeParent = nullptr;
    g_origTreeParentWndProc = nullptr;
    g_origTreeWndProc = nullptr;
    g_treeFirstPaintPrimed = false;
    g_treePrimingInProgress = false;
    g_treeTranslationObserved = false;
}

} // namespace

UiTranslator::ScopedPendingInsert::ScopedPendingInsert(bool isRoot, int imageIndex) noexcept
    : ctx_{isRoot, imageIndex}, prev_(tls_pending_insert) {
    tls_pending_insert = &ctx_;
}

UiTranslator::ScopedPendingInsert::~ScopedPendingInsert() {
    tls_pending_insert = prev_;
}

void UiTranslator::install_window_hook(HINSTANCE hDll) {
    if (!s_hWndHook_ && I18nManager::instance().is_active()) {
        s_hWndHook_ = ::SetWindowsHookExW(
            WH_CALLWNDPROCRET,
            call_wnd_ret_proc,
            hDll,
            ::GetCurrentThreadId()
        );
    }
}

void UiTranslator::uninstall_window_hook() {
    if (s_hWndHook_) {
        ::UnhookWindowsHookEx(s_hWndHook_);
        s_hWndHook_ = nullptr;
    }
}

void UiTranslator::restore_window_subclasses() {
    RestoreTreeSubclasses();
    ApiHooks::restore_mail_tab_subclass();
}

void UiTranslator::translate_menu(HMENU hMenu, const std::vector<MenuNodeRule>* levelNodes) {
    if (!hMenu || !I18nManager::instance().is_active()) return;

    ScopedHookGuard guard;
    int count = ::GetMenuItemCount(hMenu);
    if (count <= 0) return;

    for (int i = 0; i < count; ++i) {
        wchar_t szBuf[512]{};
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING;
        mii.dwTypeData = szBuf;
        mii.cch = static_cast<UINT>(std::size(szBuf) - 1);

        if (!::GetMenuItemInfoW(hMenu, static_cast<UINT>(i), TRUE, &mii)) {
            continue;
        }
        if (mii.fType & (MFT_SEPARATOR | MFT_OWNERDRAW | MFT_BITMAP)) {
            continue;
        }

        std::wstring wFullText(szBuf, mii.cch);
        std::wstring wBaseText = wFullText;
        std::wstring wAccelSuffix;
        size_t tabPos = wFullText.find(L'\t');
        if (tabPos != std::wstring::npos) {
            wBaseText = wFullText.substr(0, tabPos);
            wAccelSuffix = wFullText.substr(tabPos);
        }

        std::string u8BaseText = I18nManager::wide_to_utf8(wBaseText);

        if (mii.hSubMenu != nullptr) {
            const MenuNodeRule* popupNode =
                I18nManager::instance().find_popup_menu_node(u8BaseText, levelNodes);
            if (popupNode) {
                if (popupNode->translate && !popupNode->raw_dst_utf16.empty()) {
                    // as_is：作者写就的 & 原样保留；否则 access key 从实时文本（带 &）提取
                    std::wstring formattedPopup =
                        popupNode->as_is
                            ? popupNode->raw_dst_utf16
                            : I18nManager::instance().apply_access_key_format(
                                  wBaseText, popupNode->raw_dst_utf16);
                    std::wstring newText = formattedPopup + wAccelSuffix;
                    if (newText != wFullText) {
                        MENUITEMINFOW miiSet{};
                        miiSet.cbSize = sizeof(miiSet);
                        miiSet.fMask = MIIM_STRING;
                        miiSet.dwTypeData = newText.data();
                        ::SetMenuItemInfoW(hMenu, static_cast<UINT>(i), TRUE, &miiSet);
                    }
                }
                translate_menu(mii.hSubMenu, &popupNode->children);
            } else {
                translate_menu(mii.hSubMenu, nullptr);
            }
        } else if (mii.wID != 0 && !wFullText.empty()) {
            std::string u8FullText = I18nManager::wide_to_utf8(wFullText);
            if (auto trans = I18nManager::instance().translate_menu_command(
                    mii.wID, u8FullText, true)) {
                if (*trans != wFullText) {
                    MENUITEMINFOW miiSet{};
                    miiSet.cbSize = sizeof(miiSet);
                    miiSet.fMask = MIIM_STRING;
                    miiSet.dwTypeData = trans->data();
                    ::SetMenuItemInfoW(hMenu, static_cast<UINT>(i), TRUE, &miiSet);
                }
            }
        }
    }
}

void UiTranslator::translate_header_control(HWND hHeader,
                                            const std::vector<const ControlRule*>& rules) {
    int count = static_cast<int>(::SendMessageW(hHeader, HDM_GETITEMCOUNT, 0, 0));
    if (count <= 0) return;

    for (int i = 0; i < count; ++i) {
        wchar_t buf[256]{};
        HDITEMW hdi{};
        hdi.mask = HDI_TEXT;
        hdi.pszText = buf;
        hdi.cchTextMax = static_cast<int>(std::size(buf) - 1);

        if (::SendMessageW(hHeader, HDM_GETITEMW, static_cast<WPARAM>(i),
                           reinterpret_cast<LPARAM>(&hdi))) {
            std::string u8Text = I18nManager::wide_to_utf8(buf);
            if (auto trans = I18nManager::instance().translate_header_column(
                    rules, u8Text)) {
                hdi.pszText = trans->data();
                ::SendMessageW(hHeader, HDM_SETITEMW, static_cast<WPARAM>(i),
                               reinterpret_cast<LPARAM>(&hdi));
            }
        }
    }
}

void UiTranslator::translate_tree_item_recursive(HWND /*hTree*/, HTREEITEM /*hItem*/,
                                                 const std::vector<const ControlRule*>& /*rules*/) {
    // 文件夹树节点统一通过 TVN_GETDISPINFOA 动态回调翻译，
    // 禁止调用 TVM_SETITEMW 写入实体文本，以免破坏 LPSTR_TEXTCALLBACK 状态。
}

void UiTranslator::translate_treeview_control(HWND hTree,
                                              const std::vector<const ControlRule*>& /*rules*/) {
    EnsureTreeParentSubclassed(hTree);
    // 首帧未读数错位由 FolderTreeSubclassProc 的双周期绘制修复，此处仅做常规失效。
    ::InvalidateRect(hTree, nullptr, TRUE);
}

void UiTranslator::translate_statusbar_control(HWND hStatus,
                                               const std::vector<const ControlRule*>& rules) {
    int parts = static_cast<int>(::SendMessageW(hStatus, SB_GETPARTS, 0, 0));
    if (parts <= 0 || parts > 32) return;

    for (int i = 0; i < parts; ++i) {
        LRESULT lenRes = ::SendMessageW(hStatus, SB_GETTEXTLENGTHW,
                                        static_cast<WPARAM>(i), 0);
        int len = LOWORD(lenRes);
        WORD style = HIWORD(lenRes);
        if ((style & SBT_OWNERDRAW) != 0 || len <= 0 || len >= 512) continue;

        std::wstring buf(static_cast<size_t>(len + 2), L'\0');
        ::SendMessageW(hStatus, SB_GETTEXTW, static_cast<WPARAM>(i),
                       reinterpret_cast<LPARAM>(buf.data()));
        buf.resize(wcslen(buf.c_str()));

        if (auto trans = I18nManager::instance().translate_status_pane(
                rules, i, buf)) {
            ::SendMessageW(hStatus, SB_SETTEXTW,
                           static_cast<WPARAM>(i | style),
                           reinterpret_cast<LPARAM>(trans->c_str()));
        }
    }
}

void UiTranslator::translate_combobox_control(HWND hCombo,
                                              const std::vector<const ControlRule*>& rules) {
    int count = static_cast<int>(::SendMessageW(hCombo, CB_GETCOUNT, 0, 0));
    if (count <= 0 || count > 256) return;

    int curSel = static_cast<int>(::SendMessageW(hCombo, CB_GETCURSEL, 0, 0));
    bool modified = false;

    for (int i = 0; i < count; ++i) {
        int len = static_cast<int>(::SendMessageW(hCombo, CB_GETLBTEXTLEN,
                                                  static_cast<WPARAM>(i), 0));
        if (len <= 0 || len > 256) continue;

        std::wstring buf(static_cast<size_t>(len + 2), L'\0');
        ::SendMessageW(hCombo, CB_GETLBTEXT, static_cast<WPARAM>(i),
                       reinterpret_cast<LPARAM>(buf.data()));
        buf.resize(wcslen(buf.c_str()));

        std::string u8Item = I18nManager::wide_to_utf8(buf);
        if (auto trans = I18nManager::instance().translate_combo_item(
                rules, u8Item)) {
            if (*trans != buf) {
                LRESULT itemData = ::SendMessageW(hCombo, CB_GETITEMDATA,
                                                  static_cast<WPARAM>(i), 0);
                ::SendMessageW(hCombo, CB_DELETESTRING,
                               static_cast<WPARAM>(i), 0);
                ::SendMessageW(hCombo, CB_INSERTSTRING,
                               static_cast<WPARAM>(i),
                               reinterpret_cast<LPARAM>(trans->c_str()));
                if (itemData != CB_ERR) {
                    ::SendMessageW(hCombo, CB_SETITEMDATA,
                                   static_cast<WPARAM>(i), itemData);
                }
                modified = true;
            }
        }
    }

    if (modified && curSel != CB_ERR) {
        ::SendMessageW(hCombo, CB_SETCURSEL,
                       static_cast<WPARAM>(curSel), 0);
    }
}

void UiTranslator::translate_tab_control(HWND hTab,
                                         const std::vector<const ControlRule*>& rules) {
    int count = static_cast<int>(::SendMessageW(hTab, TCM_GETITEMCOUNT, 0, 0));
    if (count <= 0) return;

    for (int i = 0; i < count; ++i) {
        wchar_t buf[256]{};
        TCITEMW tci{};
        tci.mask = TCIF_TEXT;
        tci.pszText = buf;
        tci.cchTextMax = static_cast<int>(std::size(buf) - 1);

        if (::SendMessageW(hTab, TCM_GETITEMW, static_cast<WPARAM>(i),
                           reinterpret_cast<LPARAM>(&tci))) {
            std::string u8Text = I18nManager::wide_to_utf8(buf);
            if (auto trans = I18nManager::instance().translate_tab_item(
                    rules, i, u8Text)) {
                tci.pszText = trans->data();
                ::SendMessageW(hTab, TCM_SETITEMW, static_cast<WPARAM>(i),
                               reinterpret_cast<LPARAM>(&tci));
            }
        }
    }
}

void UiTranslator::translate_toolbar_control(HWND hToolbar) {
    constexpr int kMaxButtonCount = 256;
    constexpr int kTextCapacity = 256;

    const int buttonCount = static_cast<int>(
        ::SendMessageW(hToolbar, TB_BUTTONCOUNT, 0, 0));
    if (buttonCount <= 0 || buttonCount > kMaxButtonCount) return;

    I18nManager& manager = I18nManager::instance();

    for (int index = 0; index < buttonCount; ++index) {
        TBBUTTON button{};
        if (::SendMessageW(hToolbar, TB_GETBUTTON,
                           static_cast<WPARAM>(index),
                           reinterpret_cast<LPARAM>(&button)) == 0) {
            continue;
        }
        if ((button.fsStyle & TBSTYLE_SEP) != 0) continue;

        wchar_t textBuf[kTextCapacity]{};
        TBBUTTONINFOW info{};
        info.cbSize = sizeof(info);
        info.dwMask = TBIF_TEXT;
        info.pszText = textBuf;
        info.cchText = kTextCapacity;
        ::SendMessageW(hToolbar, TB_GETBUTTONINFOW,
                       static_cast<WPARAM>(button.idCommand),
                       reinterpret_cast<LPARAM>(&info));
        if (textBuf[0] == L'\0') continue;  // pure-icon button or empty text

        const std::wstring current(textBuf);
        const std::string utf8Text = I18nManager::wide_to_utf8(current);
        auto translated = manager.translate_menu_command(
            static_cast<UINT>(button.idCommand), utf8Text, /*isUtf8=*/true);
        if (!translated || translated->empty() || *translated == current) {
            continue;
        }

        TBBUTTONINFOW setInfo{};
        setInfo.cbSize = sizeof(setInfo);
        setInfo.dwMask = TBIF_TEXT;
        setInfo.pszText = translated->data();
        setInfo.cchText = static_cast<int>(translated->size());
        ::SendMessageW(hToolbar, TB_SETBUTTONINFOW,
                       static_cast<WPARAM>(button.idCommand),
                       reinterpret_cast<LPARAM>(&setInfo));
    }

    ::SendMessageW(hToolbar, TB_AUTOSIZE, 0, 0);
}

void UiTranslator::translate_tooltip_notify_a(NMTTDISPINFOA* pTTTA) {
    if (!pTTTA || !I18nManager::instance().is_active()) return;

    ScopedHookGuard guard;
    UINT cmdId = static_cast<UINT>(pTTTA->hdr.idFrom);
    if ((pTTTA->uFlags & TTF_IDISHWND) && cmdId != 0) {
        cmdId = static_cast<UINT>(::GetDlgCtrlID(
            reinterpret_cast<HWND>(static_cast<uintptr_t>(cmdId))));
    }

    std::string currentAnsi;
    if (is_valid_user_mode_ptr(pTTTA->lpszText)) {
        currentAnsi = pTTTA->lpszText;
    } else if (pTTTA->szText[0] != '\0') {
        currentAnsi = pTTTA->szText;
    } else if (pTTTA->lpszText != nullptr &&
               HIWORD(reinterpret_cast<uintptr_t>(pTTTA->lpszText)) == 0) {
        char resBuf[256]{};
        HINSTANCE hInst = pTTTA->hinst ? pTTTA->hinst : ::GetModuleHandleA(nullptr);
        UINT strId = LOWORD(reinterpret_cast<uintptr_t>(pTTTA->lpszText));
        if (::LoadStringA(hInst, strId, resBuf, sizeof(resBuf) - 1) > 0) {
            currentAnsi = resBuf;
        }
    }

    if (auto transWide = I18nManager::instance().translate_tooltip(
            cmdId, currentAnsi, false, false)) {
        std::string transAnsi = I18nManager::wide_to_ansi(*transWide);
        if (!transAnsi.empty()) {
            static thread_local char s_tipBufA[512]{};
            strncpy_s(s_tipBufA, transAnsi.c_str(), _TRUNCATE);
            strncpy_s(pTTTA->szText, transAnsi.c_str(), _TRUNCATE);
            pTTTA->lpszText = s_tipBufA;
            pTTTA->hinst = nullptr;
        }
    }
}

void UiTranslator::translate_tooltip_notify_w(NMTTDISPINFOW* pTTTW) {
    if (!pTTTW || !I18nManager::instance().is_active()) return;

    ScopedHookGuard guard;
    UINT cmdId = static_cast<UINT>(pTTTW->hdr.idFrom);
    if ((pTTTW->uFlags & TTF_IDISHWND) && cmdId != 0) {
        cmdId = static_cast<UINT>(::GetDlgCtrlID(
            reinterpret_cast<HWND>(static_cast<uintptr_t>(cmdId))));
    }

    std::wstring currentWide;
    if (is_valid_user_mode_ptr(pTTTW->lpszText)) {
        currentWide = pTTTW->lpszText;
    } else if (pTTTW->szText[0] != L'\0') {
        currentWide = pTTTW->szText;
    } else if (pTTTW->lpszText != nullptr &&
               HIWORD(reinterpret_cast<uintptr_t>(pTTTW->lpszText)) == 0) {
        wchar_t resBuf[256]{};
        HINSTANCE hInst = pTTTW->hinst ? pTTTW->hinst : ::GetModuleHandleW(nullptr);
        UINT strId = LOWORD(reinterpret_cast<uintptr_t>(pTTTW->lpszText));
        if (::LoadStringW(hInst, strId, resBuf,
                          static_cast<int>(std::size(resBuf) - 1)) > 0) {
            currentWide = resBuf;
        }
    }

    std::string currentUtf8 = I18nManager::wide_to_utf8(currentWide);
    if (auto transWide = I18nManager::instance().translate_tooltip(
            cmdId, currentUtf8, true, false)) {
        static thread_local wchar_t s_tipBufW[512]{};
        wcsncpy_s(s_tipBufW, transWide->c_str(), _TRUNCATE);
        wcsncpy_s(pTTTW->szText, transWide->c_str(), _TRUNCATE);
        pTTTW->lpszText = s_tipBufW;
        pTTTW->hinst = nullptr;
    }
}

void UiTranslator::translate_single_control(HWND hWnd) {
    if (!hWnd || !::IsWindow(hWnd)) return;

    auto rules = I18nManager::instance().find_control_rules(hWnd);
    if (rules.empty()) return;

    // 工具栏按钮面文字通道：节点 translate=false 是 rev11 起的历史约定（tooltip 等走独立通道），
    // 但按钮面文字经命令 ID 复用菜单译文，与该标志无关；只要存在匹配的工具栏节点即枚举回写。
    if (rules.front()->class_name == "ToolbarWindow32") {
        ScopedHookGuard guard;
        translate_toolbar_control(hWnd);
        return;
    }

    bool anyTranslatable = false;
    for (const auto* r : rules) {
        if (r && r->translate) {
            anyTranslatable = true;
            break;
        }
    }
    if (!anyTranslatable) return;

    ScopedHookGuard guard;
    const std::string& normClass = rules.front()->class_name;

    if (normClass == "SysHeader32") {
        translate_header_control(hWnd, rules);
        return;
    }
    if (normClass == "SysTreeView32") {
        translate_treeview_control(hWnd, rules);
        return;
    }
    if (normClass == "msctls_statusbar32") {
        translate_statusbar_control(hWnd, rules);
        return;
    }
    if (normClass == "ComboBox") {
        translate_combobox_control(hWnd, rules);
        return;
    }
    if (normClass == "SysTabControl32") {
        translate_tab_control(hWnd, rules);
        return;
    }
    if (normClass == "AfxWnd" && rules.front()->ctrl_id == 12562) {
        ApiHooks::subclass_mail_tab_strip(hWnd);
        ::InvalidateRect(hWnd, nullptr, TRUE);
        return;
    }

    // 普通窗口文本控件（Button / Static / GroupBox 等）
    int len = ::GetWindowTextLengthW(hWnd);
    if (len > 0 && len < 4096) {
        std::wstring wText(static_cast<size_t>(len + 1), L'\0');
        int copied = ::GetWindowTextW(hWnd, wText.data(), len + 1);
        wText.resize(static_cast<size_t>(copied));

        const bool supportsKey = I18nManager::supports_caption_access_keys(normClass);
        std::string u8Text = I18nManager::wide_to_utf8(wText);

        // 第一段：用实时文本匹配数据 src。命中说明实时文本是英文原文
        if (auto trans = I18nManager::instance().translate_control_text(
                rules, u8Text)) {
            // record_caption_raw / as_is / 助记符与尾部标点恢复统一在 finalize_caption_text 完成，
            // 与 api_hooks 的 A 版 WM_SETTEXT detour 输出口径保持一致（#131）
            std::wstring finalText = I18nManager::instance().finalize_caption_text(
                hWnd, normClass, wText, *trans);
            if (finalText != wText) {
                ::SetWindowTextW(hWnd, finalText.c_str());
                ::InvalidateRect(hWnd, nullptr, TRUE);
            }
        } else if (supportsKey && !I18nManager::has_mnemonic_amp(wText)) {
            // 第二段：实时文本不命中且无 &（被宿主重设为无 key 文本，可能已是译文）。
            // 用缓存的原始带 & 英文文本重新匹配并提 key，恢复助记符
            if (auto cached = I18nManager::instance().get_caption_raw(hWnd)) {
                std::string u8Cached = I18nManager::wide_to_utf8(*cached);
                if (auto trans2 = I18nManager::instance().translate_control_text(
                        rules, u8Cached)) {
                    // 兜底命中时以缓存的原始带 & 英文文本作为 key/标点提取源；finalize 内的
                    // record 为同键同值幂等写入，不会用当前无 & 的实时文本污染缓存（#131）
                    std::wstring finalText = I18nManager::instance().finalize_caption_text(
                        hWnd, normClass, *cached, *trans2);
                    if (finalText != wText) {
                        ::SetWindowTextW(hWnd, finalText.c_str());
                        ::InvalidateRect(hWnd, nullptr, TRUE);
                    }
                }
            }
        }
    }
}

BOOL CALLBACK UiTranslator::enum_child_proc(HWND hWndChild, LPARAM /*lParam*/) {
    try {
        translate_single_control(hWndChild);
    } catch (...) {
        // 枚举回调边界：异常绝不允许逃逸到系统枚举器，继续枚举其余窗口
        BKI_LOG_ERROR("unhandled exception in enum_child_proc, skip");
    }
    return TRUE;
}

void UiTranslator::translate_window_tree(HWND hWnd) {
    if (!hWnd || !::IsWindow(hWnd) || !I18nManager::instance().is_active()) return;

    HWND hTop = ::GetAncestor(hWnd, GA_ROOT);
    if (!hTop) hTop = hWnd;

    // rev12 下 compose/address 可能没有窗口控件规则（windows_ 仅含主窗口），
    // 但其菜单骨架在独立的 menus.popups 中：菜单翻译不依赖 WindowRule，
    // 故 winRule 为空时仍要继续处理菜单栏，仅跳过标题与控件。
    const WindowRule* winRule =
        I18nManager::instance().find_or_bind_window_rule(hTop);

    ScopedHookGuard guard;

    // 1. 翻译顶级窗口标题（需要窗口规则且配置 translate: true）
    if (winRule && hWnd == hTop && winRule->translate_title) {
        wchar_t wTitle[512]{};
        if (::GetWindowTextW(hTop, wTitle, 512) > 0) {
            if (auto transTitle = I18nManager::instance().translate_window_title(
                    *winRule, wTitle)) {
                ::SetWindowTextW(hTop, transTitle->c_str());
            }
        }
    }

    // 2. 翻译顶级窗口自带菜单栏（如有）：以该窗口自己的 popup 骨架为入口。
    //    独立于 WindowRule——这是地址簿主菜单栏的唯一翻译入口。
    if (hWnd == hTop) {
        if (HMENU hWinMenu = ::GetMenu(hTop)) {
            const auto* popupTree =
                I18nManager::instance().find_window_popup_tree(hTop);
            translate_menu(hWinMenu, popupTree);
        }
    }

    // 3. 遍历并翻译受管子控件（需要窗口规则；无规则时数据端本就未提供控件映射）
    if (!winRule) return;
    if (hWnd != hTop) {
        translate_single_control(hWnd);
    }
    ::EnumChildWindows(hWnd, enum_child_proc, 0);
}

LRESULT CALLBACK UiTranslator::call_wnd_ret_proc(int nCode, WPARAM wParam,
                                                 LPARAM lParam) {
    try {
        if (nCode >= 0 && lParam != 0 && !ScopedHookGuard::in_hook() &&
            I18nManager::instance().is_active()) {
            // CWPRETSTRUCT 由系统提供，解引用前做 SEH 探针
            if (!is_memory_readable(reinterpret_cast<const void*>(lParam),
                                    sizeof(CWPRETSTRUCT))) {
                return ::CallNextHookEx(s_hWndHook_, nCode, wParam, lParam);
            }
            const auto* pMsg = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
            switch (pMsg->message) {
                case WM_CREATE: {
                    char cls[64]{};
                    if (::GetClassNameA(pMsg->hwnd, cls, sizeof(cls)) != 0) {
                        if (_stricmp(cls, "SysTreeView32") == 0) {
                            auto earlyRules =
                                I18nManager::instance().find_control_rules(pMsg->hwnd);
                            if (!earlyRules.empty()) {
                                EnsureTreeParentSubclassed(pMsg->hwnd);
                            }
                        } else if (::GetDlgCtrlID(pMsg->hwnd) == 12562 &&
                                   I18nManager::normalize_class_name(cls) == "AfxWnd") {
                            auto earlyRules =
                                I18nManager::instance().find_control_rules(pMsg->hwnd);
                            if (!earlyRules.empty()) {
                                ApiHooks::subclass_mail_tab_strip(pMsg->hwnd);
                            }
                        }
                    }
                    break;
                }
                case WM_INITDIALOG: {
                    translate_window_tree(pMsg->hwnd);
                    break;
                }
                case WM_SHOWWINDOW: {
                    if (pMsg->wParam != FALSE) {
                        HWND hTop = ::GetAncestor(pMsg->hwnd, GA_ROOT);
                        char cls[64]{};
                        if (hTop) ::GetClassNameA(hTop, cls, sizeof(cls));
                        if (_stricmp(cls, "Becky2MainFrame") != 0) {
                            translate_window_tree(pMsg->hwnd);
                        }
                    }
                    break;
                }
                case WM_INITMENUPOPUP: {
                    HMENU hPopup = reinterpret_cast<HMENU>(pMsg->wParam);
                    if (hPopup && !HIWORD(pMsg->lParam)) {
                        // 以该窗口自己的 popup 骨架为入口（取不到则 translate_menu 全局回退）
                        const auto* popupTree =
                            I18nManager::instance().find_window_popup_tree(pMsg->hwnd);
                        translate_menu(hPopup, popupTree);
                    }
                    break;
                }
                case WM_NOTIFY: {
                    if (is_valid_user_mode_ptr(reinterpret_cast<const void*>(pMsg->lParam)) &&
                        is_memory_readable(reinterpret_cast<const void*>(pMsg->lParam),
                                          sizeof(NMHDR))) {
                        auto* pHdr = reinterpret_cast<NMHDR*>(pMsg->lParam);
                        if (pHdr->code == TCN_SELCHANGE) {
                            HWND hParent = ::GetParent(pHdr->hwndFrom);
                            if (hParent) {
                                translate_window_tree(hParent);
                            }
                        } else if (pHdr->code == TTN_NEEDTEXTA) {
                            translate_tooltip_notify_a(
                                reinterpret_cast<NMTTDISPINFOA*>(pHdr));
                        } else if (pHdr->code == TTN_NEEDTEXTW) {
                            translate_tooltip_notify_w(
                                reinterpret_cast<NMTTDISPINFOW*>(pHdr));
                        }
                    }
                    break;
                }
                case WM_NCDESTROY: {
                    // 窗口销毁最后阶段：统一清除句柄相关缓存，防止句柄复用命中陈旧映射
                    I18nManager::instance().on_window_destroyed(pMsg->hwnd);
                    break;
                }
                default:
                    break;
            }
        }
    } catch (...) {
        // 钩子回调边界：异常绝不允许逃逸到 CRT；钩子链仍在下方正常继续
        BKI_LOG_ERROR("unhandled exception in call_wnd_ret_proc, skip");
    }
    return ::CallNextHookEx(s_hWndHook_, nCode, wParam, lParam);
}

} // namespace bki18n
