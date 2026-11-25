//  Implements Win32 API detours for text, menu, and window-procedure interception, plus the mail tab strip subclass.
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
#include "api_hooks.hpp"
#include "i18n_manager.hpp"
#include "ui_translator.hpp"
#include "logger.hpp"
#include "win32_guard.hpp"

#include <MinHook.h>
#include <commctrl.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace bki18n {

namespace {

using PFN_SetWindowTextA   = BOOL(WINAPI*)(HWND, LPCSTR);
using PFN_SetDlgItemTextA  = BOOL(WINAPI*)(HWND, int, LPCSTR);
using PFN_SendMessageA     = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
using PFN_CallWindowProcA  = LRESULT(WINAPI*)(WNDPROC, HWND, UINT, WPARAM, LPARAM);
using PFN_DefWindowProcA   = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
using PFN_TrackPopupMenu   = BOOL(WINAPI*)(HMENU, UINT, int, int, int, HWND, const RECT*);
using PFN_TrackPopupMenuEx = BOOL(WINAPI*)(HMENU, UINT, int, int, HWND, LPTPMPARAMS);
using PFN_ModifyMenuA      = BOOL(WINAPI*)(HMENU, UINT, UINT, UINT_PTR, LPCSTR);
using PFN_InsertMenuA      = BOOL(WINAPI*)(HMENU, UINT, UINT, UINT_PTR, LPCSTR);
using PFN_AppendMenuA      = BOOL(WINAPI*)(HMENU, UINT, UINT_PTR, LPCSTR);

// W 版与 SendDlgItemMessage：用于覆盖 Becky 切换邮件时对只读 Edit 115 的多通道更新
using PFN_SetWindowTextW      = BOOL(WINAPI*)(HWND, LPCWSTR);
using PFN_SendMessageW        = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
using PFN_SendDlgItemMessageA = LRESULT(WINAPI*)(HWND, int, UINT, WPARAM, LPARAM);
using PFN_SendDlgItemMessageW = LRESULT(WINAPI*)(HWND, int, UINT, WPARAM, LPARAM);

// 自绘弹出菜单 (FromToSubjDate_popup_menu) 真实绘制路径：宿主对 ASCII 文本以 ExtTextOutW 输出
using PFN_ExtTextOutW = BOOL(WINAPI*)(HDC, int, int, UINT, const RECT*, LPCWSTR, UINT, const INT*);
// 邮件内容面板底部自绘页签条 (AfxWnd, ctrl_id: 12562) 测量与绘制路径：宿主统一调用 DrawTextA
using PFN_DrawTextA   = int (WINAPI*)(HDC, LPCSTR, int, LPRECT, UINT);

PFN_SetWindowTextA   fpOrigSetWindowTextA   = nullptr;
PFN_SetDlgItemTextA  fpOrigSetDlgItemTextA  = nullptr;
PFN_SendMessageA     fpOrigSendMessageA     = nullptr;
PFN_CallWindowProcA  fpOrigCallWindowProcA  = nullptr;
PFN_DefWindowProcA   fpOrigDefWindowProcA   = nullptr;
PFN_TrackPopupMenu   fpOrigTrackPopupMenu   = nullptr;
PFN_TrackPopupMenuEx fpOrigTrackPopupMenuEx = nullptr;
PFN_ModifyMenuA      fpOrigModifyMenuA      = nullptr;
PFN_InsertMenuA      fpOrigInsertMenuA      = nullptr;
PFN_AppendMenuA      fpOrigAppendMenuA      = nullptr;

PFN_SetWindowTextW      fpOrigSetWindowTextW      = nullptr;
PFN_SendMessageW        fpOrigSendMessageW        = nullptr;
PFN_SendDlgItemMessageA fpOrigSendDlgItemMessageA = nullptr;
PFN_SendDlgItemMessageW fpOrigSendDlgItemMessageW = nullptr;

PFN_ExtTextOutW fpOrigExtTextOutW = nullptr;
PFN_DrawTextA   fpOrigDrawTextA   = nullptr;

// ===== ETO_GLYPH_INDEX 自绘弹出菜单（如 Compose 优先级菜单）=====
// 弹出菜单模态期间的线程局部上下文：记录顶级窗口与其绑定的 WindowRule，并缓存当前字体的
// glyph->wchar 反查表，供 ExtTextOutW 把字形索引还原为明文后再做数据驱动的正则替换。
struct GlyphMenuContext {
    bool active{false};
    HWND top{nullptr};
    const WindowRule* window_rule{nullptr};
    HFONT cached_font{nullptr};
    std::unordered_map<WORD, wchar_t> glyph_to_char;
};
thread_local GlyphMenuContext g_glyphMenuCtx;

// 用当前 hdc 字体建立（或按 HFONT 复用）glyph->wchar 反向表，覆盖 ASCII 与 Latin-1。
const std::unordered_map<WORD, wchar_t>* GetGlyphReverseMap(HDC hdc) {
    if (!hdc) return nullptr;
    auto* hFont = static_cast<HFONT>(::GetCurrentObject(hdc, OBJ_FONT));
    if (!hFont) return nullptr;
    if (g_glyphMenuCtx.cached_font == hFont && !g_glyphMenuCtx.glyph_to_char.empty()) {
        return &g_glyphMenuCtx.glyph_to_char;
    }

    std::unordered_map<WORD, wchar_t> g2c;
    wchar_t single[2] = {0, 0};
    const auto collect = [&](wchar_t wc) {
        single[0] = wc;
        WORD glyph = 0;
        if (::GetGlyphIndicesW(hdc, single, 1, &glyph, 0) != GDI_ERROR &&
            glyph != 0xFFFF) {
            g2c.emplace(glyph, wc);
        }
    };
    for (wchar_t wc = 0x20; wc <= 0x7E; ++wc) collect(wc);
    for (wchar_t wc = 0xA0; wc <= 0xFF; ++wc) collect(wc);

    g_glyphMenuCtx.cached_font = hFont;
    g_glyphMenuCtx.glyph_to_char = std::move(g2c);
    return &g_glyphMenuCtx.glyph_to_char;
}

// 把 glyph 索引序列反解为明文；任一 glyph 无法映射则返回 nullopt（调用方原样放行）。
std::optional<std::wstring> DecodeGlyphText(HDC hdc, LPCWSTR lpString, UINT count) {
    if (count == 0 || !lpString) return std::nullopt;
    const auto* g2c = GetGlyphReverseMap(hdc);
    if (!g2c) return std::nullopt;

    std::wstring plain;
    plain.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        auto it = g2c->find(static_cast<WORD>(lpString[i]));
        if (it == g2c->end()) return std::nullopt;
        plain.push_back(it->second);
    }
    return plain;
}
// ===== END glyph menu context =====

struct HeaderItemTranslation {
    std::wstring text;
    bool is_full_line{false};  // true: To 菜单单行拼接模式；false: From/Subject/Date 左右分栏模式
};

// 自绘弹出菜单 (FromToSubjDate_popup_menu) 严格作用域开关（仅在 Static 1006..1009 处理 ODT_MENU 时置位）
thread_local bool g_inOwnerDrawDraw        = false;
thread_local int  g_ownerDrawRcLeft        = 0;
thread_local UINT g_currentDrawItemId      = 0;      // 当前真实 WM_DRAWITEM 正在绘制的 itemID
thread_local bool g_suppressRemainingDraws = false;  // 单行拼接模式已完成整行译文输出后，吞除同项后续分段调用
thread_local WNDPROC g_origHeaderStaticProc = nullptr;
// 预扫描结果：自绘菜单项 itemID -> 译文与单行/分栏模式信息
thread_local std::unordered_map<UINT, HeaderItemTranslation> g_headerItemTranslations;

// 邮件内容面板底部自绘页签条 (AfxWnd, ctrl_id: 12562) 子类化状态与绘制作用域开关
HWND g_hSubclassedMailTab = nullptr;
WNDPROC g_origMailTabProc = nullptr;
thread_local bool g_inMailTabPaint   = false;
thread_local bool g_inDrawTextDetour = false;

// rev14：gdi_patterns 自绘文本的作用域窗口缓存。窗口 DC 命中含 gdi_patterns 的控件规则时记录，
// 供同一绘制周期随后出现的双缓冲内存 DC（WindowFromDC=nullptr）复用规则；使用前以 IsWindow 惰性校验。
HWND g_hGdiPatternWnd = nullptr;

// 左栏文本判定阈值（相对 rcItem.left）：自绘菜单左栏标签 x 偏移约为 16，右栏数据远大于此
constexpr int kOwnerDrawLeftColumnLimit = 24;

// 判断菜单属主窗口是否为邮件头面板上的 From/Date/Subject/To 静态标签控件 (ctrl_id: 1006..1009)
inline bool IsMailHeaderStaticOwner(HWND hWnd) noexcept {
    if (!hWnd || !::IsWindow(hWnd)) return false;
    int ctrlId = ::GetDlgCtrlID(hWnd);
    if (ctrlId < 1006 || ctrlId > 1009) return false;
    char cls[32]{};
    if (::GetClassNameA(hWnd, cls, sizeof(cls)) <= 0) return false;
    return _stricmp(cls, "Static") == 0;
}

