//  Implements the plug-in entry points, language menu registration, and Becky! plug-in callbacks.
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
#include <windows.h>
#include <string>

#include "BeckyApi.h"
#include "config_reg.hpp"
#include "i18n_manager.hpp"
#include "ui_translator.hpp"
#include "api_hooks.hpp"
#include "about_dialog.hpp"
#include "language_dialog.hpp"
#include "logger.hpp"
#include "version.hpp"

CBeckyAPI bka;
HINSTANCE g_hInstance = nullptr;

namespace {

// 唯一的 UI Language 入口命令 ID（只注册一次）
UINT g_languageCmdId = 0;

// 入口命令回调：打开模态语言选择对话框，选择与写注册表均在对话框内完成
void WINAPI OnSelectLanguageCommand(HWND hWnd, LPARAM /*lParam*/) {
    bki18n::LanguageDialog::show(g_hInstance, hWnd);
}

// 确保 UI Language 入口命令只在 Becky! 中注册一次
void EnsureLanguageCommandRegistered() {
    if (g_languageCmdId != 0) return;
    g_languageCmdId = bka.RegisterCommand(
        "Select the user interface language", BKC_MENU_MAIN, OnSelectLanguageCommand);
}

// 定位主菜单中的 Tools 子菜单
HMENU FindToolsSubMenu(HMENU hMainMenu) {
    if (!hMainMenu) return nullptr;
    int topCount = ::GetMenuItemCount(hMainMenu);

    // 优先通过 Tools 菜单内固定存在的 General Setup (10102) 或 Mailbox Setup (32775) 精确定位
    for (int i = 0; i < topCount; ++i) {
        HMENU hSub = ::GetSubMenu(hMainMenu, i);
        if (!hSub) continue;
        if (::GetMenuState(hSub, 10102, MF_BYCOMMAND) != static_cast<UINT>(-1) ||
            ::GetMenuState(hSub, 32775, MF_BYCOMMAND) != static_cast<UINT>(-1)) {
            return hSub;
        }
    }

    // 回退到第 5 个子菜单（索引 4）
    if (HMENU hSub = ::GetSubMenu(hMainMenu, 4)) {
        return hSub;
    }
    if (topCount > 1) {
        return ::GetSubMenu(hMainMenu, topCount - 2);
    }
    return nullptr;
}

// 在主菜单 Tools 的最后一项挂分隔线 + 单项 "UI Languag&e..."
void AttachLanguageMenuItem(HMENU hMainMenu) {
    HMENU hToolsMenu = FindToolsSubMenu(hMainMenu);
    if (!hToolsMenu) return;

    EnsureLanguageCommandRegistered();

    // 防止在同一次菜单生命周期内重复追加：检查最后一项是否已是该命令
    const int itemCount = ::GetMenuItemCount(hToolsMenu);
    if (itemCount > 0 && ::GetMenuItemID(hToolsMenu, itemCount - 1) == g_languageCmdId) {
        return;
    }

    bki18n::ScopedHookGuard guard;
    ::AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(hToolsMenu, MF_STRING, g_languageCmdId, L"UI Languag&e...");
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
// DLL 入口函数
BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            g_hInstance = hModule;
            ::DisableThreadLibraryCalls(hModule);

            // 最先初始化日志器，确保后续加载各阶段的诊断都可落盘
            bki18n::Logger::instance().init();

            if (!bka.InitAPI()) {
                BKI_LOG_ERROR("CBeckyAPI::InitAPI failed, plug-in aborted");
                return FALSE;
            }

            // BKC_OnMenuInit 会在 BKC_OnStart 之前触发，必须在加载阶段完成词典与 Hook 初始化
            bki18n::I18nManager::instance().init(hModule);
            bki18n::ApiHooks::install();
            bki18n::UiTranslator::install_window_hook(hModule);
            break;
        }
        case DLL_PROCESS_DETACH: {
            bki18n::UiTranslator::uninstall_window_hook();
            bki18n::ApiHooks::uninstall();
            bki18n::Logger::instance().shutdown();
            break;
        }
        default:
            break;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Becky! 插件导出回调实现
#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagBKPLUGININFO {
    char szPlugInName[80];
    char szVendor[80];
    char szVersion[80];
    char szDescription[256];
} BKPLUGININFO, *LPBKPLUGININFO;

