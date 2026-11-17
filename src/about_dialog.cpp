//  Implements the modal About dialog, showing plugin information, language metadata, and the project homepage link.
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
#include "about_dialog.hpp"
#include "i18n_manager.hpp"
#include "logger.hpp"
#include "version.hpp"

#include <shellapi.h>
#include <vector>
#include <string>
#include <cstdint>

namespace bki18n {

namespace {

constexpr WORD IDC_LABEL_PLUGIN_NAME = 2001;
constexpr WORD IDC_VAL_PLUGIN_NAME   = 2002;
constexpr WORD IDC_LABEL_DEV         = 2003;
constexpr WORD IDC_VAL_DEV           = 2004;
constexpr WORD IDC_LABEL_HOMEPAGE    = 2005;
constexpr WORD IDC_STATIC_HOME_LINK  = 2006;
constexpr WORD IDC_GROUP_LANG_INFO   = 2007;
constexpr WORD IDC_LABEL_CUR_LANG    = 2008;
constexpr WORD IDC_VAL_CUR_LANG      = 2009;
constexpr WORD IDC_LABEL_LANG_VER    = 2010;
constexpr WORD IDC_VAL_LANG_VER      = 2011;
constexpr WORD IDC_LABEL_LANG_AUTHOR = 2012;
constexpr WORD IDC_VAL_LANG_AUTHOR   = 2013;

constexpr const wchar_t* kProjectHomepageUrl = L"https://github.com/robot-dot-win/Becky-i18n";

// 内存对话框构建器（按 DWORD 对齐写入 DLGTEMPLATE 与 DLGITEMTEMPLATE）
class DialogTemplateBuilder {
public:
    void align_dword() {
        while (buf_.size() % 4 != 0) {
            buf_.push_back(0);
        }
    }

    void append_bytes(const void* data, size_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        buf_.insert(buf_.end(), bytes, bytes + size);
    }

    void append_word(WORD w) {
        append_bytes(&w, sizeof(WORD));
    }

    void append_wstring(const wchar_t* wstr) {
        size_t len = wcslen(wstr) + 1;
        append_bytes(wstr, len * sizeof(wchar_t));
    }

    void begin_dialog(short width, short height, const wchar_t* title, const wchar_t* fontName, WORD fontSize) {
        DLGTEMPLATE dlg{};
        dlg.style = DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
        dlg.cdit = 0;
        dlg.x = 0;
        dlg.y = 0;
        dlg.cx = width;
        dlg.cy = height;

        append_bytes(&dlg, sizeof(dlg));
        append_word(0); // 无菜单
        append_word(0); // 默认对话框类
        append_wstring(title);
        append_word(fontSize);
        append_wstring(fontName);
    }

    void add_item(WORD classAtom, DWORD style, short x, short y, short cx, short cy, WORD id, const wchar_t* text) {
        align_dword();

        DLGITEMTEMPLATE item{};
        item.style = WS_CHILD | WS_VISIBLE | style;
        item.dwExtendedStyle = 0;
        item.x = x;
        item.y = y;
        item.cx = cx;
        item.cy = cy;
        item.id = id;

        append_bytes(&item, sizeof(item));
        append_word(0xFFFF);
        append_word(classAtom);
        append_wstring(text);
        append_word(0); // 无附加创建数据

        auto* hdr = reinterpret_cast<DLGTEMPLATE*>(buf_.data());
        hdr->cdit++;
    }