// 校验字符串是否符合邮件底部页签条 (ctrl_id: 12562) 的特征格式：
// 1. 带序号与冒号空格前缀，且后续无空格（如 "1: Body", "2: >t/plain", "5: >m/delivery-status", "0: Headers"）
// 2. 首尾各带一个空格的独立页签名（如 " Memo "）
inline bool IsMailTabLabelPattern(std::string_view sv) noexcept {
    if (sv.size() < 3 || sv.size() > 64) return false;

    // 情况 1：形如 " Memo "（首尾为空格，内部为无空格单词）
    if (sv.front() == ' ' && sv.back() == ' ') {
        std::string_view inner = sv.substr(1, sv.size() - 2);
        if (inner.empty()) return false;
        for (char ch : inner) {
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') return false;
        }
        return true;
    }

    // 情况 2：形如 "1: Body" 或 "2: >t/plain"（以数字 + ": " 开头，且冒号空格之后不含空白字符）
    size_t i = 0;
    while (i < sv.size() && sv[i] >= '0' && sv[i] <= '9') {
        ++i;
    }
    if (i == 0 || i + 2 >= sv.size()) return false;
    if (sv[i] != ':' || sv[i + 1] != ' ') return false;

    std::string_view rest = sv.substr(i + 2);
    for (char ch : rest) {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') return false;
    }
    return true;
}

// rev14：gdi_patterns 自绘文本的热路径廉价预筛。仅用于避免对全局高频 DrawTextA 调用逐一查找
// 控件规则；是否真正翻译仍由 JSON gdi_patterns 决定，且各作用域规则的强特征互不重叠（双重保险）。
// 覆盖：①任务 pane 三行固定起始词；②邮件列表 Subject 头部的 {...} 任务类型小标签（独立绘制调用）。
inline bool IsGdiPatternCandidate(std::string_view sv) noexcept {
    if (sv.empty()) return false;
    switch (sv.front()) {
        case 'T': return sv.starts_with("Task:");
        case 'D': return sv.starts_with("Deadline:");
        case 'C': return sv.starts_with("Comment:");
        case '{': return sv.starts_with("{Need") || sv.starts_with("{Follow");
        default:  return false;
    }
}

// 邮件头 Static 控件在弹出下拉菜单期间的临时子类化窗口过程
LRESULT CALLBACK HeaderStaticMenuSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc = g_origHeaderStaticProc;
    if (!origProc) {
        return fpOrigDefWindowProcA
            ? fpOrigDefWindowProcA(hWnd, uMsg, wParam, lParam)
            : ::DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_DRAWITEM &&
        is_valid_user_mode_ptr(lParam) &&
        is_memory_readable(reinterpret_cast<const void*>(lParam), sizeof(DRAWITEMSTRUCT))) {
        const auto* lpDis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (lpDis->CtlType == ODT_MENU && lpDis->itemID >= 1 && lpDis->itemID <= 100) {
            const bool prevDraw     = g_inOwnerDrawDraw;
            const int  prevLeft     = g_ownerDrawRcLeft;
            const UINT prevItem     = g_currentDrawItemId;
            const bool prevSuppress = g_suppressRemainingDraws;
            g_inOwnerDrawDraw        = true;
            g_ownerDrawRcLeft       = lpDis->rcItem.left;
            g_currentDrawItemId      = static_cast<UINT>(lpDis->itemID);
            g_suppressRemainingDraws = false;

            // 保存当前菜单项绘制前的 DC 裁剪区，供单行拼接模式封锁残余绘制后还原
            HRGN hSavedClip = ::CreateRectRgn(0, 0, 0, 0);
            int clipState = (lpDis->hDC && hSavedClip) ? ::GetClipRgn(lpDis->hDC, hSavedClip) : -1;

            LRESULT ret = fpOrigCallWindowProcA
                ? fpOrigCallWindowProcA(origProc, hWnd, uMsg, wParam, lParam)
                : ::CallWindowProcA(origProc, hWnd, uMsg, wParam, lParam);

            if (g_suppressRemainingDraws && lpDis->hDC && clipState != -1) {
                ::SelectClipRgn(lpDis->hDC, (clipState == 1) ? hSavedClip : nullptr);
            }
            if (hSavedClip) {
                ::DeleteObject(hSavedClip);
            }

            g_suppressRemainingDraws = prevSuppress;
            g_currentDrawItemId      = prevItem;
            g_ownerDrawRcLeft       = prevLeft;
            g_inOwnerDrawDraw        = prevDraw;
            return ret;
        }
    }

    return fpOrigCallWindowProcA
        ? fpOrigCallWindowProcA(origProc, hWnd, uMsg, wParam, lParam)
        : ::CallWindowProcA(origProc, hWnd, uMsg, wParam, lParam);
}

// 邮件内容面板底部页签条 (AfxWnd, ctrl_id: 12562) 子类化窗口过程：
// 在该控件处理任何消息（含 WM_PAINT、鼠标点击切换页签 WM_LBUTTONDOWN/UP 等）期间全程开启 g_inMailTabPaint。
LRESULT CALLBACK MailTabStripSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc = g_origMailTabProc;
    if (!origProc) {
        return fpOrigDefWindowProcA
            ? fpOrigDefWindowProcA(hWnd, uMsg, wParam, lParam)
            : ::DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_NCDESTROY) {
        ::SetWindowLongPtrA(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(origProc));
        g_hSubclassedMailTab = nullptr;
        g_origMailTabProc = nullptr;
        return fpOrigCallWindowProcA
            ? fpOrigCallWindowProcA(origProc, hWnd, uMsg, wParam, lParam)
            : ::CallWindowProcA(origProc, hWnd, uMsg, wParam, lParam);
    }

    const bool prevPaint = g_inMailTabPaint;
    g_inMailTabPaint = true;
    LRESULT ret = 0;
    try {
        ret = fpOrigCallWindowProcA
            ? fpOrigCallWindowProcA(origProc, hWnd, uMsg, wParam, lParam)
            : ::CallWindowProcA(origProc, hWnd, uMsg, wParam, lParam);
    } catch (...) {
        g_inMailTabPaint = prevPaint;
        BKI_LOG_ERROR("unhandled exception in MailTabStripSubclassProc, fallback");
        return 0;
    }
    g_inMailTabPaint = prevPaint;

    // 当鼠标点击切换页签后，触发一次非擦除重绘，确保页签栏始终以翻译状态刷新
    if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP) {
        ::InvalidateRect(hWnd, nullptr, FALSE);
    }

    return ret;
}

// RAII 临时子类化哨兵：仅在 TrackPopupMenu 模态展示期间挂载，菜单关闭立即还原
class ScopedHeaderMenuSubclass {
public:
    explicit ScopedHeaderMenuSubclass(HWND hWnd) noexcept {
        if (IsMailHeaderStaticOwner(hWnd) && g_origHeaderStaticProc == nullptr) {
            WNDPROC curProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtrA(hWnd, GWLP_WNDPROC));
            if (curProc && curProc != HeaderStaticMenuSubclassProc) {
                hWnd_ = hWnd;
                g_origHeaderStaticProc = curProc;
                ::SetWindowLongPtrA(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HeaderStaticMenuSubclassProc));
            }
        }
    }

    ~ScopedHeaderMenuSubclass() noexcept {
        if (hWnd_ && g_origHeaderStaticProc) {
            if (::IsWindow(hWnd_)) {
                WNDPROC curProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtrA(hWnd_, GWLP_WNDPROC));
                if (curProc == HeaderStaticMenuSubclassProc) {
                    ::SetWindowLongPtrA(hWnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origHeaderStaticProc));
                }
            }
            g_origHeaderStaticProc = nullptr;
        }
    }

    ScopedHeaderMenuSubclass(const ScopedHeaderMenuSubclass&) = delete;
    ScopedHeaderMenuSubclass& operator=(const ScopedHeaderMenuSubclass&) = delete;

private:
    HWND hWnd_{nullptr};
};

// 图元文件离屏重放时记录的一次文本绘制（坐标相对重放用 rcItem，文本为宽字符明文）
struct MetaTextRecord {
    int x{0};
    int y{0};
    std::wstring text;
};

