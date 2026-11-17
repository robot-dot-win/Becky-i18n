//  Provides user-mode pointer validation and an SEH memory-readability probe.
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

// Win32 地址与内存安全校验工具：所有 detour、窗口过程、钩子回调共享同一套实现，
// 避免同样的防御逻辑在多个编译单元中重复。

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace bki18n {

// 校验地址是否位于合法的用户态指针范围：
// - 下界 > 0xFFFF：排除整数、布尔值、Atom 以及 LPSTR_TEXTCALLBACK(-1) 等非指针值；
// - 上界 < 0xFFFF0000：排除内核态地址（32 位 WOW64 下用户态上限为 0xFFFF0000，
//   兼容宿主开启 /LARGEADDRESSAWARE 的 4GB 地址空间）。
//
// 注意：该函数只做范围判断，不验证内存页是否可读；
// 调用方仍须保持“先确认受管窗口/控件，再解引用外部指针”的防御顺序。
inline bool is_valid_user_mode_ptr(const void* p) noexcept {
    const auto addr = reinterpret_cast<uintptr_t>(p);
    return addr > 0xFFFFu && addr < 0xFFFF0000u;
}

inline bool is_valid_user_mode_ptr(LPARAM lParam) noexcept {
    return is_valid_user_mode_ptr(reinterpret_cast<const void*>(lParam));
}

// SEH 内存可读性探针：在解引用外部传入的结构指针前做最后一道防线。
// 纯 POD 函数（MSVC 禁止同一函数混用 __try 与需要 C++ 展开的对象），
// 逐页探测，任一页不可读即返回 false；调用方按未命中处理，绝不触发访问违例。
inline bool is_memory_readable(const void* p, std::size_t cb) noexcept {
    if (!p || cb == 0) return false;
    const auto addr = reinterpret_cast<uintptr_t>(p);
    if (addr <= 0xFFFFu || addr >= 0xFFFF0000u) return false;

    __try {
        const volatile char* probe = static_cast<const volatile char*>(p);
        for (std::size_t off = 0; off < cb; off += 4096u) {
            (void)probe[off];
        }
        (void)probe[cb - 1u];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

}  // namespace bki18n
