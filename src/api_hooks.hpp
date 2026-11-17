//  Declares the ApiHooks class that manages Win32 detour hooks and the mail tab strip subclassing.
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

namespace bki18n {

class ApiHooks {
public:
    static bool install();
    static void uninstall();

    // 为邮件内容面板底部的自绘页签条 (AfxWnd, ctrl_id: 12562) 挂载/卸载子类化过程及请求重绘
    static void subclass_mail_tab_strip(HWND hWnd);
    static void restore_mail_tab_subclass();
    static void invalidate_mail_tab_strip();
};

} // namespace bki18n