// EnumEnhMetaFile 回调：把图元中的文本记录（W/A）转成 MetaTextRecord 追加到 lParam 指向的 vector。
int CALLBACK EnumMetaTextProc(HDC, HANDLETABLE*, const ENHMETARECORD* lpmr, int, LPARAM lParam) {
    auto* pOut = reinterpret_cast<std::vector<MetaTextRecord>*>(lParam);
    if (lpmr->iType == EMR_EXTTEXTOUTW) {
        const auto* peto = reinterpret_cast<const EMREXTTEXTOUTW*>(lpmr);
        if (peto->emrtext.nChars > 0 && peto->emrtext.offString > 0 &&
            peto->emrtext.offString + peto->emrtext.nChars * sizeof(wchar_t) <= lpmr->nSize) {
            const auto* wptr = reinterpret_cast<const wchar_t*>(
                reinterpret_cast<const BYTE*>(lpmr) + peto->emrtext.offString);
            pOut->push_back(MetaTextRecord{
                static_cast<int>(peto->emrtext.ptlReference.x),
                static_cast<int>(peto->emrtext.ptlReference.y),
                std::wstring(wptr, peto->emrtext.nChars)});
        }
    } else if (lpmr->iType == EMR_EXTTEXTOUTA) {
        const auto* peto = reinterpret_cast<const EMREXTTEXTOUTA*>(lpmr);
        if (peto->emrtext.nChars > 0 && peto->emrtext.offString > 0 &&
            peto->emrtext.offString + peto->emrtext.nChars <= lpmr->nSize) {
            const auto* aptr = reinterpret_cast<const char*>(
                reinterpret_cast<const BYTE*>(lpmr) + peto->emrtext.offString);
            const int cch = static_cast<int>(peto->emrtext.nChars);
            int wlen = ::MultiByteToWideChar(CP_ACP, 0, aptr, cch, nullptr, 0);
            if (wlen > 0) {
                std::wstring ws(static_cast<size_t>(wlen), L'\0');
                ::MultiByteToWideChar(CP_ACP, 0, aptr, cch, ws.data(), wlen);
                pOut->push_back(MetaTextRecord{
                    static_cast<int>(peto->emrtext.ptlReference.x),
                    static_cast<int>(peto->emrtext.ptlReference.y),
                    std::move(ws)});
            }
        }
    }
    return 1;  // 继续枚举
}

// 用增强图元文件 DC 离屏重放单个自绘菜单项，枚举其全部文本绘制记录。
bool ReplayOwnerdrawToMetafile(HWND hOwner, HMENU hMenu, const MENUITEMINFOW& mii,
                               std::vector<MetaTextRecord>& outRecords) {
    HDC hRefDC = ::GetDC(hOwner);
    const RECT rcRef{0, 0, 15000, 1000};
    HDC hMetaDC = ::CreateEnhMetaFileW(hRefDC, nullptr, &rcRef, nullptr);
    if (!hMetaDC) {
        if (hRefDC) ::ReleaseDC(hOwner, hRefDC);
        return false;
    }

    DRAWITEMSTRUCT dis{};
    dis.CtlType    = ODT_MENU;
    dis.CtlID      = 0;
    dis.itemID     = mii.wID;
    dis.itemAction = ODA_DRAWENTIRE;
    dis.itemState  = 0;
    dis.hwndItem   = reinterpret_cast<HWND>(hMenu);
    dis.hDC        = hMetaDC;
    dis.rcItem     = {0, 0, 600, 24};
    dis.itemData   = mii.dwItemData;

    ::SendMessageA(hOwner, WM_DRAWITEM, 0, reinterpret_cast<LPARAM>(&dis));

    HENHMETAFILE hEmf = ::CloseEnhMetaFile(hMetaDC);
    if (hRefDC) ::ReleaseDC(hOwner, hRefDC);
    if (!hEmf) return false;

    ::EnumEnhMetaFile(nullptr, hEmf, &EnumMetaTextProc, &outRecords, nullptr);

    ::DeleteEnhMetaFile(hEmf);
    return true;
}

// 从图元重放记录中取左栏（x<=阈值）第一条非空白文本，返回其 UTF-8 明文。
std::string ExtractLeftColumnPlainText(const std::vector<MetaTextRecord>& records) {
    for (const auto& rec : records) {
        if (rec.x > kOwnerDrawLeftColumnLimit) continue;
        std::wstring_view sv = rec.text;
        while (!sv.empty() && (sv.front() == L' ' || sv.front() == L'\t')) sv.remove_prefix(1);
        while (!sv.empty() && (sv.back() == L' ' || sv.back() == L'\t')) sv.remove_suffix(1);
        if (sv.empty()) continue;
        return I18nManager::wide_to_utf8(sv);
    }
    return {};
}

// 在真实 TrackPopupMenu 之前，离屏预扫描菜单内全部自绘项，建立 itemID -> 左栏完整译文。
void PrescanHeaderOwnerdrawMenu(HMENU hMenu, HWND hOwner) {
    g_headerItemTranslations.clear();
    if (!hMenu || !IsMailHeaderStaticOwner(hOwner)) return;

    const int count = ::GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA;
        if (!::GetMenuItemInfoW(hMenu, static_cast<UINT>(i), TRUE, &mii)) continue;
        if ((mii.fType & MFT_OWNERDRAW) == 0) continue;
        if (mii.wID < 1 || mii.wID > 100) continue;

        std::vector<MetaTextRecord> records;
        if (!ReplayOwnerdrawToMetafile(hOwner, hMenu, mii, records)) continue;

        const std::string plain = ExtractLeftColumnPlainText(records);
        if (plain.empty()) continue;
        if (auto translated = I18nManager::instance().translate_ownerdraw_menu_text(plain)) {
            bool hasRightColumn = false;
            for (const auto& rec : records) {
                if (rec.x > kOwnerDrawLeftColumnLimit) {
                    hasRightColumn = true;
                    break;
                }
            }
            g_headerItemTranslations.emplace(
                mii.wID,
                HeaderItemTranslation{std::move(*translated), !hasRightColumn}
            );
        }
    }
}

// 检查窗口是否为系统 Tooltip 控件 (tooltips_class32)
inline bool IsTooltipControl(HWND hWnd) noexcept {
    if (!hWnd || !::IsWindow(hWnd)) return false;
    char cls[32]{};
    if (::GetClassNameA(hWnd, cls, sizeof(cls)) <= 0) return false;
    return _stricmp(cls, "tooltips_class32") == 0;
}

// SendMessageA 入口快速白名单
inline bool IsInterceptedMessageA(UINT msg) noexcept {
    switch (msg) {
        case WM_NOTIFY:
        case WM_SETTEXT:
        case SB_SETTEXTA:
        case TTM_ADDTOOLA:
        case TTM_UPDATETIPTEXTA:
        case TTM_SETTOOLINFOA:
        case HDM_INSERTITEMA:
        case HDM_SETITEMA:
        case LVM_INSERTCOLUMNA:
        case LVM_SETCOLUMNA:
        case TVM_INSERTITEMA:
        case TVM_DELETEITEM:
        case TVM_GETITEMA:
        case TVM_SETITEMA:
        case TCM_INSERTITEMA:
        case TCM_SETITEMA:
        case EM_REPLACESEL:
            return true;
        default:
            return false;
    }
}

// SendMessageW / SendDlgItemMessageW 入口白名单
inline bool IsInterceptedMessageW(UINT msg) noexcept {
    switch (msg) {
        case WM_SETTEXT:
        case EM_REPLACESEL:
        case SB_SETTEXTW:
            return true;
        default:
            return false;
    }
}

// 窄范围判定：仅当目标窗口命中受管的 Edit 控件且存在 translate=true 的规则时才处理。
inline bool IsTranslatableEdit(const std::vector<const ControlRule*>& rules) noexcept {
    if (rules.empty() || rules.front()->class_name != "Edit") return false;
    for (const auto* r : rules) {
        if (r && r->translate) return true;
    }
    return false;
}

