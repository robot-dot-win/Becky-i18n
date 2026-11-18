//  Implements the modal UI-language picker dialog with built-in restart and system-locale guidance.
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
#include "language_dialog.hpp"

#include "config_reg.hpp"
#include "i18n_manager.hpp"
#include "logger.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace bki18n {

namespace {

constexpr WORD IDC_LANG_COMBO = 2100;

// 英文文案写死在代码中；将来由语言 JSON 的 src/dst 一视同仁地翻译，本文件不含译文。
constexpr const wchar_t* kRestartText =
    L"The selected language will take effect after you restart Becky!.";
constexpr const wchar_t* kSystemLocaleText =
    L"Your current system locale (the language for non-Unicode programs) may not display every "
    L"language correctly. If menus or the folder tree appear as question marks (\"?\"), open "
    L"Control Panel > Region > Administrative tab > Change system locale, select the language "
    L"matching your choice, and then restart the computer.";
constexpr const wchar_t* kCurrentLanguageLabel = L"Current language:";

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
        const size_t len = wcslen(wstr) + 1;
        append_bytes(wstr, len * sizeof(wchar_t));
    }

    void begin_dialog(short width, short height, const wchar_t* title,
                      const wchar_t* fontName, WORD fontSize) {
        DLGTEMPLATE dlg{};
        dlg.style = DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION |
                    WS_SYSMENU;
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

    void add_item(WORD classAtom, DWORD style, short x, short y, short cx, short cy,
                  WORD id, const wchar_t* text) {
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
        append_word(0); // 无附加数据

        auto* hdr = reinterpret_cast<DLGTEMPLATE*>(buf_.data());
        hdr->cdit++;
    }

    const DLGTEMPLATE* data() const {
        return reinterpret_cast<const DLGTEMPLATE*>(buf_.data());
    }

private:
    std::vector<uint8_t> buf_;
};

INT_PTR CALLBACK LanguageDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    try {
        switch (uMsg) {
            case WM_INITDIALOG: {
                auto* locales = reinterpret_cast<std::vector<LocaleMeta>*>(lParam);
                ::SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
                if (!locales) return FALSE;

                HWND hCombo = ::GetDlgItem(hDlg, IDC_LANG_COMBO);
                const std::string current = read_reg_locale();

                int selected = 0;
                for (size_t i = 0; i < locales->size(); ++i) {
                    ::SendMessageW(hCombo, CB_ADDSTRING, 0,
                                   reinterpret_cast<LPARAM>((*locales)[i].display_name.c_str()));
                    if ((*locales)[i].id == current) {
                        selected = static_cast<int>(i);
                    }
                }
                // 下拉框关闭态直接显示当前语言，无需用户展开即可确认
                ::SendMessageW(hCombo, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
                return TRUE;
            }

            case WM_COMMAND: {
                const WORD id = LOWORD(wParam);
                if (id == IDOK) {
                    auto* locales = reinterpret_cast<std::vector<LocaleMeta>*>(
                        ::GetWindowLongPtrW(hDlg, GWLP_USERDATA));
                    HWND hCombo = ::GetDlgItem(hDlg, IDC_LANG_COMBO);
                    const int sel = static_cast<int>(
                        ::SendMessageW(hCombo, CB_GETCURSEL, 0, 0));
                    if (locales && sel >= 0 &&
                        static_cast<size_t>(sel) < locales->size()) {
                        write_reg_locale((*locales)[sel].id);
                    }
                    ::EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                if (id == IDCANCEL) {
                    ::EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                }
                break;
            }

            default:
                break;
        }
    } catch (...) {
        // 对话框回调边界：异常绝不允许逃逸到 CRT，返回 FALSE 走系统默认处理
        BKI_LOG_ERROR("unhandled exception in language dialog proc, skip");
    }
    return FALSE;
}

} // namespace

void LanguageDialog::show(HINSTANCE hInstance, HWND hParent) {
    ScopedHookGuard guard;

    // 每次打开都重新扫描 locale 目录，运行期间增删语言文件立即生效
    std::vector<LocaleMeta> locales =
        I18nManager::instance().scan_available_locales(get_locale_dir(hInstance));

    // 即使当前语言文件已被删除，列表也要保证至少包含英文与当前语言，
    // 否则下拉框会因找不到当前项而退化显示为第一项（英文）。
    const std::string current = read_reg_locale();
    const bool hasCurrent = std::any_of(locales.begin(), locales.end(),
        [&](const LocaleMeta& m) { return m.id == current; });
    if (!hasCurrent) {
        // 当前语言文件在运行期间被删除：init 已成功加载并缓存其 meta，
        // 直接复用内存中的 display_name，避免退化成裸 id；对用户自行新增的
        // 语言同样成立，无需在代码里维护任何静态名称映射表。
        LocaleMeta placeholder = I18nManager::instance().active_locale_meta();
        placeholder.id = current;
        placeholder.version = L"(file missing)";
        placeholder.author = L"(file missing)";
        locales.push_back(std::move(placeholder));
    }

    DialogTemplateBuilder builder;
    builder.begin_dialog(300, 129, L"UI Language", L"MS Shell Dlg", 9);

    // 顶部：重启说明与系统 locale 指引（英文写死，将来由 JSON 翻译）
    builder.add_item(0x0082, SS_LEFT, 7, 7, 286, 8, 0xFFFF, kRestartText);
    builder.add_item(0x0082, SS_LEFT, 7, 19, 286, 58, 0xFFFF, kSystemLocaleText);

    // 当前语言标签 + 只读下拉框（W 通道，display_name 不被系统代码页损坏；名称本身不翻译）
    builder.add_item(0x0082, SS_LEFT, 7, 72, 286, 8, 0xFFFF, kCurrentLanguageLabel);
    builder.add_item(0x0085,
                     CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
                     7, 83, 286, 86, IDC_LANG_COMBO, L"");

    // 操作按钮
    builder.add_item(0x0080, BS_DEFPUSHBUTTON | WS_TABSTOP,
                     187, 108, 50, 14, IDOK, L"OK");
    builder.add_item(0x0080, WS_TABSTOP,
                     243, 108, 50, 14, IDCANCEL, L"Cancel");

    ::DialogBoxIndirectParamW(hInstance, builder.data(), hParent, LanguageDlgProc,
                              reinterpret_cast<LPARAM>(&locales));
}

} // namespace bki18n
