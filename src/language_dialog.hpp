//  Declares the LanguageDialog class that presents the modal UI-language picker.
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

class LanguageDialog {
public:
    // 弹出模态语言选择对话框：用户确认时写注册表，取消则不做任何改动。
    static void show(HINSTANCE hInstance, HWND hParent);
};

} // namespace bki18n
