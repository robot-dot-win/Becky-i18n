//  Provides inline helpers to read and write the selected locale in the registry and to locate the locale directory.
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
#include <string>
#include <string_view>
#include <filesystem>

namespace bki18n {

constexpr const wchar_t* REG_BECKY_SETTINGS = L"Software\\RimArts\\B2\\Settings";

// 读取注册表中的当前语言标识（默认返回 "en_US"）
inline std::string read_reg_locale() noexcept {
    HKEY hKey = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, REG_BECKY_SETTINGS, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "en_US";
    }
    char buf[128]{};
    DWORD dwSize = sizeof(buf) - 1;
    DWORD dwType = REG_SZ;
    LSTATUS status = ::RegQueryValueExA(hKey, "Locale", nullptr, &dwType, reinterpret_cast<LPBYTE>(buf), &dwSize);
    ::RegCloseKey(hKey);

    if (status == ERROR_SUCCESS && buf[0] != '\0') {
        return std::string(buf);
    }
    return "en_US";
}

// 将选中的语言标识写入注册表
inline bool write_reg_locale(std::string_view locale_id) noexcept {
    HKEY hKey = nullptr;
    DWORD dwDisp = 0;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, REG_BECKY_SETTINGS, 0, nullptr,
                          REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, &dwDisp) != ERROR_SUCCESS) {
        return false;
    }
    std::string val(locale_id);
    LSTATUS status = ::RegSetValueExA(
        hKey, "Locale", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(val.c_str()),
        static_cast<DWORD>(val.size() + 1)
    );
    ::RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

// 获取 PlugIns\locale\ 目录路径
inline std::filesystem::path get_locale_dir(HINSTANCE hDll) {
    wchar_t dllPath[MAX_PATH]{};
    ::GetModuleFileNameW(hDll, dllPath, MAX_PATH);
    std::filesystem::path p(dllPath);
    return p.parent_path() / L"locale";
}

} // namespace bki18n