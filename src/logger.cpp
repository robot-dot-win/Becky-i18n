//  Implements the file logger that writes to LOCALAPPDATA and degrades silently on failure.
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
#include "logger.hpp"
#include "limits.hpp"

#include <shlobj.h>

#include <filesystem>
#include <string>
#include <cstdio>
#include <cstring>
#include <system_error>

namespace bki18n {

namespace {

// 解析 LOCALAPPDATA 根目录。SHGetFolderPath 自 Windows 2000 起可用（兼容 XP），
// 失败时按 XP 时代目录约定推导，最后退到临时目录。
bool get_local_appdata_root(std::wstring& out) noexcept {
    wchar_t path[MAX_PATH]{};

    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                     SHGFP_TYPE_CURRENT, path))) {
        out = path;
        return true;
    }

    // 兜底链 1：%USERPROFILE%\Local Settings\Application Data（XP 约定）
    wchar_t profile[MAX_PATH]{};
    const DWORD copied = ::GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    if (copied > 0 && copied < MAX_PATH) {
        out = std::wstring(profile) + L"\\Local Settings\\Application Data";
        return true;
    }

    // 兜底链 2：系统临时目录
    const DWORD tempLen = ::GetTempPathW(MAX_PATH, path);
    if (tempLen > 0 && tempLen < MAX_PATH) {
        out = path;
        return true;
    }

    return false;
}

// 递归确保目录存在；任何错误都返回 false，由调用方静默降级。
bool ensure_directory(const std::filesystem::path& dir) noexcept {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return !ec;
}

}  // namespace

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

std::string_view Logger::level_tag(Level level) noexcept {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
    }
    return "INFO";
}

void Logger::init() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return;

        std::wstring root;
        if (!get_local_appdata_root(root)) return;

        // 固定日志目录：%LOCALAPPDATA%\BeckyI18n
        const std::filesystem::path logDir =
            std::filesystem::path(root) / L"BeckyI18n";
        if (!ensure_directory(logDir)) return;

        const std::filesystem::path logPath = logDir / L"locale_i18n.log";

        bool truncate = false;
        std::error_code ec;
        if (std::filesystem::exists(logPath, ec) && !ec) {
            const auto currentSize = std::filesystem::file_size(logPath, ec);
            if (!ec && currentSize > limits::kMaxLogFileBytes) {
                truncate = true;
            }
        }

        const auto mode = std::ios::out |
                          (truncate ? std::ios::trunc : std::ios::app) |
                          std::ios::binary;
        file_.open(logPath, mode);
        if (!file_.is_open()) return;

        // 不依赖 C  locale，固定输出 UTF-8 字节；文件本身不写 BOM
        initialized_ = true;
    } catch (...) {
        // 日志器初始化失败永远不允许影响插件加载
    }
}

void Logger::shutdown() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
        initialized_ = false;
    } catch (...) {
    }
}

void Logger::write(Level level, std::string_view message) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || !file_.is_open()) return;

        SYSTEMTIME st{};
        ::GetLocalTime(&st);

        char header[64]{};
        const int headerLen = std::snprintf(
            header, sizeof(header),
            "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [%-5s] ",
            static_cast<unsigned>(st.wYear),
            static_cast<unsigned>(st.wMonth),
            static_cast<unsigned>(st.wDay),
            static_cast<unsigned>(st.wHour),
            static_cast<unsigned>(st.wMinute),
            static_cast<unsigned>(st.wSecond),
            static_cast<unsigned>(st.wMilliseconds),
            level_tag(level).data());

        if (headerLen > 0) {
            file_.write(header, headerLen);
        }
        file_.write(message.data(), static_cast<std::streamsize>(message.size()));
        file_.put('\n');
        file_.flush();
    } catch (...) {
    }
}

}  // namespace bki18n
