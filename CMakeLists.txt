#  CMakeLists.txt
#
#  Copyright (C) 2026, Martin Young <martin_young@live.cn>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the MIT License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  MIT License for more details.
#
#  You should have received a copy of the MIT License
#  along with this program. If not, see <https://opensource.org/licenses/MIT>.
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.15)

# 静态 CRT（Release /MT、Debug /MTd）：CMAKE_MSVC_RUNTIME_LIBRARY 必须在第一个
# project()/enable_language() 之前设置，才会作为本工程及 add_subdirectory 目标的默认值。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Becky_i18n VERSION 1.0.0 LANGUAGES C CXX RC)

# Becky! (B2.exe) 是 32 位程序，强制检查目标平台指针大小
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Becky! is a 32-bit (x86) application. Please configure CMake with '-A Win32'.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3 /permissive- /EHsc)
endif()

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

# ---------------------------------------------------------------------------
# 版本号单一来源
#
# project(VERSION) 提供主版本号；BECKY_VERSION_SUFFIX 承载 SemVer 预发布标识
# （不含前导连字符，如 rc.1），正式发布留空。生成的 version.hpp 与 version.rc
# 均引用此处计算结果，杜绝代码/资源中硬编码版本导致的不一致。
# ---------------------------------------------------------------------------
set(BECKY_VERSION_SUFFIX "" CACHE STRING
    "SemVer pre-release suffix without leading hyphen (e.g. rc.1); empty for a stable release")

if(BECKY_VERSION_SUFFIX STREQUAL "")
    set(BECKY_VERSION_FULL "${PROJECT_VERSION}")
elseif(BECKY_VERSION_SUFFIX MATCHES "^[0-9A-Za-z.-]+$")
    set(BECKY_VERSION_FULL "${PROJECT_VERSION}-${BECKY_VERSION_SUFFIX}")
else()
    message(FATAL_ERROR
        "BECKY_VERSION_SUFFIX may only contain [0-9A-Za-z.-], got: ${BECKY_VERSION_SUFFIX}")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp"
    @ONLY)

# 版本资源标志：调试位 VS_FF_DEBUG(0x1)、预发布位 VS_FF_PRERELEASE(0x2)
set(BKI_RC_FILEFLAGS "0x0")
if(BECKY_VERSION_SUFFIX)
    set(BKI_RC_FILEFLAGS "0x2")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc"
    @ONLY)

# ---------------------------------------------------------------------------
# 本地依赖（不再从 GitHub 拉取）
#
# 约定目录布局：依赖与本工程 (Becky-i18n) 同级放置：
#   <parent>/
#   +-- Becky-i18n/        # 本工程
#   +-- Becky-SDK/         # Becky 官方插件 SDK（rev5 起迁出，不再随插件打包）
#   +-- nlohmann/json/     # git clone https://github.com/nlohmann/json
#   +-- minhook/           # git clone https://github.com/TsudaKageyu/minhook
#
# 若仓库放在其他位置，配置时用 -D 显式指定（建议传绝对路径）：
#   -DBECKY_SDK_DIR=D:/sdk/Becky-SDK
#   -DNLOHMANN_JSON_DIR=D:/third_party/json
#   -DMINHOOK_DIR=D:/third_party/minhook
# ---------------------------------------------------------------------------

set(NLOHMANN_JSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann/json"
    CACHE PATH "Path to the locally cloned nlohmann/json repository (the directory containing include/nlohmann/json.hpp)")
set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../minhook"
    CACHE PATH "Path to the locally cloned MinHook repository (the directory containing CMakeLists.txt and include/MinHook.h)")

