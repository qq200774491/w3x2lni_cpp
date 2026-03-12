# w3x2lni_cpp

w3x2lni 的 C++ 重写版本，用于 Warcraft III 地图文件（`.w3x`）与 lni 格式之间的相互转换。

## 项目目的

原 [w3x2lni](https://github.com/sumneko/w3x2lni) 使用 Lua 实现，本项目使用 C++23 重写，目标是：

- **更高性能** — 利用 C++ 原生执行速度，大幅提升地图解析与转换效率
- **更好的可维护性** — 强类型系统、编译期检查、完善的错误处理
- **跨平台支持** — Windows / Linux / macOS
- **提供 CLI 和 GUI** — 命令行工具用于自动化流程，图形界面方便日常使用

## 功能规划

- W3X 地图文件解包与打包
- SLK / W3U / W3I / JASS 等格式的解析与转换
- LNI 格式读写
- 地图分析与优化

## 项目结构

```
├── core/           # 核心库：配置、错误处理、文件系统、日志、平台抽象
├── parser/         # 解析器：slk、w3i、w3u、w3x、jass
├── cli/            # 命令行工具
├── gui/            # 图形界面
└── external/       # 外部依赖（原 w3x2lni Lua 实现，仅供参考）
```

## 技术栈

- **语言**：C++23（`std::expected`、`std::source_location` 等）
- **日志**：spdlog
- **配置**：nlohmann/json
- **风格**：Google C++ Style Guide

## 构建

TODO

## 许可证

MIT License
