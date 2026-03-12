# w3x2lni_cpp
`w3x2lni_cpp` 是一个面向 Warcraft III 地图工具链的纯 C++ 重写工程。

## 项目目的

原 [w3x2lni](https://github.com/sumneko/w3x2lni) 使用 Lua 实现，本项目使用 C++23 重写，目标是：


当前仓库的首版目标已经收敛为：

- 纯 C++ 实现，不依赖 `lua54`、`luamake` 或 Lua 运行时
- 以 `external/` 原项目为行为参考，不直接复用其 Lua 构建链
- 首版只承诺 `Windows x64 + MSVC + CMake`
- 首版交付 `CLI`，`GUI` 默认关闭
- 当前可用输入为“已解包地图目录”，打包 `.w3x/.w3m` MPQ 支持仍在后续阶段

## 当前能力

- `convert <input_map_dir> <output_dir>`
  - 将已解包地图目录转换为 LNI 工作区布局
  - 输出 `map/`、`table/`、`trigger/` 和 `w3x2lni.ini`
- `extract <input_map_dir> <output_dir>`
  - 按资源类型提取目录中的模型、贴图、声音、脚本、UI、数据或地图文件
- `analyze <input_map_dir>`
  - 统计目录资源分类
  - 若存在 `war3map.w3i`，输出地图元数据摘要

## 非目标

- 首版不交付 GUI 功能
- 首版不做 `JASS -> Lua` 转换
- 首版不支持直接读取打包 `.w3x/.w3m` MPQ 文件
- 首版不承诺 Linux/macOS 发布

## 目录结构

```text
app/          CLI 入口
cli/          命令实现
converter/    LNI 转换与资源提取
core/         日志、文件系统、错误、平台能力
parser/       Warcraft III 格式解析器
tests/        目录模式 smoke tests
external/     原 Lua 项目参考资料，不参与新构建
```

## 构建

要求：

- Visual Studio 2022
- CMake 3.20+
- C++23 编译器

Windows x64 示例：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build --build-config Release
```

默认情况下：

- `W3X_BUILD_GUI=OFF`
- `W3X_BUILD_TESTS=ON`

如需显式关闭 GUI：

```powershell
cmake -S . -B build -A x64 -DW3X_BUILD_GUI=OFF
```

## 参考来源

- 原始工具链参考：`external/`
- 该目录仅用于对照功能与行为，不作为新工程的运行时依赖

## 许可证

MIT