// 统一处理发往受管控件的动态文本消息
std::optional<LRESULT> TryTranslateControlMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (!hWnd || !is_valid_user_mode_ptr(lParam) || ScopedHookGuard::in_hook() || !I18nManager::instance().is_active()) {
        return std::nullopt;
    }

    switch (Msg) {
        case WM_SETTEXT: {
            HWND hTop = ::GetAncestor(hWnd, GA_ROOT);
            if (hWnd == hTop) {
                const auto* winRule = I18nManager::instance().find_or_bind_window_rule(hTop);
                if (!winRule || !winRule->translate_title) break;

                const char* lpStr = reinterpret_cast<const char*>(lParam);
                if (!lpStr || lpStr[0] == '\0') break;

                std::wstring wTitle = I18nManager::ansi_to_wide(lpStr);
                if (auto trans = I18nManager::instance().translate_window_title(*winRule, wTitle)) {
                    ScopedHookGuard guard;
                    return ::SendMessageW(hWnd, WM_SETTEXT, wParam, reinterpret_cast<LPARAM>(trans->c_str()));
                }
                break;
            }

            // 必须先确认 hWnd 是 JSON 中受管的控件，再去读取 lParam 字符串
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (rules.empty()) break;

            const char* lpStr = reinterpret_cast<const char*>(lParam);
            if (!lpStr || lpStr[0] == '\0') break;

            // 若目标控件为状态栏（MFC CFrameWnd::OnSetMessageString 会对状态栏调用 SetWindowTextA 更新第 0 格菜单 Hint）
            if (rules.front()->class_name == "msctls_statusbar32") {
                std::wstring wPaneText = I18nManager::ansi_to_wide(lpStr);
                if (auto trans = I18nManager::instance().translate_status_pane(rules, 0, wPaneText)) {
                    ScopedHookGuard guard;
                    LRESULT res = fpOrigSendMessageA
                        ? fpOrigSendMessageA(hWnd, WM_SETTEXT, wParam, lParam)
                        : ::DefWindowProcA(hWnd, WM_SETTEXT, wParam, lParam);
                    LRESULT lenRes = ::SendMessageW(hWnd, SB_GETTEXTLENGTHW, 0, 0);
                    WORD style = HIWORD(lenRes);
                    if ((style & SBT_OWNERDRAW) == 0) {
                        ::SendMessageW(hWnd, SB_SETTEXTW, static_cast<WPARAM>(0 | style), reinterpret_cast<LPARAM>(trans->c_str()));
                    }
                    return res;
                }
                break;
            }

            std::string u8Text = I18nManager::ansi_to_utf8(lpStr);
            if (auto trans = I18nManager::instance().translate_control_text(rules, u8Text)) {
                // #131：caption 类（Button/Static）必须与 ui 枚举路径共用 finalize_caption_text，
                // 否则这里直接写裸 trans->text 会丢失助记符模板与尾部 ":"/"..." 恢复（Win11 工作机
                // Compose 抄送/密送 Button 被本端点最后定格为无 key 裸译文，即此缺陷）。
                // 其余控件类（Edit/ComboBox/...）finalize 原样返回译文，保持旧行为不变。
                const std::string& cls = rules.front()->class_name;
                std::wstring finalText;
                if (I18nManager::supports_caption_access_keys(cls)) {
                    // 必须用未归一的 ANSI 实时原文转宽：保留 '&' 与尾部标点供 finalize 提取
                    std::wstring wRawSource = I18nManager::ansi_to_wide(lpStr);
                    finalText = I18nManager::instance().finalize_caption_text(
                        hWnd, cls, wRawSource, *trans);
                } else {
                    finalText = trans->text;
                }

                ScopedHookGuard guard;
                return ::SendMessageW(hWnd, WM_SETTEXT, wParam,
                                      reinterpret_cast<LPARAM>(finalText.c_str()));
            }
            break;
        }

        case EM_REPLACESEL: {
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (!IsTranslatableEdit(rules)) break;

            const char* lpStr = reinterpret_cast<const char*>(lParam);
            if (!lpStr || lpStr[0] == '\0') break;

            std::string u8Text = I18nManager::ansi_to_utf8(lpStr);
            if (auto trans = I18nManager::instance().translate_control_text(rules, u8Text)) {
                ScopedHookGuard guard;
                return ::SendMessageW(hWnd, EM_REPLACESEL, wParam,
                                      reinterpret_cast<LPARAM>(trans->text.c_str()));
            }
            break;
        }

        case SB_SETTEXTA: {
            if ((wParam & SBT_OWNERDRAW) != 0) break;

            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (rules.empty() || rules.front()->class_name != "msctls_statusbar32") break;

            const char* lpStr = reinterpret_cast<const char*>(lParam);
            if (!lpStr || lpStr[0] == '\0') break;

            int paneIndex = static_cast<int>(LOWORD(wParam) & 0xFF);
            std::wstring wPaneText = I18nManager::ansi_to_wide(lpStr);
            if (auto trans = I18nManager::instance().translate_status_pane(rules, paneIndex, wPaneText)) {
                ScopedHookGuard guard;
                return ::SendMessageW(hWnd, SB_SETTEXTW, wParam, reinterpret_cast<LPARAM>(trans->c_str()));
            }
            break;
        }

        case TTM_ADDTOOLA:
        case TTM_UPDATETIPTEXTA:
        case TTM_SETTOOLINFOA: {
            if (!IsTooltipControl(hWnd)) break;

            const auto* pTiA = reinterpret_cast<const TTTOOLINFOA*>(lParam);
            if (!is_memory_readable(pTiA, sizeof(UINT))) break;
            if (pTiA->cbSize < TTTOOLINFOA_V1_SIZE) break;
            if (!is_memory_readable(pTiA, pTiA->cbSize)) break;
            if (!is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(pTiA->lpszText))) break;

            UINT cmdId = static_cast<UINT>(pTiA->uId);
            if ((pTiA->uFlags & TTF_IDISHWND) && cmdId != 0) {
                cmdId = static_cast<UINT>(::GetDlgCtrlID(reinterpret_cast<HWND>(static_cast<uintptr_t>(cmdId))));
            }

            if (auto trans = I18nManager::instance().translate_tooltip(cmdId, pTiA->lpszText, false, true)) {
                ScopedHookGuard guard;
                TTTOOLINFOW tiW{};
                size_t copyBytes = (pTiA->cbSize <= sizeof(TTTOOLINFOW)) ? pTiA->cbSize : sizeof(TTTOOLINFOW);
                std::memcpy(&tiW, pTiA, copyBytes);
                tiW.lpszText = const_cast<LPWSTR>(trans->c_str());

                UINT msgW = TTM_UPDATETIPTEXTW;
                if (Msg == TTM_ADDTOOLA) msgW = TTM_ADDTOOLW;
                else if (Msg == TTM_SETTOOLINFOA) msgW = TTM_SETTOOLINFOW;

                return ::SendMessageW(hWnd, msgW, wParam, reinterpret_cast<LPARAM>(&tiW));
            }
            break;
        }

        case HDM_INSERTITEMA:
        case HDM_SETITEMA: {
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (rules.empty() || rules.front()->class_name != "SysHeader32") break;

            const auto* pHdA = reinterpret_cast<const HDITEMA*>(lParam);
            if (!is_memory_readable(pHdA, sizeof(HDITEMA))) break;
            if ((pHdA->mask & HDI_TEXT) == 0 || !is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(pHdA->pszText))) break;

            std::string u8Col = I18nManager::ansi_to_utf8(pHdA->pszText);
            if (auto trans = I18nManager::instance().translate_header_column(rules, u8Col)) {
                ScopedHookGuard guard;
                HDITEMW hdW{};
                std::memcpy(&hdW, pHdA, sizeof(HDITEMA));
                hdW.pszText = const_cast<LPWSTR>(trans->c_str());
                UINT msgW = (Msg == HDM_INSERTITEMA) ? HDM_INSERTITEMW : HDM_SETITEMW;
                return ::SendMessageW(hWnd, msgW, wParam, reinterpret_cast<LPARAM>(&hdW));
            }
            break;
        }

        case LVM_INSERTCOLUMNA:
        case LVM_SETCOLUMNA: {
            ScopedHookGuard guard;
            HWND hHeader = ::GetDlgItem(hWnd, 0);
            if (!hHeader) break;

            auto rules = I18nManager::instance().find_control_rules(hHeader);
            if (rules.empty() || rules.front()->class_name != "SysHeader32") break;

            const auto* pColA = reinterpret_cast<const LVCOLUMNA*>(lParam);
            if (!is_memory_readable(pColA, sizeof(LVCOLUMNA))) break;
            if ((pColA->mask & LVCF_TEXT) == 0 || !is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(pColA->pszText))) break;

            std::string u8Col = I18nManager::ansi_to_utf8(pColA->pszText);
            if (auto trans = I18nManager::instance().translate_header_column(rules, u8Col)) {
                LVCOLUMNW colW{};
                std::memcpy(&colW, pColA, sizeof(LVCOLUMNA));
                colW.pszText = const_cast<LPWSTR>(trans->c_str());
                UINT msgW = (Msg == LVM_INSERTCOLUMNA) ? LVM_INSERTCOLUMNW : LVM_SETCOLUMNW;
                return ::SendMessageW(hWnd, msgW, wParam, reinterpret_cast<LPARAM>(&colW));
            }
            break;
        }

        case TVM_INSERTITEMA: {
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (rules.empty() || rules.front()->class_name != "SysTreeView32") break;

            const auto* pInsA = reinterpret_cast<const TVINSERTSTRUCTA*>(lParam);
            if (!is_memory_readable(pInsA, sizeof(TVINSERTSTRUCTA))) break;

            const bool isRoot = (pInsA->hParent == nullptr || pInsA->hParent == TVI_ROOT);
            const int insertImg = (pInsA->item.mask & TVIF_IMAGE) ? pInsA->item.iImage : -1;
            const bool isCallback = (pInsA->item.pszText == LPSTR_TEXTCALLBACK);

            // 仅实体文本节点（非 LPSTR_TEXTCALLBACK）需要在插入时翻译；
            // callback 节点保持原样，翻译统一留给 TVN_GETDISPINFOA。
            if (!isCallback && (pInsA->item.mask & TVIF_TEXT) &&
                is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(pInsA->item.pszText))) {

                std::wstring wItemText = I18nManager::ansi_to_wide(pInsA->item.pszText);
                if (auto trans = I18nManager::instance().translate_tree_item(rules, insertImg, wItemText)) {
                    ScopedHookGuard guard;
                    const std::string transAnsi = I18nManager::wide_to_ansi(*trans);
                    // 只复制结构体本身（TVINSERTSTRUCTA，不含 TVITEMEX 扩展尾部），
                    // pszText 指向本次同步调用期间有效的局部缓冲，仍走 ANSI 插入，
                    // 杜绝整结构 memcpy 改 W 与 TVITEMEX 垃圾导致的重入/AV。
                    TVINSERTSTRUCTA insLocal = *pInsA;
                    insLocal.item.pszText = const_cast<LPSTR>(transAnsi.c_str());
                    LRESULT hNewItem = fpOrigSendMessageA(hWnd, TVM_INSERTITEMA, wParam,
                                                          reinterpret_cast<LPARAM>(&insLocal));
                    if (hNewItem) {
                        auto* hNew = reinterpret_cast<HTREEITEM>(hNewItem);
                        I18nManager::instance().record_tree_item_orig_text(hNew, pInsA->item.pszText);
                        I18nManager::instance().record_tree_item_state(hNew, isRoot, insertImg);
                    }
                    return hNewItem;
                }
            }

            // callback 节点或无需翻译：发布本次插入上下文，使插入未完成时的宽度回调能安全
            // 翻译（不能用 ScopedHookGuard 屏蔽，否则宽度按英文算导致译文截断）；返回后再
            // 记录 map，保证 B2 拿到句柄前缓存已就绪。
            {
                UiTranslator::ScopedPendingInsert pending(isRoot, insertImg);
                LRESULT hNewItem = fpOrigSendMessageA(hWnd, TVM_INSERTITEMA, wParam, lParam);
                if (hNewItem) {
                    I18nManager::instance().record_tree_item_state(
                        reinterpret_cast<HTREEITEM>(hNewItem), isRoot, insertImg);
                }
                return hNewItem;
            }
        }

        case TVM_DELETEITEM: {
            // lParam 是 TVITEM(EX)，首字段布局兼容，只需 hItem；删除前移除该句柄全部缓存。
            const auto* pDel = reinterpret_cast<const TVITEMA*>(lParam);
            if (is_memory_readable(pDel, sizeof(TVITEMA)) && pDel->hItem) {
                I18nManager::instance().forget_tree_item(pDel->hItem);
            }
            break;
        }

        case TVM_SETITEMA: {
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (rules.empty() || rules.front()->class_name != "SysTreeView32") break;

            const auto* pItemA = reinterpret_cast<const TVITEMA*>(lParam);
            if (!is_memory_readable(pItemA, sizeof(TVITEMA))) break;
            if ((pItemA->mask & TVIF_TEXT) == 0 || !is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(pItemA->pszText))) break;

            ScopedHookGuard guard;
            int imgIdx = pItemA->iImage;
            if ((pItemA->mask & TVIF_IMAGE) == 0 && pItemA->hItem != nullptr) {
                // 只查插入边界缓存，绝不向树重发 TVM_GETITEMW（重入风险）
                if (auto st = I18nManager::instance().get_tree_item_state(pItemA->hItem)) {
                    imgIdx = st->image_index;
                }
            }

            std::wstring wItemText = I18nManager::ansi_to_wide(pItemA->pszText);
            if (auto trans = I18nManager::instance().translate_tree_item(rules, imgIdx, wItemText)) {
                if (pItemA->hItem) {
                    I18nManager::instance().record_tree_item_orig_text(pItemA->hItem, pItemA->pszText);
                }
                // 保持 ANSI 路径：复制 TVITEMA 本身，pszText 指向本次调用有效的局部缓冲
                const std::string transAnsi = I18nManager::wide_to_ansi(*trans);
                TVITEMA itemLocal = *pItemA;
                itemLocal.pszText = const_cast<LPSTR>(transAnsi.c_str());
                return fpOrigSendMessageA(hWnd, TVM_SETITEMA, wParam,
                                          reinterpret_cast<LPARAM>(&itemLocal));
            }
            break;
        }

        case TVM_GETITEMA: {
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (rules.empty() || rules.front()->class_name != "SysTreeView32") break;

            auto* pItemA = reinterpret_cast<TVITEMA*>(lParam);
            if (!is_memory_readable(pItemA, sizeof(TVITEMA))) break;
            if ((pItemA->mask & TVIF_TEXT) == 0 || !pItemA->hItem ||
                !is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(pItemA->pszText)) ||
                pItemA->cchTextMax <= 0) {
                break;
            }

            if (auto origText = I18nManager::instance().get_tree_item_orig_text(pItemA->hItem)) {
                ScopedHookGuard guard;
                LRESULT ret = fpOrigSendMessageA
                    ? fpOrigSendMessageA(hWnd, Msg, wParam, lParam)
                    : ::DefWindowProcA(hWnd, Msg, wParam, lParam);
                if (ret) {
                    strncpy_s(pItemA->pszText, static_cast<size_t>(pItemA->cchTextMax), origText->c_str(), _TRUNCATE);
                }
                return ret;
            }
            break;
        }

        case TCM_INSERTITEMA:
        case TCM_SETITEMA: {
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            if (rules.empty() || rules.front()->class_name != "SysTabControl32") break;

            const auto* pTcA = reinterpret_cast<const TCITEMA*>(lParam);
            if (!is_memory_readable(pTcA, sizeof(TCITEMA))) break;
            if ((pTcA->mask & TCIF_TEXT) == 0 || !is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(pTcA->pszText))) break;

            std::string u8Tab = I18nManager::ansi_to_utf8(pTcA->pszText);
            if (auto trans = I18nManager::instance().translate_tab_item(
                    rules, static_cast<int>(wParam), u8Tab)) {
                ScopedHookGuard guard;
                TCITEMW tcW{};
                std::memcpy(&tcW, pTcA, sizeof(TCITEMA));
                tcW.pszText = const_cast<LPWSTR>(trans->c_str());
                UINT msgW = (Msg == TCM_INSERTITEMA) ? TCM_INSERTITEMW : TCM_SETITEMW;
                return ::SendMessageW(hWnd, msgW, wParam, reinterpret_cast<LPARAM>(&tcW));
            }
            break;
        }

        default:
            break;
    }

    return std::nullopt;
}

