//  Declares the Logger singleton and the streaming log macros used across the plug-in.
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

#include <mutex>
#include <fstream>
#include <sstream>
#include <string_view>

namespace bki18n {

// 本地文件日志器（进程内单例）。
// 日志文件固定位于 %LOCALAPPDATA%\BeckyI18n\locale_i18n.log；
// 启动时若现存日志超过限额则截断重建，避免无限增长。
// 所有接口均不抛异常：日志失败静默降级，绝不影响宿主程序。
class Logger {
public:
    enum class Level {
        Debug,
        Info,
        Warn,
        Error
    };

    static Logger& instance();

    // 在 DllMain ATTACH 阶段调用：确定日志路径并打开文件
    void init() noexcept;

    // 进程卸载时关闭文件句柄
    void shutdown() noexcept;

    void write(Level level, std::string_view message) noexcept;

    static std::string_view level_tag(Level level) noexcept;

private:
    Logger() = default;

    std::mutex mutex_;
    std::ofstream file_;
    bool initialized_{false};
};

}  // namespace bki18n

// 流式日志宏，用法：BKI_LOG_INFO("reject file, size=" << n);
#define BKI_LOG(level, expr)                                                     \
    do {                                                                         \
        std::ostringstream _bki_log_oss;                                         \
        _bki_log_oss << expr;                                                    \
        ::bki18n::Logger::instance().write((level), _bki_log_oss.str());         \
    } while (0)

// DEBUG 级日志仅在调试构建（_DEBUG）下求值；Release 下展开为空操作，
// 既消除 ostringstream 开销，也避免内部细节在发行版落盘。
#ifdef _DEBUG
#define BKI_LOG_DEBUG(expr) BKI_LOG(::bki18n::Logger::Level::Debug, expr)
#else
#define BKI_LOG_DEBUG(expr) do {} while (0)
#endif
#define BKI_LOG_INFO(expr)  BKI_LOG(::bki18n::Logger::Level::Info, expr)
#define BKI_LOG_WARN(expr)  BKI_LOG(::bki18n::Logger::Level::Warn, expr)
#define BKI_LOG_ERROR(expr) BKI_LOG(::bki18n::Logger::Level::Error, expr)