int WINAPI BKC_OnPlugInInfo(LPBKPLUGININFO lpPlugInInfo) {
    try {
        if (lpPlugInInfo) {
            strncpy_s(lpPlugInInfo->szPlugInName,  "Becky! I18n", _TRUNCATE);
            strncpy_s(lpPlugInInfo->szVendor,      "Martin Young", _TRUNCATE);
            strncpy_s(lpPlugInInfo->szVersion,     bki18n::version::kVersion, _TRUNCATE);
            strncpy_s(lpPlugInInfo->szDescription, "Multi-language localization plug-in for Becky! Internet Mail.", _TRUNCATE);
        }
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnPlugInInfo");
    }
    return 0;
}

int WINAPI BKC_OnPlugInSetup(HWND hWnd) {
    try {
        bki18n::AboutDialog::show(g_hInstance, hWnd);
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnPlugInSetup");
    }
    // 返回非零值告知 Becky! 已处理 Setup 对话框，避免弹出 "This plug-in has no setup dialog"
    return 1;
}

int WINAPI BKC_OnStart() {
    try {
        HWND hMain = nullptr, hTree = nullptr, hList = nullptr, hView = nullptr;
        if (bka.GetWindowHandles(&hMain, &hTree, &hList, &hView) && hMain) {
            bki18n::UiTranslator::translate_window_tree(hMain);
        }
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnStart");
    }
    return 0;
}

int WINAPI BKC_OnExit() {
    try {
        // 窗口仍存在时显式还原树/父窗口原窗口过程，避免 DLL 卸载后出现悬挂回调
        bki18n::UiTranslator::restore_window_subclasses();
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnExit");
    }
    return 0;
}

int WINAPI BKC_OnMenuInit(HWND hWnd, HMENU hMenu, int nType) {
    try {
        // 1. 以所属窗口自己的 popup 骨架为入口翻译菜单（rev12 三窗口分桶）；
        //    窗口类无法识别时 find_window_popup_tree 返回 nullptr，translate_menu 内部全局回退。
        const auto* popupTree =
            bki18n::I18nManager::instance().find_window_popup_tree(hWnd);
        bki18n::UiTranslator::translate_menu(hMenu, popupTree);

        // 2. 如果是主菜单栏，在 Tools 最后挂分隔线与 UI Language 单项
        if (nType == BKC_MENU_MAIN) {
            AttachLanguageMenuItem(hMenu);
        }
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnMenuInit");
    }
    return 0;
}

int WINAPI BKC_OnOpenFolder(LPCSTR /*lpFolderID*/) {
    try {
        if (bki18n::I18nManager::instance().is_active()) {
            HWND hMain = nullptr, hTree = nullptr, hList = nullptr, hView = nullptr;
            if (bka.GetWindowHandles(&hMain, &hTree, &hList, &hView)) {
                if (hList) {
                    HWND hHeader = ::GetDlgItem(hList, 0);
                    if (hHeader) {
                        bki18n::UiTranslator::translate_single_control(hHeader);
                    }
                }
            }
        }
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnOpenFolder");
    }
    return 0;
}

int WINAPI BKC_OnOpenCompose(HWND hWnd, int /*nMode*/) {
    try {
        if (hWnd) {
            bki18n::UiTranslator::translate_window_tree(hWnd);
        }
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnOpenCompose");
    }
    return 0;
}

int WINAPI BKC_OnFinishRetrieve(int /*nNumber*/) {
    return 0;
}

int WINAPI BKC_OnOpenMail(LPCSTR /*lpMailID*/) {
    try {
        if (bki18n::I18nManager::instance().is_active()) {
            bki18n::ApiHooks::invalidate_mail_tab_strip();
        }
    } catch (...) {
        BKI_LOG_ERROR("unhandled exception in BKC_OnOpenMail");
    }
    return 0;
}

int WINAPI BKC_OnEveryMinute() { return 0; }
int WINAPI BKC_OnOutgoing(HWND /*hWnd*/, int /*nMode*/) { return 0; }
int WINAPI BKC_OnKeyDispatch(HWND /*hWnd*/, int /*nKey*/, int /*nShift*/) { return 0; }
int WINAPI BKC_OnRetrieve(LPCSTR /*lpMessage*/, LPCSTR /*lpMailID*/) { return 0; }
int WINAPI BKC_OnSend(LPCSTR /*lpMessage*/) { return 0; }
int WINAPI BKC_OnDragDrop(LPCSTR /*lpTgt*/, LPCSTR /*lpSrc*/, int /*nCount*/, int /*dropEffect*/) { return 0; }
int WINAPI BKC_OnBeforeFilter2(LPCSTR /*lpMessage*/, LPCSTR /*lpMailBox*/, int* /*lpnAction*/, char** /*lppParam*/) {
    return BKC_FILTER_DEFAULT;
}

#ifdef __cplusplus
}
#endif