// 统一处理发往受管 Edit 的 W 版动态文本（WM_SETTEXT / EM_REPLACESEL）。
// 设计边界（#131，本期刻意不动逻辑）：W 通道只服务 IsTranslatableEdit（class=="Edit" 且存在
// translate 规则）。B2 为 ANSI(x86) 程序，Button/Static 等 caption 控件的动态文本走 A 版
// TryTranslateControlMessageA，其 WM_SETTEXT 普通分支与 ui 枚举路径共用 finalize_caption_text
// 完成助记符/尾部标点加工；非 Edit 控件在下方闸门直接 break 并放行宿主原始 W 消息，不做 caption 加工。
std::optional<LRESULT> TryTranslateControlMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (!hWnd || !is_valid_user_mode_ptr(lParam) || ScopedHookGuard::in_hook() ||
        !I18nManager::instance().is_active()) {
        return std::nullopt;
    }

    // rev15：顶级窗口标题的 W 版实时翻译，与 TryTranslateControlMessageA 中 hWnd==hTop
    // 的 WM_SETTEXT 分支对称。Compose 窗口六种标题（Compose/Reply/Forward/Template，
    // 模板名可能含非 ANSI 字符）若经 SetWindowTextW/SendMessageW 更新也需即时翻译。
    // 严格限定 hWnd==GA_ROOT 顶级窗口本身；Button/Static 等子控件 caption 的 W 通道仍沿用
    // #131 边界（仅服务下方 IsTranslatableEdit），不在此做任何 caption 加工。
    if (Msg == WM_SETTEXT && hWnd == ::GetAncestor(hWnd, GA_ROOT)) {
        const auto* winRule = I18nManager::instance().find_or_bind_window_rule(hWnd);
        if (winRule && winRule->translate_title) {
            const wchar_t* lpTitle = reinterpret_cast<const wchar_t*>(lParam);
            if (lpTitle && lpTitle[0] != L'\0') {
                std::wstring wTitle(lpTitle);
                if (auto transTitle =
                        I18nManager::instance().translate_window_title(*winRule, wTitle)) {
                    ScopedHookGuard guard;
                    return ::SendMessageW(hWnd, WM_SETTEXT, wParam,
                                          reinterpret_cast<LPARAM>(transTitle->c_str()));
                }
            }
        }
        // 顶级窗口不是 Edit 子控件：未命中标题翻译时放行原 W 消息，不再进入下方控件判定。
        return std::nullopt;
    }

    switch (Msg) {
        case WM_SETTEXT:
        case EM_REPLACESEL: {
            auto rules = I18nManager::instance().find_control_rules(hWnd);
            // 仅 Edit 走 W 动态翻译；Button/Static 等 caption 控件在此直接放行（设计边界见函数头，#131）
            if (!IsTranslatableEdit(rules)) break;

            const wchar_t* lpStr = reinterpret_cast<const wchar_t*>(lParam);
            if (!lpStr || lpStr[0] == L'\0') break;

            std::string u8Text = I18nManager::wide_to_utf8(lpStr);
            if (u8Text.empty()) break;

            if (auto trans = I18nManager::instance().translate_control_text(rules, u8Text)) {
                ScopedHookGuard guard;
                if (Msg == WM_SETTEXT) {
                    return ::SendMessageW(hWnd, WM_SETTEXT, wParam,
                                          reinterpret_cast<LPARAM>(trans->text.c_str()));
                }
                if (fpOrigSendMessageW) {
                    return fpOrigSendMessageW(hWnd, EM_REPLACESEL, wParam,
                                              reinterpret_cast<LPARAM>(trans->text.c_str()));
                }
                return ::SendMessageW(hWnd, EM_REPLACESEL, wParam,
                                      reinterpret_cast<LPARAM>(trans->text.c_str()));
            }
            break;
        }
        case SB_SETTEXTW: {
            if ((wParam & SBT_OWNERDRAW) != 0) break;

            auto sbRules = I18nManager::instance().find_control_rules(hWnd);
            if (sbRules.empty() ||
                sbRules.front()->class_name != "msctls_statusbar32") {
                break;
            }

            const wchar_t* lpStr = reinterpret_cast<const wchar_t*>(lParam);
            if (!lpStr || lpStr[0] == L'\0') break;

            const int paneIndex = static_cast<int>(LOWORD(wParam) & 0xFF);
            if (auto trans = I18nManager::instance().translate_status_pane(
                    sbRules, paneIndex, std::wstring_view(lpStr))) {
                ScopedHookGuard guard;
                return ::SendMessageW(hWnd, SB_SETTEXTW, wParam,
                                      reinterpret_cast<LPARAM>(trans->c_str()));
            }
            break;
        }
        default:
            break;
    }

    return std::nullopt;
}