    const DLGTEMPLATE* data() const {
        return reinterpret_cast<const DLGTEMPLATE*>(buf_.data());
    }

private:
    std::vector<uint8_t> buf_;
};

HFONT g_hLinkFont = nullptr;

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    try {
    switch (uMsg) {
        case WM_INITDIALOG: {
            // 为项目主页创建带下划线的字体
            HFONT hDlgFont = reinterpret_cast<HFONT>(::SendMessageW(hDlg, WM_GETFONT, 0, 0));
            if (hDlgFont) {
                LOGFONTW lf{};
                if (::GetObjectW(hDlgFont, sizeof(lf), &lf)) {
                    lf.lfUnderline = TRUE;
                    g_hLinkFont = ::CreateFontIndirectW(&lf);
                    if (g_hLinkFont) {
                        ::SendDlgItemMessageW(hDlg, IDC_STATIC_HOME_LINK, WM_SETFONT, reinterpret_cast<WPARAM>(g_hLinkFont), TRUE);
                    }
                }
            }
            return TRUE;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND hCtrl = reinterpret_cast<HWND>(lParam);
            if (::GetDlgCtrlID(hCtrl) == IDC_STATIC_HOME_LINK) {
                ::SetTextColor(hdc, RGB(0, 102, 204));
                ::SetBkMode(hdc, TRANSPARENT);
                return reinterpret_cast<INT_PTR>(::GetSysColorBrush(COLOR_BTNFACE));
            }
            break;
        }

        case WM_SETCURSOR: {
            HWND hCtrl = reinterpret_cast<HWND>(wParam);
            if (hCtrl && ::GetDlgCtrlID(hCtrl) == IDC_STATIC_HOME_LINK) {
                ::SetCursor(::LoadCursorW(nullptr, MAKEINTRESOURCEW(32649))); // IDC_HAND
                ::SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, TRUE);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            WORD code = HIWORD(wParam);
            if (id == IDC_STATIC_HOME_LINK && code == STN_CLICKED) {
                ::ShellExecuteW(hDlg, L"open", kProjectHomepageUrl, nullptr, nullptr, SW_SHOWNORMAL);
                return TRUE;
            }
            if (id == IDOK || id == IDCANCEL) {
                ::EndDialog(hDlg, id);
                return TRUE;
            }
            break;
        }

        case WM_DESTROY: {
            if (g_hLinkFont) {
                ::DeleteObject(g_hLinkFont);
                g_hLinkFont = nullptr;
            }
            break;
        }

        default:
            break;
    }
    } catch (...) {
        // 对话框回调边界：异常绝不允许逃逸到 CRT，返回 FALSE 走系统默认处理
        BKI_LOG_ERROR("unhandled exception in about dialog proc, skip");
    }
    return FALSE;
}

} // namespace

void AboutDialog::show(HINSTANCE hInstance, HWND hParent) {
    ScopedHookGuard guard;

    const auto& meta = I18nManager::instance().active_locale_meta();

    DialogTemplateBuilder builder;
    builder.begin_dialog(250, 164, L"About Becky! I18n", L"MS Shell Dlg", 9);

    constexpr short x1 = 18;         // 左侧标签 x
    constexpr short x2 = x1+66;      // 右侧值 x

    // 顶部插件信息区
    builder.add_item(0x0082, SS_LEFT,             x1, 12, 52,  12, IDC_LABEL_PLUGIN_NAME, L"Plug-in Name:");
    const std::wstring pluginName =
        std::wstring(L"Becky! I18n (v") + version::kVersionW + L")";
    builder.add_item(0x0082, SS_LEFT,             x2, 12, 172, 12, IDC_VAL_PLUGIN_NAME,   pluginName.c_str());

    builder.add_item(0x0082, SS_LEFT,             x1, 28, 52,  12, IDC_LABEL_DEV,         L"Developer:");
    builder.add_item(0x0082, SS_LEFT,             x2, 28, 172, 12, IDC_VAL_DEV,           L"Martin Young");

    builder.add_item(0x0082, SS_LEFT,             x1, 44, 52,  12, IDC_LABEL_HOMEPAGE,    L"Homepage:");
    builder.add_item(0x0082, SS_LEFT | SS_NOTIFY, x2, 44, 172, 12, IDC_STATIC_HOME_LINK,  kProjectHomepageUrl);

    // 当前语言信息分组框
    builder.add_item(0x0080, BS_GROUPBOX,         10, 64, 230, 66, IDC_GROUP_LANG_INFO,   L"Active Language Pack");

    builder.add_item(0x0082, SS_LEFT,             x1, 78,  64, 12, IDC_LABEL_CUR_LANG,    L"Current Language:");
    builder.add_item(0x0082, SS_LEFT,             x2, 78, 150, 12, IDC_VAL_CUR_LANG,      meta.display_name.c_str());

    builder.add_item(0x0082, SS_LEFT,             x1, 94,  64, 12, IDC_LABEL_LANG_VER,    L"Language Version:");
    builder.add_item(0x0082, SS_LEFT,             x2, 94, 150, 12, IDC_VAL_LANG_VER,      meta.version.c_str());

    builder.add_item(0x0082, SS_LEFT,             x1, 110, 64, 12, IDC_LABEL_LANG_AUTHOR, L"Language Author:");
    builder.add_item(0x0082, SS_LEFT,             x2, 110, 150, 12, IDC_VAL_LANG_AUTHOR,  meta.author.c_str());

    builder.add_item(0x0080, BS_DEFPUSHBUTTON | WS_TABSTOP, 95, 138, 60, 16, IDOK, L"OK");

    ::DialogBoxIndirectParamW(hInstance, builder.data(), hParent, AboutDlgProc, 0);
}

} // namespace bki18n