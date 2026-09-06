# SnapParse

<p align="center">
  <strong>现代化、高性能、超轻量原生的 Windows 剪贴板管理工具</strong><br>
  基于 Qt 6.8 + C++20 全新架构重构，原生 GPU 硬件加速 Acrylic/Mica 毛玻璃特效，单文件体积仅 12MB，内存占用极低，毫秒级即时响应。
</p>

<p align="center">
  <a href="https://github.com/leisure462/SnapParse/releases"><img src="https://img.shields.io/github/v/release/leisure462/SnapParse?style=flat-square&color=1677ff" alt="Latest Release"></a>
  <a href="https://github.com/leisure462/SnapParse/actions"><img src="https://img.shields.io/github/actions/workflow/status/leisure462/SnapParse/release.yml?style=flat-square&label=build" alt="Build Status"></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20x64-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/framework-Qt%206.8%20%7C%20C%2B%2B20-008080?style=flat-square" alt="Qt6 & C++20">
  <img src="https://img.shields.io/badge/size-~12%20MB-success?style=flat-square" alt="Bundle Size">
</p>

---

## ✨ 核心亮点

- **⚡ 极致轻量与性能**：摒弃传统庞大的 Webview/Electron 架构，采用 100% 原生 C++20 + Qt 6.8 渲染管线，单文件独立便携版体积仅 **~12 MB**，开机即用，常驻后台内存极致修剪。
- **💎 Windows 11/10 原生毛玻璃材质**：深度集成 DWM 原生 Acrylic（亚克力）与 Mica 特效，配合自适应深浅色主题，丝滑细腻。
- **🚀 极速唤起与防遮挡定位**：按快捷键瞬间呼出，智能计算屏幕边缘与任务栏防遮挡边界，支持光标跟随、记忆位置与屏幕居中。
- **📋 全能剪贴板捕获与历史搜索**：
  - 支持纯文本、富文本（RTF）、HTML、图片以及文件/文件夹捕获。
  - 内置基于 SQLite FTS5 全文检索引擎，百万字历史瞬间直达。
  - 智能敏感内容防护（密码/API Key 自动脱敏）与黑名单应用静默过滤。
- **🎯 快捷高效的工作流**：
  - 支持单机/双击/中键自定义粘贴与复制行为。
  - 收藏置顶、快捷标签分组、多级分类筛选与大图/文件即时空格悬浮预览。
- **📦 零依赖绿色便携**：单文件 `SnapParse.exe` 随拷随走，免安装，不写注册表残留。

---

## 📥 下载安装

前往 [GitHub Releases](https://github.com/leisure462/SnapParse/releases/latest) 下载最新版本：

| 交付格式 | 说明 |
| :--- | :--- |
| **`SnapParse.exe`** | **推荐**：单文件绿色便携版（~12MB），双击直接运行，随身 U 盘随处即用 |
| **`SnapParse-*-portable.zip`** | 完整绿色便携压缩包，解压到任意文件夹即可使用 |

---

## 🛠️ 本地构建指南

### 环境需求
- **操作系统**: Windows 10 / 11 (64-bit)
- **编译工具**: Visual Studio 2022 (MSVC v143+，包含 C++20 支持)
- **Qt 版本**: Qt 6.8.x (`win64_msvc2022_64`，包含 `qtsvg` 模块)
- **构建工具**: CMake 3.20+、Ninja

### 自动化一键构建
```powershell
# 1. 编译核心可执行程序
powershell -ExecutionPolicy Bypass -File .\src-qt\build.ps1

# 2. 生成单文件独立便携可执行程序 (SnapParse.exe)
powershell -ExecutionPolicy Bypass -File .\src-qt\build_single_exe.ps1
```

---

## 📄 开源许可证

本项目基于 MIT 许可证开源。欢迎提交 Issue 与 Pull Request！