BOOL WINAPI DetourSetWindowTextA(HWND hWnd, LPCSTR lpString) {
    try {
        if (lpString && !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            if (auto res = TryTranslateControlMessageA(hWnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(lpString))) {
                return static_cast<BOOL>(*res);
            }
        }
        return fpOrigSetWindowTextA(hWnd, lpString);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in SetWindowTextA detour, fallback");
        return fpOrigSetWindowTextA(hWnd, lpString);
    }
}

BOOL WINAPI DetourSetDlgItemTextA(HWND hDlg, int nIDDlgItem, LPCSTR lpString) {
    try {
        if (hDlg && lpString && !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            HWND hCtrl = ::GetDlgItem(hDlg, nIDDlgItem);
            if (hCtrl) {
                if (auto res = TryTranslateControlMessageA(hCtrl, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(lpString))) {
                    return static_cast<BOOL>(*res);
                }
            }
        }
        return fpOrigSetDlgItemTextA(hDlg, nIDDlgItem, lpString);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in SetDlgItemTextA detour, fallback");
        return fpOrigSetDlgItemTextA(hDlg, nIDDlgItem, lpString);
    }
}

LRESULT WINAPI DetourSendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    try {
        if (!IsInterceptedMessageA(Msg) ||
            ScopedHookGuard::in_hook() ||
            !I18nManager::instance().is_active()) {
            return fpOrigSendMessageA(hWnd, Msg, wParam, lParam);
        }

        if (Msg == WM_NOTIFY && is_valid_user_mode_ptr(lParam)) {
            if (!is_memory_readable(reinterpret_cast<const void*>(lParam), sizeof(NMHDR))) {
                return fpOrigSendMessageA(hWnd, Msg, wParam, lParam);
            }
            auto* pHdr = reinterpret_cast<NMHDR*>(lParam);
            if (pHdr->code == TTN_NEEDTEXTA) {
                LRESULT ret = fpOrigSendMessageA(hWnd, Msg, wParam, lParam);
                UiTranslator::translate_tooltip_notify_a(reinterpret_cast<NMTTDISPINFOA*>(pHdr));
                return ret;
            }
            if (pHdr->code == TTN_NEEDTEXTW) {
                LRESULT ret = fpOrigSendMessageA(hWnd, Msg, wParam, lParam);
                UiTranslator::translate_tooltip_notify_w(reinterpret_cast<NMTTDISPINFOW*>(pHdr));
                return ret;
            }
        }

        if (auto res = TryTranslateControlMessageA(hWnd, Msg, wParam, lParam)) {
            return *res;
        }
        return fpOrigSendMessageA(hWnd, Msg, wParam, lParam);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in SendMessageA detour, fallback to original");
        return fpOrigSendMessageA(hWnd, Msg, wParam, lParam);
    }
}

// 仅针对 SB_SETTEXTA 与 TVM_GETITEMA 拦截 CallWindowProcA / DefWindowProcA
LRESULT WINAPI DetourCallWindowProcA(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    try {
        if (Msg == SB_SETTEXTA || Msg == TVM_GETITEMA) {
            if (auto res = TryTranslateControlMessageA(hWnd, Msg, wParam, lParam)) {
                return *res;
            }
        }
        return fpOrigCallWindowProcA(lpPrevWndFunc, hWnd, Msg, wParam, lParam);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in CallWindowProcA detour, fallback");
        return fpOrigCallWindowProcA(lpPrevWndFunc, hWnd, Msg, wParam, lParam);
    }
}

LRESULT WINAPI DetourDefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    try {
        if (Msg == SB_SETTEXTA || Msg == TVM_GETITEMA) {
            if (auto res = TryTranslateControlMessageA(hWnd, Msg, wParam, lParam)) {
                return *res;
            }
        }
        return fpOrigDefWindowProcA(hWnd, Msg, wParam, lParam);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in DefWindowProcA detour, fallback");
        return fpOrigDefWindowProcA(hWnd, Msg, wParam, lParam);
    }
}

// 进入弹出菜单模态前建立 glyph 菜单上下文，退出（含异常）时复位，确保线程局部状态不泄漏。
class ScopedGlyphMenuContext {
public:
    explicit ScopedGlyphMenuContext(HWND hOwner) {
        HWND hTop = hOwner ? ::GetAncestor(hOwner, GA_ROOT) : nullptr;
        const WindowRule* pRule = nullptr;
        if (hTop && I18nManager::instance().is_active()) {
            pRule = I18nManager::instance().find_or_bind_window_rule(hTop);
        }
        // 全部计算成功后最后再写入共享上下文，避免构造期异常留下半激活状态
        g_glyphMenuCtx.active = true;
        g_glyphMenuCtx.top = hTop;
        g_glyphMenuCtx.window_rule = pRule;
    }
    ~ScopedGlyphMenuContext() {
        g_glyphMenuCtx = GlyphMenuContext{};
    }
    ScopedGlyphMenuContext(const ScopedGlyphMenuContext&) = delete;
    ScopedGlyphMenuContext& operator=(const ScopedGlyphMenuContext&) = delete;
};

BOOL WINAPI DetourTrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT* prcRect) {
    try {
        if (hMenu && !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            UiTranslator::translate_menu(hMenu, nullptr);
            PrescanHeaderOwnerdrawMenu(hMenu, hWnd);
        }
        ScopedHeaderMenuSubclass headerSubclass(hWnd);
        ScopedGlyphMenuContext glyphCtx(hWnd);
        return fpOrigTrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in TrackPopupMenu detour, fallback");
        return fpOrigTrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);
    }
}

BOOL WINAPI DetourTrackPopupMenuEx(HMENU hMenu, UINT uFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm) {
    try {
        if (hMenu && !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            UiTranslator::translate_menu(hMenu, nullptr);
            PrescanHeaderOwnerdrawMenu(hMenu, hWnd);
        }
        ScopedHeaderMenuSubclass headerSubclass(hWnd);
        ScopedGlyphMenuContext glyphCtx(hWnd);
        return fpOrigTrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, lptpm);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in TrackPopupMenuEx detour, fallback");
        return fpOrigTrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, lptpm);
    }
}

BOOL WINAPI DetourModifyMenuA(HMENU hMnu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
    try {
        if ((uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR | MF_POPUP)) == 0 &&
            is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(lpNewItem)) &&
            !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            ScopedHookGuard guard;
            if (auto trans = I18nManager::instance().translate_menu_command(static_cast<UINT>(uIDNewItem), lpNewItem, false)) {
                return ::ModifyMenuW(hMnu, uPosition, uFlags, uIDNewItem, trans->c_str());
            }
        }
        return fpOrigModifyMenuA(hMnu, uPosition, uFlags, uIDNewItem, lpNewItem);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in ModifyMenuA detour, fallback");
        return fpOrigModifyMenuA(hMnu, uPosition, uFlags, uIDNewItem, lpNewItem);
    }
}

BOOL WINAPI DetourInsertMenuA(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
    try {
        if ((uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR | MF_POPUP)) == 0 &&
            is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(lpNewItem)) &&
            !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            ScopedHookGuard guard;
            if (auto trans = I18nManager::instance().translate_menu_command(static_cast<UINT>(uIDNewItem), lpNewItem, false)) {
                return ::InsertMenuW(hMenu, uPosition, uFlags, uIDNewItem, trans->c_str());
            }
        }
        return fpOrigInsertMenuA(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in InsertMenuA detour, fallback");
        return fpOrigInsertMenuA(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem);
    }
}