# 相对路径统一以本工程源码目录为基准转成绝对路径，避免依赖配置时的工作目录
if(NOT IS_ABSOLUTE "${NLOHMANN_JSON_DIR}")
    get_filename_component(NLOHMANN_JSON_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/${NLOHMANN_JSON_DIR}" ABSOLUTE)
endif()
if(NOT IS_ABSOLUTE "${MINHOOK_DIR}")
    get_filename_component(MINHOOK_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/${MINHOOK_DIR}" ABSOLUTE)
endif()

# 1. nlohmann/json：header-only，直接建 INTERFACE 导入目标，不执行其自带
#    CMake（避免拉入测试、安装与编译器探测等无关逻辑）
if(NOT EXISTS "${NLOHMANN_JSON_DIR}/include/nlohmann/json.hpp")
    message(FATAL_ERROR
        "nlohmann/json was not found at:\n"
        "    ${NLOHMANN_JSON_DIR}\n"
        "Expected header: ${NLOHMANN_JSON_DIR}/include/nlohmann/json.hpp\n"
        "Clone https://github.com/nlohmann/json.git there, or pass:\n"
        "    -DNLOHMANN_JSON_DIR=<path-to-json-repo>")
endif()

add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json INTERFACE "${NLOHMANN_JSON_DIR}/include")
target_compile_features(nlohmann_json INTERFACE cxx_std_17)

# 2. MinHook：纳入其自带 CMake 工程（静态库 minhook，PUBLIC 暴露 include/）
if(NOT EXISTS "${MINHOOK_DIR}/CMakeLists.txt"
   OR NOT EXISTS "${MINHOOK_DIR}/include/MinHook.h")
    message(FATAL_ERROR
        "MinHook was not found at:\n"
        "    ${MINHOOK_DIR}\n"
        "Expected files under that directory: CMakeLists.txt and include/MinHook.h\n"
        "Clone https://github.com/TsudaKageyu/minhook.git there, or pass:\n"
        "    -DMINHOOK_DIR=<path-to-minhook-repo>")
endif()

add_subdirectory("${MINHOOK_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/minhook" EXCLUDE_FROM_ALL)

# 3. Becky 官方插件 SDK：rev5 起移到与本工程同级的 Becky-SDK 目录，不再随插件打包
set(BECKY_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Becky-SDK"
    CACHE PATH "Path to the official Becky! plug-in SDK (the directory containing BeckyAPI.cpp and BeckyApi.h)")

# 相对路径统一以本工程源码目录为基准转成绝对路径
if(NOT IS_ABSOLUTE "${BECKY_SDK_DIR}")
    get_filename_component(BECKY_SDK_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/${BECKY_SDK_DIR}" ABSOLUTE)
endif()

if(NOT EXISTS "${BECKY_SDK_DIR}/BeckyAPI.cpp"
   OR NOT EXISTS "${BECKY_SDK_DIR}/BeckyApi.h")
    message(FATAL_ERROR
        "Becky! plug-in SDK was not found at:\n"
        "    ${BECKY_SDK_DIR}\n"
        "Expected files under that directory: BeckyAPI.cpp and BeckyApi.h\n"
        "Move the official SDK there (renamed to Becky-SDK), or pass:\n"
        "    -DBECKY_SDK_DIR=<path-to-sdk>")
endif()

# 4. 构建 locale.dll 插件目标
add_library(locale SHARED
    "${BECKY_SDK_DIR}/BeckyAPI.cpp"
    "src/logger.cpp"
    "src/i18n_manager.cpp"
    "src/ui_translator.cpp"
    "src/api_hooks.cpp"
    "src/about_dialog.cpp"
    "src/language_dialog.cpp"
    "src/dllmain.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc"
    "locale.def"
)

set_target_properties(locale PROPERTIES
    OUTPUT_NAME "locale"
    PREFIX ""
)

# minhook 的 include 目录由其 target PUBLIC 传递，无需在此重复列出
target_include_directories(locale PRIVATE
    "${BECKY_SDK_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(locale PRIVATE
    nlohmann_json::nlohmann_json
    minhook
    user32
    gdi32
    comctl32
    advapi32
    shell32
)