BOOL WINAPI DetourAppendMenuA(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
    try {
        if ((uFlags & (MF_BITMAP | MF_OWNERDRAW | MF_SEPARATOR | MF_POPUP)) == 0 &&
            is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(lpNewItem)) &&
            !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            ScopedHookGuard guard;
            if (auto trans = I18nManager::instance().translate_menu_command(static_cast<UINT>(uIDNewItem), lpNewItem, false)) {
                return ::AppendMenuW(hMenu, uFlags, uIDNewItem, trans->c_str());
            }
        }
        return fpOrigAppendMenuA(hMenu, uFlags, uIDNewItem, lpNewItem);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in AppendMenuA detour, fallback");
        return fpOrigAppendMenuA(hMenu, uFlags, uIDNewItem, lpNewItem);
    }
}

BOOL WINAPI DetourSetWindowTextW(HWND hWnd, LPCWSTR lpString) {
    try {
        if (lpString && !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            if (auto res = TryTranslateControlMessageW(hWnd, WM_SETTEXT, 0,
                                                       reinterpret_cast<LPARAM>(lpString))) {
                return static_cast<BOOL>(*res);
            }
        }
        return fpOrigSetWindowTextW(hWnd, lpString);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in SetWindowTextW detour, fallback");
        return fpOrigSetWindowTextW(hWnd, lpString);
    }
}

LRESULT WINAPI DetourSendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    try {
        if (!IsInterceptedMessageW(Msg) ||
            ScopedHookGuard::in_hook() ||
            !I18nManager::instance().is_active()) {
            return fpOrigSendMessageW(hWnd, Msg, wParam, lParam);
        }

        if (auto res = TryTranslateControlMessageW(hWnd, Msg, wParam, lParam)) {
            return *res;
        }
        return fpOrigSendMessageW(hWnd, Msg, wParam, lParam);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in SendMessageW detour, fallback to original");
        return fpOrigSendMessageW(hWnd, Msg, wParam, lParam);
    }
}

LRESULT WINAPI DetourSendDlgItemMessageA(HWND hDlg, int nIDDlgItem, UINT Msg,
                                       WPARAM wParam, LPARAM lParam) {
    try {
        if (hDlg && IsInterceptedMessageA(Msg) &&
            !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            HWND hCtrl = ::GetDlgItem(hDlg, nIDDlgItem);
            if (hCtrl) {
                if (auto res = TryTranslateControlMessageA(hCtrl, Msg, wParam, lParam)) {
                    return *res;
                }
            }
        }
        return fpOrigSendDlgItemMessageA(hDlg, nIDDlgItem, Msg, wParam, lParam);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in SendDlgItemMessageA detour, fallback");
        return fpOrigSendDlgItemMessageA(hDlg, nIDDlgItem, Msg, wParam, lParam);
    }
}

LRESULT WINAPI DetourSendDlgItemMessageW(HWND hDlg, int nIDDlgItem, UINT Msg,
                                        WPARAM wParam, LPARAM lParam) {
    try {
        if (hDlg && IsInterceptedMessageW(Msg) &&
            !ScopedHookGuard::in_hook() && I18nManager::instance().is_active()) {
            HWND hCtrl = ::GetDlgItem(hDlg, nIDDlgItem);
            if (hCtrl) {
                if (auto res = TryTranslateControlMessageW(hCtrl, Msg, wParam, lParam)) {
                    return *res;
                }
            }
        }
        return fpOrigSendDlgItemMessageW(hDlg, nIDDlgItem, Msg, wParam, lParam);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in SendDlgItemMessageW detour, fallback");
        return fpOrigSendDlgItemMessageW(hDlg, nIDDlgItem, Msg, wParam, lParam);
    }
}

// ExtTextOutW 拦截，覆盖两条独立通道：
//  1) ETO_GLYPH_INDEX 自绘弹出菜单（如 Compose 优先级菜单），由窗口级 glyph_menu 规则驱动；
//  2) 邮件头 Static 1006..1009 处理 ODT_MENU 的 WM_DRAWITEM 期间的自绘菜单。
BOOL WINAPI DetourExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT* lprect,
                             LPCWSTR lpString, UINT c, const INT* lpDx) {
    try {
        // ETO_GLYPH_INDEX 自绘弹出菜单（如 Compose 优先级菜单）：先把 glyph 反解为明文，
        // 命中窗口级 glyph_menu 规则则去掉 GLYPH_INDEX，以正常字符输出译文。
        if (g_glyphMenuCtx.active && c > 0 && g_glyphMenuCtx.window_rule != nullptr &&
            (options & ETO_GLYPH_INDEX) != 0 && !ScopedHookGuard::in_hook()) {
            if (auto plain = DecodeGlyphText(hdc, lpString, c)) {
                if (auto translated = I18nManager::instance().translate_glyph_menu_text(
                        *g_glyphMenuCtx.window_rule, *plain)) {
                    ScopedHookGuard guard;
                    const UINT plainOptions = options & ~static_cast<UINT>(ETO_GLYPH_INDEX);
                    return fpOrigExtTextOutW(hdc, x, y, plainOptions, lprect,
                                            translated->c_str(),
                                            static_cast<UINT>(translated->size()), nullptr);
                }
            }
        }
        if (g_inOwnerDrawDraw && !ScopedHookGuard::in_hook()) {
            // 1. 不论是否带 ETO_GLYPH_INDEX，只要单行拼接模式已输出整行译文，后续所有字符绘制一律吞除
            if (g_suppressRemainingDraws && c > 0) {
                return TRUE;
            }

            // 2. 命中左栏首段带字形索引的绘制调用
            if ((options & ETO_GLYPH_INDEX) != 0 &&
                x <= g_ownerDrawRcLeft + kOwnerDrawLeftColumnLimit) {
                auto it = g_headerItemTranslations.find(g_currentDrawItemId);
                if (it != g_headerItemTranslations.end() && !it->second.text.empty()) {
                    const bool isFullLine = it->second.is_full_line;
                    if (isFullLine) {
                        g_suppressRemainingDraws = true;
                    }
                    // ScopedHookGuard 确保译文自身在内部经 LpkExtTextOut 分段绘制时直通原函数
                    ScopedHookGuard guard;
                    const UINT plainOptions = options & ~static_cast<UINT>(ETO_GLYPH_INDEX);
                    BOOL ret = fpOrigExtTextOutW(hdc, x, y, plainOptions, lprect,
                                                it->second.text.c_str(),
                                                static_cast<UINT>(it->second.text.size()), nullptr);
                    if (isFullLine && hdc) {
                        // 将 DC 裁剪区设为空，彻底阻止 gdi32full 内部字体链接直通内核绘制残余中文片段
                        ::IntersectClipRect(hdc, 0, 0, 0, 0);
                    }
                    return ret;
                }
            }
        }
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in ExtTextOutW detour, fallback");
    }
    return fpOrigExtTextOutW(hdc, x, y, options, lprect, lpString, c, lpDx);
}

// 邮件内容面板底部自绘页签条 (AfxWnd, ctrl_id: 12562) 测量与绘制拦截：
// 同时覆盖窗口消息内绘制、CClientDC 直接绘制，以及切换邮件时 C++ 直接向离屏内存双缓冲 DC 的绘制。

int WINAPI DetourDrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format) {
    try {
        if (!g_inDrawTextDetour && !(format & DT_MODIFYSTRING) &&
            g_hSubclassedMailTab != nullptr &&
            is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(lpchText)) &&
            is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(lprc)) &&
            is_memory_readable(lprc, sizeof(RECT)) &&
            I18nManager::instance().is_active()) {

            std::string_view sv;
            if (cchText < 0) {
                sv = std::string_view(lpchText);
            } else if (cchText > 0 && cchText < 256) {
                sv = std::string_view(lpchText, static_cast<size_t>(cchText));
            }

            if (!sv.empty() && sv.size() < 256) {
                HWND hDcWnd = hdc ? ::WindowFromDC(hdc) : nullptr;
                const bool isDirectTabContext = g_inMailTabPaint || (hDcWnd == g_hSubclassedMailTab);
                const bool isOffscreenTabContext = (hDcWnd == nullptr) &&
                                                   (lprc->top >= 0 && lprc->top <= 6) &&
                                                   ((lprc->bottom - lprc->top) <= 30) &&
                                                   IsMailTabLabelPattern(sv);

                if (isDirectTabContext || isOffscreenTabContext) {
                    auto trans = I18nManager::instance().translate_mail_tab_text(sv);
                    if (trans) {
                        // 页签条字号本就与左侧树根节点一致、比常规字号大两号，属宿主固有设计，保持 W 通道原样输出。
                        g_inDrawTextDetour = true;
                        int ret = ::DrawTextW(hdc, trans->c_str(), static_cast<int>(trans->size()), lprc, format);
                        g_inDrawTextDetour = false;
                        return ret;
                    }
                }
            }
        }

        // rev14 通道二：控件 gdi_patterns 自绘文本（任务 pane 的 Task/Deadline/Comment 三行等）。
        // 廉价前缀预筛后才解析 DC 窗口与规则；窗口 DC 命中即缓存作用域，供同周期随后的双缓冲内存 DC 复用。
        if (!g_inDrawTextDetour && !(format & DT_MODIFYSTRING) &&
            I18nManager::instance().is_active() &&
            is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(lpchText)) &&
            is_valid_user_mode_ptr(reinterpret_cast<LPARAM>(lprc)) &&
            is_memory_readable(lprc, sizeof(RECT))) {

            std::string_view gsv;
            if (cchText < 0) {
                gsv = std::string_view(lpchText);
            } else if (cchText > 0 && cchText < 512) {
                gsv = std::string_view(lpchText, static_cast<size_t>(cchText));
            }

            if (!gsv.empty() && gsv.size() < 512 && IsGdiPatternCandidate(gsv)) {
                HWND hDcWndGdi = hdc ? ::WindowFromDC(hdc) : nullptr;
                HWND hScope = nullptr;
                std::vector<const ControlRule*> gdiRules;

                auto pickIfHasPatterns = [&](HWND hCandidate) -> bool {
                    if (!hCandidate || !::IsWindow(hCandidate)) return false;
                    auto rs = I18nManager::instance().find_control_rules(hCandidate);
                    if (std::any_of(rs.begin(), rs.end(),
                                    [](const ControlRule* r) noexcept {
                                        return r != nullptr && !r->gdi_patterns.empty();
                                    })) {
                        gdiRules = std::move(rs);
                        hScope = hCandidate;
                        return true;
                    }
                    return false;
                };

                if (hDcWndGdi != nullptr) {
                    // 窗口 DC：只匹配其所属控件，命中后记录作用域
                    if (pickIfHasPatterns(hDcWndGdi)) {
                        g_hGdiPatternWnd = hScope;
                    }
                } else if (g_hGdiPatternWnd != nullptr) {
                    // 双缓冲内存 DC：WindowFromDC 为空，回退到最近活动的任务 pane 作用域窗口
                    if (!::IsWindow(g_hGdiPatternWnd) || !pickIfHasPatterns(g_hGdiPatternWnd)) {
                        g_hGdiPatternWnd = nullptr;
                    }
                }

                if (hScope != nullptr && !gdiRules.empty()) {
                    if (auto trans = I18nManager::instance().translate_gdi_pattern_text(gdiRules, gsv)) {
                        g_inDrawTextDetour = true;
                        const int gdiRet = ::DrawTextW(hdc, trans->c_str(),
                                                       static_cast<int>(trans->size()),
                                                       lprc, format);
                        g_inDrawTextDetour = false;
                        return gdiRet;
                    }
                }
            }
        }
    } catch (...) {
        g_inDrawTextDetour = false;
        BKI_LOG_ERROR("unhandled exception in DrawTextA detour, fallback");
    }
    return fpOrigDrawTextA(hdc, lpchText, cchText, lprc, format);
}

} // namespace

void ApiHooks::subclass_mail_tab_strip(HWND hWnd) {
    if (!hWnd || !::IsWindow(hWnd)) return;

    if (g_hSubclassedMailTab && g_hSubclassedMailTab != hWnd &&
        ::IsWindow(g_hSubclassedMailTab) && g_origMailTabProc) {
        auto oldCur = reinterpret_cast<WNDPROC>(::GetWindowLongPtrA(g_hSubclassedMailTab, GWLP_WNDPROC));
        if (oldCur == MailTabStripSubclassProc) {
            ::SetWindowLongPtrA(g_hSubclassedMailTab, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origMailTabProc));
        }
        g_hSubclassedMailTab = nullptr;
        g_origMailTabProc = nullptr;
    }

    WNDPROC curProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtrA(hWnd, GWLP_WNDPROC));
    if (curProc && curProc != MailTabStripSubclassProc) {
        LONG_PTR prev = ::SetWindowLongPtrA(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MailTabStripSubclassProc));
        if (prev != 0) {
            g_origMailTabProc = curProc;
            g_hSubclassedMailTab = hWnd;
        }
    } else if (curProc == MailTabStripSubclassProc) {
        g_hSubclassedMailTab = hWnd;
    }
}

void ApiHooks::restore_mail_tab_subclass() {
    if (g_hSubclassedMailTab && ::IsWindow(g_hSubclassedMailTab) && g_origMailTabProc) {
        auto cur = reinterpret_cast<WNDPROC>(::GetWindowLongPtrA(g_hSubclassedMailTab, GWLP_WNDPROC));
        if (cur == MailTabStripSubclassProc) {
            ::SetWindowLongPtrA(g_hSubclassedMailTab, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origMailTabProc));
        }
    }
    g_hSubclassedMailTab = nullptr;
    g_origMailTabProc = nullptr;
    g_inMailTabPaint = false;
}

void ApiHooks::invalidate_mail_tab_strip() {
    if (g_hSubclassedMailTab && ::IsWindow(g_hSubclassedMailTab) && ::IsWindowVisible(g_hSubclassedMailTab)) {
        ::InvalidateRect(g_hSubclassedMailTab, nullptr, FALSE);
    }
}

bool ApiHooks::install() {
    if (!I18nManager::instance().is_active()) {
        return true;
    }
    if (MH_Initialize() != MH_OK) {
        return false;
    }

    MH_CreateHookApi(L"user32.dll", "SetWindowTextA",   &DetourSetWindowTextA,   reinterpret_cast<LPVOID*>(&fpOrigSetWindowTextA));
    MH_CreateHookApi(L"user32.dll", "SetDlgItemTextA",  &DetourSetDlgItemTextA,  reinterpret_cast<LPVOID*>(&fpOrigSetDlgItemTextA));
    MH_CreateHookApi(L"user32.dll", "SendMessageA",     &DetourSendMessageA,     reinterpret_cast<LPVOID*>(&fpOrigSendMessageA));
    MH_CreateHookApi(L"user32.dll", "CallWindowProcA",  &DetourCallWindowProcA,  reinterpret_cast<LPVOID*>(&fpOrigCallWindowProcA));
    MH_CreateHookApi(L"user32.dll", "DefWindowProcA",   &DetourDefWindowProcA,   reinterpret_cast<LPVOID*>(&fpOrigDefWindowProcA));
    MH_CreateHookApi(L"user32.dll", "TrackPopupMenu",   &DetourTrackPopupMenu,   reinterpret_cast<LPVOID*>(&fpOrigTrackPopupMenu));
    MH_CreateHookApi(L"user32.dll", "TrackPopupMenuEx", &DetourTrackPopupMenuEx, reinterpret_cast<LPVOID*>(&fpOrigTrackPopupMenuEx));
    MH_CreateHookApi(L"user32.dll", "ModifyMenuA",      &DetourModifyMenuA,      reinterpret_cast<LPVOID*>(&fpOrigModifyMenuA));
    MH_CreateHookApi(L"user32.dll", "InsertMenuA",      &DetourInsertMenuA,      reinterpret_cast<LPVOID*>(&fpOrigInsertMenuA));
    MH_CreateHookApi(L"user32.dll", "AppendMenuA",      &DetourAppendMenuA,      reinterpret_cast<LPVOID*>(&fpOrigAppendMenuA));

    // W 版与 SendDlgItemMessage：覆盖 Edit 115 切换邮件时的全部更新通道
    MH_CreateHookApi(L"user32.dll", "SetWindowTextW",      &DetourSetWindowTextW,      reinterpret_cast<LPVOID*>(&fpOrigSetWindowTextW));
    MH_CreateHookApi(L"user32.dll", "SendMessageW",        &DetourSendMessageW,        reinterpret_cast<LPVOID*>(&fpOrigSendMessageW));
    MH_CreateHookApi(L"user32.dll", "SendDlgItemMessageA", &DetourSendDlgItemMessageA, reinterpret_cast<LPVOID*>(&fpOrigSendDlgItemMessageA));
    MH_CreateHookApi(L"user32.dll", "SendDlgItemMessageW", &DetourSendDlgItemMessageW, reinterpret_cast<LPVOID*>(&fpOrigSendDlgItemMessageW));

    // 自绘弹出菜单 (FromToSubjDate_popup_menu) 真实绘制拦截
    MH_CreateHookApi(L"gdi32.dll",  "ExtTextOutW", &DetourExtTextOutW, reinterpret_cast<LPVOID*>(&fpOrigExtTextOutW));
    // 邮件内容面板底部自绘页签条 (AfxWnd, ctrl_id: 12562) 测量与绘制拦截
    MH_CreateHookApi(L"user32.dll", "DrawTextA",   &DetourDrawTextA,   reinterpret_cast<LPVOID*>(&fpOrigDrawTextA));

    return (MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
}

void ApiHooks::uninstall() {
    if (!I18nManager::instance().is_active()) return;

    // 必须先禁用全部 hook（detour 停止转发），再反初始化 MinHook。顺序不可颠倒：
    // MH_Uninitialize 会释放 trampoline，若仍有线程处于 detour 内将访问已释放内存。
    // 本插件仅在进程退出（DLL_PROCESS_DETACH）时卸载，不支持运行期热卸载/重新加载。
    if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK) {
        BKI_LOG_ERROR("MH_DisableHook failed during uninstall");
    }
    if (MH_Uninitialize() != MH_OK) {
        BKI_LOG_ERROR("MH_Uninitialize failed during uninstall");
    }
}

} // namespace bki18n